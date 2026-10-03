#include <genomes/world/WorldGenerationTask.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/hydrology/HydrologyProcedural.hpp>

#include <memory>
#include <utility>

namespace genomes::world {

namespace {

[[nodiscard]] foundation::StableId requestHash(const WorldGenerationRequest& request) noexcept {
    std::uint64_t input_hash = foundation::stableHashFloat(
        static_cast<float>(request.map_size_m));
    input_hash = foundation::stableHashCombine(
        input_hash, foundation::stableHashFloat(request.vegetation));
    input_hash = foundation::stableHashCombine(
        input_hash, foundation::stableHashFloat(request.buildings));
    input_hash = foundation::stableHashCombine(
        input_hash, foundation::stableHashFloat(request.fenced_parcels));
    input_hash = foundation::stableHashCombine(
        input_hash, static_cast<std::uint64_t>(request.hydrology_mode));
    return foundation::stableHashCombine(
        input_hash, foundation::stableHashFloat(request.river_probability));
}

} // namespace

bool WorldGenerationTask::failed() const noexcept {
    const proc::GenerationStatus status = ticket_.status();
    return status == proc::GenerationStatus::Failed ||
           status == proc::GenerationStatus::Canceled ||
           status == proc::GenerationStatus::Superseded;
}

foundation::Error WorldGenerationTask::error() const noexcept {
    if (!ticket_.valid()) {
        return {foundation::ErrorCode::InvalidState, "invalid world generation task"};
    }
    const proc::GenerationStatus status = ticket_.status();
    if (status == proc::GenerationStatus::Canceled ||
        status == proc::GenerationStatus::Superseded) {
        return {foundation::ErrorCode::InvalidState, "world generation task was canceled"};
    }
    return ticket_.error();
}

std::optional<WorldPlan> WorldGenerationTask::take_result() {
    if (!consumption_ || ticket_.status() != proc::GenerationStatus::Completed) {
        return std::nullopt;
    }
    std::lock_guard lock(consumption_->mutex);
    if (consumption_->consumed) {
        return std::nullopt;
    }
    const auto artifact = ticket_.artifact();
    if (!artifact) {
        return std::nullopt;
    }
    consumption_->consumed = true;
    return *artifact;
}

foundation::Result<void, foundation::Error> registerWorldGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("world.plan"),
        "world.plan",
        {static_cast<std::uint16_t>(WorldGeneratorVersion), 0, 0},
        foundation::stable_id("world.generation.request"),
        foundation::stable_id("world.plan"),
        true,
        proc::GeneratorExecutionPolicy::Cpu,
        proc::GeneratorCachePolicy::Artifact};
    const auto added = builder.addTyped<WorldGenerationRequest, WorldPlan>(
        descriptor,
        [](const WorldGenerationRequest& request,
           proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const WorldPlan>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const WorldPlan>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "world generation canceled"});
            }
            auto generated = WorldGenerator::generate(request);
            if (!generated) {
                return foundation::Result<std::shared_ptr<const WorldPlan>,
                                          foundation::Error>::failure(generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const WorldPlan>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "world generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const WorldPlan>,
                                      foundation::Error>::success(
                std::make_shared<const WorldPlan>(std::move(generated.value())));
        });
    return added;
}

proc::GeneratorRegistry WorldGenerationService::makeRegistry() {
    proc::GeneratorRegistry::Builder builder;
    if (!registerWorldGenerator(builder)) {
        return {};
    }
    if (!hydrology::registerHydrologyGenerator(builder)) {
        return {};
    }
    auto registry = std::move(builder).freeze();
    return registry ? std::move(registry).value() : proc::GeneratorRegistry{};
}

WorldGenerationService::WorldGenerationService(
    jobs::JobSystem& jobs,
    std::shared_ptr<proc::ArtifactCache> cache)
    : WorldGenerationService(jobs, std::move(cache), makeRegistry()) {}

WorldGenerationService::WorldGenerationService(
    jobs::JobSystem& jobs,
    std::shared_ptr<proc::ArtifactCache> cache,
    proc::GeneratorRegistry registry)
    : cache_(std::move(cache)), registry_(registry.size() != 0U
                                               ? std::move(registry)
                                               : makeRegistry()),
      runtime_(registry_, jobs, cache_.get()) {}

WorldGenerationTask WorldGenerationService::submit(const WorldGenerationRequest& request,
                                                   proc::GenerationChannel* channel) {
    proc::GenerationRequest<WorldGenerationRequest, WorldPlan> generation;
    generation.generator = proc::generatorId("world.plan");
    generation.input = std::make_shared<const WorldGenerationRequest>(request);
    generation.seed_path = proc::SeedPath(request.seed);
    generation.options.input_hash = requestHash(request);
    generation.options.retained_bytes = sizeof(WorldPlan);
    // ProceduralRuntime owns an internal cache when no shared cache was
    // supplied, so world generation remains cacheable in both configurations.
    generation.options.use_cache = true;
    return WorldGenerationTask(runtime_.request(std::move(generation), channel));
}

} // namespace genomes::world
