#include <genomes/proc/ProceduralRuntime.hpp>

namespace genomes::proc {

GenerationChannel::GenerationChannel()
    : state_(std::make_shared<detail::GenerationChannelState>()) {}

jobs::CancelSource GenerationChannel::begin(std::uint64_t request_id) {
    std::lock_guard lock(state_->mutex);
    if (state_->active) {
        state_->active->cancel();
    }
    state_->latest_request = request_id;
    state_->active.emplace();
    return *state_->active;
}

bool GenerationChannel::isCurrent(std::uint64_t request_id) const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->latest_request == request_id;
}

std::uint64_t GenerationChannel::latestRequest() const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->latest_request;
}

void GenerationDiagnostics::record(GenerationDiagnostic diagnostic) {
    std::lock_guard lock(mutex_);
    diagnostics_.push_back(diagnostic);
}

std::vector<GenerationDiagnostic> GenerationDiagnostics::snapshot() const {
    std::lock_guard lock(mutex_);
    return diagnostics_;
}

ProceduralRuntime::ProceduralRuntime(const GeneratorRegistry& registry,
                                     jobs::JobSystem& jobs,
                                     ArtifactCache* cache)
    : registry_(registry), cache_(cache == nullptr ? &owned_cache_ : cache),
      group_(jobs) {}

ProceduralRuntime::~ProceduralRuntime() {
    group_.wait();
}

ArtifactKey ProceduralRuntime::makeKey(const GeneratorEntry& entry,
                                       SeedPath seed_path,
                                       const GenerationOptions& options) noexcept {
    const GeneratorVersion version = entry.descriptor.version;
    const std::uint32_t packed_version = (static_cast<std::uint32_t>(version.major) << 20U) |
                                         (static_cast<std::uint32_t>(version.minor) << 10U) |
                                         static_cast<std::uint32_t>(version.patch);
    return {entry.descriptor.id.value(), packed_version, seed_path.seed(),
            options.input_hash, options.schema_version, SeedDerivationVersion,
            options.dependency_hash};
}

ProceduralRuntimeTelemetry ProceduralRuntime::telemetry() const noexcept {
    return {requested_.load(std::memory_order_relaxed),
            completed_.load(std::memory_order_relaxed),
            failed_.load(std::memory_order_relaxed),
            canceled_.load(std::memory_order_relaxed),
            superseded_.load(std::memory_order_relaxed),
            cache_hits_.load(std::memory_order_relaxed)};
}

} // namespace genomes::proc
