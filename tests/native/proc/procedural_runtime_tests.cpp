#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <latch>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>

namespace {

using RuntimeResult = genomes::foundation::Result<std::shared_ptr<const int>,
                                                   genomes::foundation::Error>;

genomes::proc::GeneratorDescriptor descriptor(std::uint16_t major = 1) {
    return {genomes::proc::generatorId("test.integer"),
            "test.integer",
            {major, 0, 0},
            genomes::foundation::stable_id("test.integer.input"),
            genomes::foundation::stable_id("test.integer.output"),
            true,
            genomes::proc::GeneratorExecutionPolicy::Cpu,
            genomes::proc::GeneratorCachePolicy::Artifact};
}

} // namespace

int main() {
    using namespace genomes;
    std::atomic_uint32_t calls{0};
    std::atomic_bool superseded_observed{false};
    std::latch first_started{1};
    proc::GeneratorRegistry::Builder builder;
    assert((builder.addTyped<int, int>(
        descriptor(),
        [&calls, &superseded_observed, &first_started](const int& input,
                                                 proc::GenerationContext& context) -> RuntimeResult {
            calls.fetch_add(1, std::memory_order_relaxed);
            if (input == 1) {
                first_started.count_down();
                while (!context.cancellationRequested()) {
                    std::this_thread::yield();
                }
                superseded_observed.store(true, std::memory_order_release);
            }
            if (input == 4) {
                return RuntimeResult::failure(
                    {foundation::ErrorCode::Internal, "expected generation failure"});
            }
            if (input == 5) {
                while (!context.cancellationRequested()) {
                    std::this_thread::yield();
                }
            }
            if (input == 6) {
                throw std::runtime_error("expected generator exception");
            }
            return RuntimeResult::success(std::make_shared<const int>(input * 3));
        },
        [](const int& input) {
            return foundation::stableHashU64(static_cast<std::uint64_t>(input));
        })));
    auto frozen = std::move(builder).freeze();
    assert(frozen);

    proc::GenerationPipeline pipeline;
    pipeline.addStage(proc::generatorId("test.integer"));
    assert(pipeline.validate(frozen.value()));
    jobs::CancelSource pipeline_cancel;
    jobs::ScratchContext pipeline_scratch;
    proc::GenerationContext pipeline_context(proc::SeedPath(11), nullptr,
                                              pipeline_cancel.token(), {}, nullptr, nullptr,
                                              &pipeline_scratch);
    std::uint32_t pipeline_calls = 0U;
    assert(pipeline.execute(
        pipeline_context,
        [&pipeline_calls](proc::GeneratorId, proc::GenerationContext&) {
            ++pipeline_calls;
            return foundation::Result<void, foundation::Error>::success();
        }));
    assert(pipeline_calls == 1U);
    proc::GenerationPipeline failing_pipeline;
    failing_pipeline.addStage(proc::generatorId("test.integer"));
    failing_pipeline.addStage(proc::generatorId("test.integer"));
    std::uint32_t failing_calls = 0U;
    const auto pipeline_failure = failing_pipeline.execute(
        pipeline_context,
        [&failing_calls](proc::GeneratorId, proc::GenerationContext&) {
            ++failing_calls;
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "expected pipeline stage failure"});
        });
    assert(!pipeline_failure);
    assert(failing_calls == 1U);
    pipeline_cancel.cancel();
    assert(!pipeline.execute(
        pipeline_context,
        [](proc::GeneratorId, proc::GenerationContext&) {
            return foundation::Result<void, foundation::Error>::success();
        }));

    jobs::JobSystem jobs(2);
    proc::ArtifactCache cache;
    proc::ProceduralRuntime runtime(frozen.value(), jobs, &cache);
    proc::GenerationOptions stage_options{};
    auto unknown_stage = runtime.requestStage<int>(
        proc::generatorId("test.unregistered-stage"), proc::SeedPath(91), stage_options,
        [](proc::GenerationContext&) -> RuntimeResult {
            return RuntimeResult::success(std::make_shared<const int>(1));
        });
    assert(unknown_stage.status() == proc::GenerationStatus::Failed);
    assert(unknown_stage.error().code == foundation::ErrorCode::NotFound);

    auto typed_as_stage = runtime.requestStage<int>(
        proc::generatorId("test.integer"), proc::SeedPath(91), stage_options,
        [](proc::GenerationContext&) -> RuntimeResult {
            return RuntimeResult::success(std::make_shared<const int>(1));
        });
    assert(typed_as_stage.status() == proc::GenerationStatus::Failed);
    assert(typed_as_stage.error().code == foundation::ErrorCode::InvalidArgument);

    const proc::GeneratorDescriptor stage_descriptor{
        proc::generatorId("test.stage"), "test.stage", {1, 0, 0},
        foundation::stable_id("test.stage.input"), foundation::stable_id("test.stage.output"),
        true, proc::GeneratorExecutionPolicy::Cpu, proc::GeneratorCachePolicy::None};
    proc::GeneratorRegistry::Builder stage_builder;
    assert(stage_builder.add(
        stage_descriptor,
        [](proc::GenerationContext&) {
            return foundation::Result<void, foundation::Error>::success();
        }));
    auto stage_registry = std::move(stage_builder).freeze();
    assert(stage_registry);
    proc::ProceduralRuntime stage_runtime(stage_registry.value(), jobs);
    auto stage_ticket = stage_runtime.requestStage<int>(
        proc::generatorId("test.stage"), proc::SeedPath(92), stage_options,
        [](proc::GenerationContext&) -> RuntimeResult {
            return RuntimeResult::success(std::make_shared<const int>(42));
        });
    stage_ticket.wait();
    assert(stage_ticket.status() == proc::GenerationStatus::Completed);
    assert(stage_ticket.artifact() != nullptr && *stage_ticket.artifact() == 42);
    jobs::CancelSource stage_cancel;
    stage_cancel.cancel();
    proc::GenerationOptions canceled_stage_options{};
    canceled_stage_options.cancellation = stage_cancel.token();
    auto canceled_stage = stage_runtime.requestStage<int>(
        proc::generatorId("test.stage"), proc::SeedPath(93), canceled_stage_options,
        [](proc::GenerationContext&) -> RuntimeResult {
            return RuntimeResult::success(std::make_shared<const int>(99));
        });
    canceled_stage.wait();
    assert(canceled_stage.status() == proc::GenerationStatus::Canceled);
    assert(!canceled_stage.artifact());
    const auto make_request = [](int input, foundation::StableId hash) {
        proc::GenerationRequest<int, int> request;
        request.generator = proc::generatorId("test.integer");
        request.input = std::make_shared<const int>(input);
        request.seed_path = proc::SeedPath(77);
        request.options.input_hash = hash;
        request.options.retained_bytes = sizeof(int);
        return request;
    };

    const std::uint32_t before_canonical_identity =
        calls.load(std::memory_order_relaxed);
    const auto caller_collision_a = runtime.generateInline(
        make_request(7, foundation::stable_id("caller.supplied.same-hash")));
    const auto caller_collision_b = runtime.generateInline(
        make_request(8, foundation::stable_id("caller.supplied.same-hash")));
    assert(caller_collision_a && *caller_collision_a.value() == 21);
    assert(caller_collision_b && *caller_collision_b.value() == 24);
    assert(calls.load(std::memory_order_relaxed) == before_canonical_identity + 2U);
    const auto caller_collision_cached = runtime.generateInline(
        make_request(7, foundation::stable_id("different.caller.hash")));
    assert(caller_collision_cached && *caller_collision_cached.value() == 21);
    assert(calls.load(std::memory_order_relaxed) == before_canonical_identity + 2U);

    const auto inline_result = runtime.generateInline(
        make_request(2, foundation::stable_id("input.two")));
    assert(inline_result && *inline_result.value() == 6);
    const std::uint32_t after_inline = calls.load(std::memory_order_relaxed);
    auto canceled_inline_request = make_request(2, foundation::stable_id("input.canceled"));
    jobs::CancelSource inline_cancel;
    canceled_inline_request.options.cancellation = inline_cancel.token();
    inline_cancel.cancel();
    const auto canceled_inline = runtime.generateInline(canceled_inline_request);
    assert(!canceled_inline);
    assert(canceled_inline.error().code == foundation::ErrorCode::InvalidState);
    assert(calls.load(std::memory_order_relaxed) == after_inline);
    const auto cached = runtime.generateInline(
        make_request(2, foundation::stable_id("input.two")));
    assert(cached && *cached.value() == 6);
    assert(calls.load(std::memory_order_relaxed) == after_inline);
    auto schema_changed = make_request(2, foundation::stable_id("input.two"));
    schema_changed.options.schema_version = 2U;
    const auto schema_result = runtime.generateInline(schema_changed);
    assert(schema_result && *schema_result.value() == 6);
    const std::uint32_t after_schema = calls.load(std::memory_order_relaxed);
    assert(after_schema == after_inline + 1U);
    auto dependency_changed = make_request(2, foundation::stable_id("input.two"));
    dependency_changed.options.dependency_hash = foundation::stable_id("dependency.two");
    const auto dependency_result = runtime.generateInline(dependency_changed);
    assert(dependency_result && *dependency_result.value() == 6);
    assert(calls.load(std::memory_order_relaxed) == after_schema + 1U);

    // Generator version is part of the artifact key: reusing the same cache
    // with a new registry version must not return the version-1 artifact.
    proc::GeneratorRegistry::Builder versioned_builder;
    assert((versioned_builder.addTyped<int, int>(
        descriptor(2), [&calls](const int& input, proc::GenerationContext&) -> RuntimeResult {
            calls.fetch_add(1, std::memory_order_relaxed);
            return RuntimeResult::success(std::make_shared<const int>(input * 3));
        },
        [](const int& input) {
            return foundation::stableHashU64(static_cast<std::uint64_t>(input));
        })));
    auto versioned_registry = std::move(versioned_builder).freeze();
    assert(versioned_registry);
    proc::ProceduralRuntime versioned_runtime(versioned_registry.value(), jobs, &cache);
    const auto before_versioned = calls.load(std::memory_order_relaxed);
    const auto versioned_result = versioned_runtime.generateInline(
        make_request(2, foundation::stable_id("input.two")));
    assert(versioned_result && *versioned_result.value() == 6);
    assert(calls.load(std::memory_order_relaxed) == before_versioned + 1U);

    proc::GenerationRequest<int, std::string> mismatched;
    mismatched.generator = proc::generatorId("test.integer");
    mismatched.input = std::make_shared<const int>(2);
    auto mismatch_ticket = runtime.request(std::move(mismatched));
    assert(mismatch_ticket.status() == proc::GenerationStatus::Failed);

    proc::GenerationChannel channel;
    auto first = runtime.request(make_request(1, foundation::stable_id("input.one")), &channel);
    first_started.wait();
    auto latest = runtime.request(make_request(3, foundation::stable_id("input.three")), &channel);
    latest.wait();
    assert(latest.status() == proc::GenerationStatus::Completed);
    assert(*latest.artifact() == 9);
    first.wait();
    assert(first.status() == proc::GenerationStatus::Superseded);
    assert(!first.artifact());
    assert(superseded_observed.load(std::memory_order_acquire));

    auto canceled = runtime.request(make_request(5, foundation::stable_id("input.five")));
    while (canceled.status() == proc::GenerationStatus::Pending) {
        std::this_thread::yield();
    }
    // A request that has entered execution must be visible as running until
    // its terminal transition, even when cancellation is cooperative.
    assert(runtime.telemetry().running >= 1U);
    canceled.cancel();
    canceled.wait();
    assert(canceled.status() == proc::GenerationStatus::Canceled);

    auto failed = runtime.request(make_request(4, foundation::stable_id("input.four")));
    failed.wait();
    assert(failed.status() == proc::GenerationStatus::Failed);
    assert(failed.error().code == foundation::ErrorCode::Internal);
    auto thrown = runtime.request(make_request(6, foundation::stable_id("input.six")));
    thrown.wait();
    assert(thrown.status() == proc::GenerationStatus::Failed);
    assert(thrown.error().message == "procedural generator threw");
    assert(!runtime.diagnostics().snapshot().empty());

    const auto telemetry = runtime.telemetry();
    assert(telemetry.requested >= 8);
    assert(telemetry.completed >= 3);
    assert(telemetry.failed >= 2);
    assert(telemetry.canceled >= 2);
    assert(telemetry.superseded == 1);
    assert(telemetry.running == 0U);
    assert(telemetry.cache_hits >= 1);
    assert(telemetry.cache_misses >= 1);

    // Worker-side composition should execute a registered child inline
    // through the same cache/cancellation contract, without creating a nested
    // ticket or consuming another worker just to wait for it.
    jobs::JobSystem inline_worker(1);
    proc::ArtifactCache inline_cache;
    proc::ProceduralRuntime inline_runtime(frozen.value(), inline_worker, &inline_cache);
    std::atomic_bool inline_composed{false};
    const auto inline_owner = inline_worker.submit([&](jobs::JobContext& job) {
        jobs::CancelSource cancellation;
        proc::ArtifactReader reader(inline_cache);
        proc::GenerationDiagnostics diagnostics;
        proc::GenerationContext parent(
            proc::SeedPath(77), &job, cancellation.token(), {}, &reader, &diagnostics);
        const auto generated = inline_runtime.generateInline(
            make_request(9, foundation::stable_id("ignored.caller.hash")), parent);
        assert(generated && *generated.value() == 27);
        const auto failed_inline = inline_runtime.generateInline(
            make_request(4, foundation::stable_id("ignored.failure.hash")), parent);
        assert(!failed_inline);
        const auto recorded = diagnostics.snapshot();
        assert(!recorded.empty());
        assert(recorded.back().generator == proc::generatorId("test.integer"));
        assert(recorded.back().error.code == foundation::ErrorCode::Internal);
        inline_composed.store(true, std::memory_order_release);
    });
    inline_worker.wait(inline_owner);
    assert(inline_composed.load(std::memory_order_acquire));

    // A composed generator may synchronously consume a child ticket while it
    // is itself running on the scheduler.  Ticket wait must use the owning
    // scheduler's worker-helping path, otherwise a one-worker pool deadlocks.
    jobs::JobSystem single_worker(1);
    proc::ProceduralRuntime nested_runtime(frozen.value(), single_worker);
    std::atomic_bool nested_completed{false};
    const auto outer = single_worker.submit([&](jobs::JobContext&) {
        auto nested = nested_runtime.request(
            make_request(8, foundation::stable_id("nested.worker.ticket")));
        nested.wait();
        nested_completed.store(nested.status() == proc::GenerationStatus::Completed,
                               std::memory_order_release);
    });
    single_worker.wait(outer);
    assert(nested_completed.load(std::memory_order_acquire));
    return 0;
}
