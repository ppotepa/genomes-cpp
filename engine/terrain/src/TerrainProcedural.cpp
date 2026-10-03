#include <genomes/terrain/TerrainProcedural.hpp>

#include <genomes/terrain/TerrainGenerator.hpp>

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
        });
}

} // namespace genomes::terrain
