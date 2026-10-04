#include <genomes/hydrology/HydrologyProcedural.hpp>

#include <genomes/hydrology/HydrologyArtifact.hpp>

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
        });
}

} // namespace genomes::hydrology
