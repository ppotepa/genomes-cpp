#include <genomes/terrain/TerrainProcedural.hpp>

#include <genomes/terrain/TerrainGenerator.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <memory>

namespace genomes::terrain {

foundation::Result<void, foundation::Error> registerTerrainGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("terrain.height-field"), "terrain.height-field", {1, 0, 0},
        foundation::stable_id("terrain.spec"), foundation::stable_id("terrain.height-field"),
        true, proc::GeneratorExecutionPolicy::Cpu, proc::GeneratorCachePolicy::Artifact};
    return builder.addTyped<TerrainSpec, HeightField>(
        descriptor,
        [](const TerrainSpec& spec, proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const HeightField>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const HeightField>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "terrain generation canceled"});
            }
            auto generated = TerrainGenerator::generate(spec);
            if (!generated) {
                return foundation::Result<std::shared_ptr<const HeightField>, foundation::Error>::failure(
                    generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const HeightField>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "terrain generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const HeightField>, foundation::Error>::success(
                std::make_shared<const HeightField>(std::move(generated.value())));
        },
        [](const TerrainSpec& spec) {
            std::uint64_t hash = foundation::stableHashU64(spec.world_id.value());
            hash = foundation::stableHashCombine(
                hash, static_cast<std::uint64_t>(spec.region.x));
            hash = foundation::stableHashCombine(
                hash, static_cast<std::uint64_t>(spec.region.z));
            hash = foundation::stableHashCombine(
                hash, static_cast<std::uint64_t>(spec.region.layer));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashDouble(spec.coordinates.region_size_m));
            hash = foundation::stableHashCombine(hash, spec.seed_path.seed());
            hash = foundation::stableHashCombine(hash, spec.samples_x);
            hash = foundation::stableHashCombine(hash, spec.samples_z);
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.cell_size_m));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashDouble(spec.origin_offset_x));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashDouble(spec.origin_offset_z));
            hash = foundation::stableHashCombine(
                hash, static_cast<std::uint64_t>(spec.generation.preset));
            hash = foundation::stableHashCombine(hash, spec.generation.sample_spacing_m);
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.generation.elevation_range_m));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.generation.landform_scale_m));
            hash = foundation::stableHashCombine(
                hash, foundation::stableHashFloat(spec.generation.roughness));
            return hash == 0U ? foundation::StableId{1U} : hash;
        });
}

} // namespace genomes::terrain
