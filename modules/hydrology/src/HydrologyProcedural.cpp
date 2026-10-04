#include <genomes/hydrology/HydrologyProcedural.hpp>

#include <genomes/hydrology/HydrologyArtifact.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <memory>

namespace genomes::hydrology {

foundation::Result<void, foundation::Error> registerHydrologyGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("hydrology.artifact"), "hydrology.artifact", {HydrologyGeneratorVersion, 0, 0},
        foundation::stable_id("hydrology.generation-input"), foundation::stable_id("hydrology.artifact"),
        true, proc::GeneratorExecutionPolicy::Cpu, proc::GeneratorCachePolicy::Artifact};
    return builder.addTyped<HydrologyGenerationInput, HydrologyArtifact>(
        descriptor,
        [](const HydrologyGenerationInput& input, proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "hydrology generation canceled"});
            }
            if (!input.valid()) {
                return foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidArgument, "invalid terrain-dependent hydrology input"});
            }
            auto generated = HydrologyGenerator::generate(input.spec, input.terrainView());
            if (!generated) {
                return foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error>::failure(
                    generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "hydrology generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const HydrologyArtifact>, foundation::Error>::success(
                std::make_shared<const HydrologyArtifact>(std::move(generated.value())));
        },
        [](const HydrologyGenerationInput& input) {
            const auto& spec = input.spec;
            std::uint64_t hash = foundation::stableHashU64(spec.seed);
            hash = foundation::stableHashCombine(hash, spec.map_size_m);
            hash = foundation::stableHashCombine(hash, spec.cells_x);
            hash = foundation::stableHashCombine(hash, spec.cells_z);
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.cell_size_m));
            hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(spec.mode));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.river_probability));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.base_width_m));
            hash = foundation::stableHashCombine(hash, spec.main_river_min);
            hash = foundation::stableHashCombine(hash, spec.main_river_max);
            hash = foundation::stableHashCombine(
                hash, static_cast<std::uint64_t>(spec.tributary_density));
            for (const float value : {
                     spec.stream_width_min_m, spec.stream_width_max_m,
                     spec.river_width_min_m, spec.river_width_max_m,
                     spec.depth_min_m, spec.depth_max_m, spec.meander_strength,
                     spec.valley_width_min_m, spec.valley_width_max_m}) {
                hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value));
            }
            hash = foundation::stableHashCombine(hash, input.samples_x);
            hash = foundation::stableHashCombine(hash, input.samples_z);
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(input.cell_size_m));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(input.origin_x));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(input.origin_z));
            if (input.heights != nullptr) {
                for (const float height : *input.heights) {
                    hash = foundation::stableHashCombine(
                        hash, foundation::stableHashFloat(height));
                }
            }
            return hash == 0U ? foundation::StableId{1U} : hash;
        });
}

} // namespace genomes::hydrology
