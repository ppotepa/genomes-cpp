#include <genomes/weapons/WeaponProcedural.hpp>

#include <memory>

namespace genomes::weapons {

foundation::Result<void, foundation::Error> registerWeaponGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("weapons.artifact"), "weapons.artifact", {1, 0, 0},
        foundation::stable_id("weapons.generation.request"), foundation::stable_id("weapons.artifact"),
        true, proc::GeneratorExecutionPolicy::Cpu, proc::GeneratorCachePolicy::Artifact};
    return builder.addTyped<WeaponGenerationRequest, WeaponArtifact>(
        descriptor,
        [](const WeaponGenerationRequest& request, proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const WeaponArtifact>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const WeaponArtifact>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "weapon generation canceled"});
            }
            auto generated = WeaponGeometryGenerator::build(request.definition, request.variant);
            if (!generated) {
                return foundation::Result<std::shared_ptr<const WeaponArtifact>, foundation::Error>::failure(
                    generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const WeaponArtifact>, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "weapon generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const WeaponArtifact>, foundation::Error>::success(
                std::make_shared<const WeaponArtifact>(std::move(generated.value())));
        },
        [](const WeaponGenerationRequest& request) {
            return WeaponGeometryGenerator::cacheKey(request.definition, request.variant);
        });
}

} // namespace genomes::weapons
