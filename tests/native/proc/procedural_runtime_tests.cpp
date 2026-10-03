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
    const auto make_request = [](int input, foundation::StableId hash) {
        proc::GenerationRequest<int, int> request;
        request.generator = proc::generatorId("test.integer");
        request.input = std::make_shared<const int>(input);
        request.seed_path = proc::SeedPath(77);
        request.options.input_hash = hash;
        request.options.retained_bytes = sizeof(int);
        return request;
    };

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
    assert(telemetry.cache_hits >= 1);
    return 0;
}
