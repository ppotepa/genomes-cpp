#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <typeindex>
#include <utility>
#include <vector>

namespace genomes::proc {

enum class GenerationStatus : std::uint8_t {
    Pending,
    Running,
    Completed,
    Failed,
    Canceled,
    Superseded,
};

struct GenerationOptions final {
    foundation::StableId input_hash{0};
    std::uint32_t schema_version{1};
    foundation::StableId dependency_hash{0};
    std::size_t retained_bytes{0};
    bool use_cache{true};
    jobs::JobPriority priority{jobs::JobPriority::Normal};
    jobs::CancelToken cancellation{};
};

template <class Input, class Output>
struct GenerationRequest final {
    GeneratorId generator{};
    std::shared_ptr<const Input> input;
    SeedPath seed_path{};
    GenerationOptions options{};
};

namespace detail {

template <class T>
struct GenerationTicketState final {
    void transition(GenerationStatus next,
                    std::shared_ptr<const T> value = {},
                    foundation::Error failure = {}) noexcept {
        {
            std::lock_guard lock(mutex);
            status = next;
            artifact = std::move(value);
            error = failure;
        }
        condition.notify_all();
    }

    mutable std::mutex mutex;
    mutable std::condition_variable condition;
    GenerationStatus status{GenerationStatus::Pending};
    std::shared_ptr<const T> artifact;
    foundation::Error error{};
    jobs::CancelSource cancellation;
    std::uint64_t request_id{0};
    // The ticket keeps the scheduler that owns its job so a wait issued from
    // a worker can help execute queued work instead of blocking the worker
    // pool (important for composed world-generation stages).
    jobs::JobSystem* scheduler{nullptr};
    std::optional<jobs::JobHandle> job;
};

struct GenerationChannelState final {
    mutable std::mutex mutex;
    std::uint64_t latest_request{0};
    std::optional<jobs::CancelSource> active;
};

} // namespace detail

template <class T>
class GenerationTicket final {
public:
    GenerationTicket() = default;

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state_); }
    [[nodiscard]] GenerationStatus status() const noexcept {
        if (!state_) {
            return GenerationStatus::Failed;
        }
        std::lock_guard lock(state_->mutex);
        return state_->status;
    }
    [[nodiscard]] bool complete() const noexcept {
        const auto current = status();
        return current == GenerationStatus::Completed || current == GenerationStatus::Failed ||
               current == GenerationStatus::Canceled || current == GenerationStatus::Superseded;
    }
    void wait() const noexcept {
        if (!state_) {
            return;
        }
        jobs::JobSystem* scheduler = nullptr;
        std::optional<jobs::JobHandle> job;
        {
            std::lock_guard lock(state_->mutex);
            scheduler = state_->scheduler;
            job = state_->job;
        }
        if (scheduler != nullptr && job.has_value()) {
            scheduler->wait(*job);
            return;
        }
        std::unique_lock lock(state_->mutex);
        state_->condition.wait(lock, [this] {
            return state_->status == GenerationStatus::Completed ||
                   state_->status == GenerationStatus::Failed ||
                   state_->status == GenerationStatus::Canceled ||
                   state_->status == GenerationStatus::Superseded;
        });
    }
    void cancel() noexcept {
        if (state_) {
            state_->cancellation.cancel();
        }
    }
    [[nodiscard]] std::shared_ptr<const T> artifact() const noexcept {
        if (!state_) {
            return {};
        }
        std::lock_guard lock(state_->mutex);
        return state_->artifact;
    }
    [[nodiscard]] foundation::Error error() const noexcept {
        if (!state_) {
            return {foundation::ErrorCode::InvalidState, "invalid generation ticket"};
        }
        std::lock_guard lock(state_->mutex);
        return state_->error;
    }
    [[nodiscard]] std::uint64_t requestId() const noexcept {
        return state_ ? state_->request_id : 0;
    }

private:
    friend class ProceduralRuntime;
    explicit GenerationTicket(std::shared_ptr<detail::GenerationTicketState<T>> state)
        : state_(std::move(state)) {}

    std::shared_ptr<detail::GenerationTicketState<T>> state_;
};

class GenerationChannel final {
public:
    GenerationChannel();

    [[nodiscard]] std::uint64_t latestRequest() const noexcept;

private:
    friend class ProceduralRuntime;
    [[nodiscard]] jobs::CancelSource begin(std::uint64_t request_id);
    [[nodiscard]] bool isCurrent(std::uint64_t request_id) const noexcept;

    std::shared_ptr<detail::GenerationChannelState> state_;
};

class ArtifactReader final {
public:
    explicit ArtifactReader(const ArtifactCache& cache) noexcept : cache_(cache) {}

    template <class T>
    [[nodiscard]] std::shared_ptr<const T> find(const ArtifactKey& key) const {
        return cache_.find<T>(key);
    }

private:
    const ArtifactCache& cache_;
};

struct GenerationDiagnostic final {
    std::uint64_t request_id{0};
    GeneratorId generator{};
    foundation::Error error{};
};

class GenerationDiagnostics final {
public:
    void record(GenerationDiagnostic diagnostic);
    [[nodiscard]] std::vector<GenerationDiagnostic> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::vector<GenerationDiagnostic> diagnostics_;
};

struct ProceduralRuntimeTelemetry final {
    std::uint64_t requested{0};
    std::uint64_t running{0};
    std::uint64_t completed{0};
    std::uint64_t failed{0};
    std::uint64_t canceled{0};
    std::uint64_t superseded{0};
    std::uint64_t cache_hits{0};
    std::uint64_t cache_misses{0};
};

class GenerationPipeline final {
public:
    using StageExecutor = std::function<foundation::Result<void, foundation::Error>(
        GeneratorId, GenerationContext&)>;

    void addStage(GeneratorId generator) { stages_.push_back(generator); }
    [[nodiscard]] const std::vector<GeneratorId>& stages() const noexcept { return stages_; }
    [[nodiscard]] bool empty() const noexcept { return stages_.empty(); }

    [[nodiscard]] foundation::Result<void, foundation::Error> validate(
        const GeneratorRegistry& registry) const {
        for (const GeneratorId generator : stages_) {
            if (generator.value() == 0U || registry.find(generator) == nullptr) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::NotFound, "procedural pipeline stage is not registered"});
            }
        }
        return foundation::Result<void, foundation::Error>::success();
    }

    [[nodiscard]] foundation::Result<void, foundation::Error> execute(
        GenerationContext& context, const StageExecutor& executor) const {
        if (!executor) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "procedural pipeline executor is null"});
        }
        for (const GeneratorId generator : stages_) {
            if (context.cancellationRequested()) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "procedural pipeline canceled"});
            }
            try {
                auto result = executor(generator, context);
                if (!result) return result;
            } catch (...) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::Internal, "procedural pipeline stage threw"});
            }
        }
        return foundation::Result<void, foundation::Error>::success();
    }

private:
    std::vector<GeneratorId> stages_;
};

class ProceduralRuntime final {
public:
    ProceduralRuntime(const GeneratorRegistry& registry,
                      jobs::JobSystem& jobs,
                      ArtifactCache* cache = nullptr);
    ~ProceduralRuntime();

    ProceduralRuntime(const ProceduralRuntime&) = delete;
    ProceduralRuntime& operator=(const ProceduralRuntime&) = delete;

    template <class Input, class Output>
    [[nodiscard]] GenerationTicket<Output> request(
        GenerationRequest<Input, Output> request,
        GenerationChannel* channel = nullptr) {
        auto state = std::make_shared<detail::GenerationTicketState<Output>>();
        state->scheduler = &jobs_;
        state->request_id = next_request_.fetch_add(1, std::memory_order_relaxed);
        GenerationTicket<Output> ticket(state);
        requested_.fetch_add(1, std::memory_order_relaxed);

        const GeneratorEntry* entry = registry_.find(request.generator);
        if (entry == nullptr || !entry->generate_typed ||
            entry->input_cpp_type != std::type_index(typeid(Input)) ||
            entry->output_cpp_type != std::type_index(typeid(Output))) {
            const foundation::Error error{foundation::ErrorCode::InvalidArgument,
                                          "procedural generator type mismatch"};
            state->transition(GenerationStatus::Failed, {}, error);
            diagnostics_.record({state->request_id, request.generator, error});
            failed_.fetch_add(1, std::memory_order_relaxed);
            return ticket;
        }
        if (!request.input) {
            const foundation::Error error{foundation::ErrorCode::InvalidArgument,
                                          "procedural generation input is null"};
            state->transition(GenerationStatus::Failed, {}, error);
            diagnostics_.record({state->request_id, request.generator, error});
            failed_.fetch_add(1, std::memory_order_relaxed);
            return ticket;
        }

        std::shared_ptr<detail::GenerationChannelState> channel_state;
        jobs::CancelToken superseded;
        std::optional<jobs::CancelSource> superseded_source;
        if (channel != nullptr) {
            superseded_source.emplace(channel->begin(state->request_id));
            superseded = superseded_source->token();
            channel_state = channel->state_;
        }
        const ArtifactKey key = makeKey(*entry, request.seed_path, request.options);
        jobs::JobOptions job_options;
        job_options.priority = request.options.priority;
        job_options.work_class = jobs::WorkClass::Procedural;
        auto handle = group_.submit(
            [this, entry, request = std::move(request), state, channel_state,
             superseded, superseded_source = std::move(superseded_source),
             key](jobs::JobContext& job) mutable {
                // Keep the channel's source alive for the duration of this
                // request.  CancelToken is intentionally non-owning; without
                // this retention a superseded source could be destroyed
                // before a cooperative generator observes its cancellation.
                (void)superseded_source;
                state->transition(GenerationStatus::Running);
                try {
                const auto is_current = [&] {
                    if (!channel_state) {
                        return true;
                    }
                    std::lock_guard lock(channel_state->mutex);
                    return channel_state->latest_request == state->request_id;
                };
                const bool canceled_before = state->cancellation.isCancellationRequested() ||
                                             request.options.cancellation.isCancellationRequested();
                if (canceled_before || !is_current()) {
                    settleCancellation(state, is_current());
                    return;
                }
                const bool cacheable = request.options.use_cache &&
                                       entry->descriptor.cache == GeneratorCachePolicy::Artifact;
                if (cacheable) {
                    if (auto cached = cache_->find<Output>(key)) {
                        if (is_current() && !state->cancellation.isCancellationRequested() &&
                            !request.options.cancellation.isCancellationRequested()) {
                            state->transition(GenerationStatus::Completed, std::move(cached));
                            cache_hits_.fetch_add(1, std::memory_order_relaxed);
                            completed_.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            settleCancellation(state, is_current());
                        }
                        return;
                    }
                    cache_misses_.fetch_add(1, std::memory_order_relaxed);
                }

                ArtifactReader reader(*cache_);
                GenerationContext context(request.seed_path, &job,
                                          state->cancellation.token(), superseded,
                                          &reader, &diagnostics_);
                auto generated = entry->generate_typed(request.input.get(), context);
                if (context.cancellationRequested() || !is_current()) {
                    settleCancellation(state, is_current());
                    return;
                }
                if (!generated) {
                    state->transition(GenerationStatus::Failed, {}, generated.error());
                    diagnostics_.record(
                        {state->request_id, request.generator, generated.error()});
                    failed_.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                auto output = std::static_pointer_cast<const Output>(generated.value());
                if (!output) {
                    const foundation::Error error{foundation::ErrorCode::Internal,
                                                  "procedural generator returned null"};
                    state->transition(GenerationStatus::Failed, {}, error);
                    diagnostics_.record({state->request_id, request.generator, error});
                    failed_.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                // Supersession and publication share the channel lock. This
                // closes the race where a completed generator could otherwise
                // store or publish an artifact after a newer request became
                // current.
                if (channel_state) {
                    std::lock_guard channel_lock(channel_state->mutex);
                    if (channel_state->latest_request != state->request_id ||
                        state->cancellation.isCancellationRequested() ||
                        request.options.cancellation.isCancellationRequested()) {
                        settleCancellation(
                            state, channel_state->latest_request == state->request_id);
                        return;
                    }
                    if (cacheable) {
                        cache_->store(key, output, {request.options.retained_bytes});
                    }
                    state->transition(GenerationStatus::Completed, std::move(output));
                } else {
                    if (state->cancellation.isCancellationRequested() ||
                        request.options.cancellation.isCancellationRequested()) {
                        settleCancellation(state, true);
                        return;
                    }
                    if (cacheable) {
                        cache_->store(key, output, {request.options.retained_bytes});
                    }
                    state->transition(GenerationStatus::Completed, std::move(output));
                }
                completed_.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    const foundation::Error error{foundation::ErrorCode::Internal,
                                                  "procedural generator threw"};
                    state->transition(GenerationStatus::Failed, {}, error);
                    diagnostics_.record({state->request_id, request.generator, error});
                    failed_.fetch_add(1, std::memory_order_relaxed);
                }
            },
            job_options);
        {
            std::lock_guard lock(state->mutex);
            state->job = handle;
        }
        if (handle.wasCanceled()) {
            state->transition(GenerationStatus::Canceled);
            canceled_.fetch_add(1, std::memory_order_relaxed);
        }
        return ticket;
    }

    // Composition-owned stages that produce a resolved artifact from several
    // registered generator outputs use the same ticket lifecycle as a single
    // registered generator.  The stage remains identified and scheduled by
    // this runtime; the callback is deliberately not exposed to scenes.
    template <class Output>
    [[nodiscard]] GenerationTicket<Output> requestStage(
        GeneratorId stage,
        SeedPath seed_path,
        GenerationOptions options,
        std::function<foundation::Result<std::shared_ptr<const Output>, foundation::Error>(
            GenerationContext&)> execute,
        GenerationChannel* channel = nullptr) {
        const GeneratorEntry* entry = registry_.find(stage);
        if (entry == nullptr || !entry->descriptor.valid()) {
            auto state = std::make_shared<detail::GenerationTicketState<Output>>();
            state->request_id = next_request_.fetch_add(1, std::memory_order_relaxed);
            state->transition(GenerationStatus::Failed, {},
                              {foundation::ErrorCode::NotFound,
                               "procedural stage is not registered"});
            failed_.fetch_add(1, std::memory_order_relaxed);
            requested_.fetch_add(1, std::memory_order_relaxed);
            return GenerationTicket<Output>(std::move(state));
        }
        auto state = std::make_shared<detail::GenerationTicketState<Output>>();
        state->scheduler = &jobs_;
        state->request_id = next_request_.fetch_add(1, std::memory_order_relaxed);
        GenerationTicket<Output> ticket(state);
        requested_.fetch_add(1, std::memory_order_relaxed);
        std::shared_ptr<detail::GenerationChannelState> channel_state;
        jobs::CancelToken superseded;
        std::optional<jobs::CancelSource> superseded_source;
        if (channel != nullptr) {
            superseded_source.emplace(channel->begin(state->request_id));
            superseded = superseded_source->token();
            channel_state = channel->state_;
        }
        auto handle = group_.submit(
            [this, stage, seed_path, options, execute = std::move(execute), state,
             channel_state, superseded,
             superseded_source = std::move(superseded_source)](jobs::JobContext& job) mutable {
                (void)superseded_source;
                state->transition(GenerationStatus::Running);
                const auto current = [&] {
                    if (!channel_state) return true;
                    std::lock_guard lock(channel_state->mutex);
                    return channel_state->latest_request == state->request_id;
                };
                if (state->cancellation.isCancellationRequested() ||
                    options.cancellation.isCancellationRequested() || !current()) {
                    settleCancellation(state, current());
                    return;
                }
                try {
                    ArtifactReader reader(*cache_);
                    GenerationContext context(seed_path, &job, state->cancellation.token(),
                                              superseded, &reader, &diagnostics_);
                    auto result = execute(context);
                    if (context.cancellationRequested() || !current()) {
                        settleCancellation(state, current());
                        return;
                    }
                    if (!result || !result.value()) {
                        const auto error = result ? foundation::Error{
                                                          foundation::ErrorCode::Internal,
                                                          "procedural stage returned null"}
                                                  : result.error();
                        state->transition(GenerationStatus::Failed, {}, error);
                        diagnostics_.record({state->request_id, stage, error});
                        failed_.fetch_add(1, std::memory_order_relaxed);
                        return;
                    }
                    if (!current() || state->cancellation.isCancellationRequested() ||
                        options.cancellation.isCancellationRequested()) {
                        settleCancellation(state, current());
                        return;
                    }
                    state->transition(GenerationStatus::Completed, std::move(result.value()));
                    completed_.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    const foundation::Error error{foundation::ErrorCode::Internal,
                                                  "procedural stage threw"};
                    state->transition(GenerationStatus::Failed, {}, error);
                    diagnostics_.record({state->request_id, stage, error});
                    failed_.fetch_add(1, std::memory_order_relaxed);
                }
            },
            jobs::JobOptions{jobs::ExecutionLane::Worker, jobs::WorkClass::Procedural,
                             options.priority, options.cancellation});
        {
            std::lock_guard lock(state->mutex);
            state->job = handle;
        }
        if (handle.wasCanceled()) {
            state->transition(GenerationStatus::Canceled);
            canceled_.fetch_add(1, std::memory_order_relaxed);
        }
        return ticket;
    }

    template <class Input, class Output>
    [[nodiscard]] foundation::Result<std::shared_ptr<const Output>, foundation::Error>
    generateInline(const GenerationRequest<Input, Output>& request) {
        requested_.fetch_add(1, std::memory_order_relaxed);
        if (request.options.cancellation.isCancellationRequested()) {
            canceled_.fetch_add(1, std::memory_order_relaxed);
            return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "procedural generation canceled"});
        }
        const GeneratorEntry* entry = registry_.find(request.generator);
        if (entry == nullptr || !entry->generate_typed ||
            entry->input_cpp_type != std::type_index(typeid(Input)) ||
            entry->output_cpp_type != std::type_index(typeid(Output)) || !request.input) {
            failed_.fetch_add(1, std::memory_order_relaxed);
            return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "procedural generator type mismatch"});
        }
        const ArtifactKey key = makeKey(*entry, request.seed_path, request.options);
        const bool cacheable = request.options.use_cache &&
                               entry->descriptor.cache == GeneratorCachePolicy::Artifact;
        if (cacheable) {
            if (auto cached = cache_->find<Output>(key)) {
                cache_hits_.fetch_add(1, std::memory_order_relaxed);
                completed_.fetch_add(1, std::memory_order_relaxed);
                return foundation::Result<std::shared_ptr<const Output>,
                                          foundation::Error>::success(std::move(cached));
            }
        }
        cache_misses_.fetch_add(cacheable ? 1U : 0U, std::memory_order_relaxed);
        jobs::ScratchContext scratch;
        ArtifactReader reader(*cache_);
        GenerationContext context(request.seed_path, nullptr, request.options.cancellation,
                                  {}, &reader, &diagnostics_, &scratch);
        const auto generated = [&]()
            -> foundation::Result<std::shared_ptr<const void>, foundation::Error> {
            try {
                return entry->generate_typed(request.input.get(), context);
            } catch (...) {
                const foundation::Error error{foundation::ErrorCode::Internal,
                                              "procedural generator threw"};
                diagnostics_.record({0, request.generator, error});
                return foundation::Result<std::shared_ptr<const void>, foundation::Error>::failure(
                    error);
            }
        }();
        if (request.options.cancellation.isCancellationRequested() ||
            context.cancellationRequested()) {
            canceled_.fetch_add(1, std::memory_order_relaxed);
            return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "procedural generation canceled"});
        }
        if (!generated) {
            failed_.fetch_add(1, std::memory_order_relaxed);
            return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::failure(
                generated.error());
        }
        auto output = std::static_pointer_cast<const Output>(generated.value());
        if (!output) {
            const foundation::Error error{foundation::ErrorCode::Internal,
                                          "procedural generator returned null"};
            diagnostics_.record({0, request.generator, error});
            failed_.fetch_add(1, std::memory_order_relaxed);
            return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::failure(
                error);
        }
        if (cacheable) {
            cache_->store(key, output, {request.options.retained_bytes});
        }
        completed_.fetch_add(1, std::memory_order_relaxed);
        return foundation::Result<std::shared_ptr<const Output>, foundation::Error>::success(
            std::move(output));
    }

    [[nodiscard]] ProceduralRuntimeTelemetry telemetry() const noexcept;
    [[nodiscard]] GenerationDiagnostics& diagnostics() noexcept { return diagnostics_; }
    [[nodiscard]] const GeneratorRegistry& registry() const noexcept { return registry_; }

private:
    [[nodiscard]] static ArtifactKey makeKey(const GeneratorEntry&,
                                             SeedPath,
                                             const GenerationOptions&) noexcept;

    template <class Output>
    void settleCancellation(
        const std::shared_ptr<detail::GenerationTicketState<Output>>& state,
        bool current) noexcept {
        if (current) {
            state->transition(GenerationStatus::Canceled);
            canceled_.fetch_add(1, std::memory_order_relaxed);
        } else {
            state->transition(GenerationStatus::Superseded);
            superseded_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    const GeneratorRegistry& registry_;
    ArtifactCache owned_cache_;
    ArtifactCache* cache_{nullptr};
    GenerationDiagnostics diagnostics_;
    std::atomic_uint64_t next_request_{1};
    std::atomic_uint64_t requested_{0};
    std::atomic_uint64_t completed_{0};
    std::atomic_uint64_t failed_{0};
    std::atomic_uint64_t canceled_{0};
    std::atomic_uint64_t superseded_{0};
    std::atomic_uint64_t cache_hits_{0};
    std::atomic_uint64_t cache_misses_{0};
    jobs::JobSystem& jobs_;
    jobs::JobGroup group_;
};

} // namespace genomes::proc
