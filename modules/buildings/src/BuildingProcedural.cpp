#include <genomes/buildings/BuildingProcedural.hpp>

#include <genomes/buildings/BuildingModel.hpp>

#include <memory>

namespace genomes::buildings {

foundation::Result<void, foundation::Error> registerBuildingGenerator(
    proc::GeneratorRegistry::Builder& builder) {
    const proc::GeneratorDescriptor descriptor{
        proc::generatorId("buildings.plan"),
        "buildings.plan",
        {static_cast<std::uint16_t>(BuildingGeneratorVersion), 0, 0},
        foundation::stable_id("buildings.spec"),
        foundation::stable_id("buildings.plan"),
        true,
        proc::GeneratorExecutionPolicy::Cpu,
        proc::GeneratorCachePolicy::Artifact};
    const auto added_plan = builder.addTyped<BuildingSpec, BuildingPlan>(
        descriptor,
        [](const BuildingSpec& spec,
           proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const BuildingPlan>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const BuildingPlan>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "building generation canceled"});
            }
            auto generated = BuildingGenerator::generate(spec);
            if (!generated) {
                return foundation::Result<std::shared_ptr<const BuildingPlan>,
                                          foundation::Error>::failure(generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const BuildingPlan>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "building generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const BuildingPlan>,
                                      foundation::Error>::success(
                std::make_shared<const BuildingPlan>(std::move(generated.value())));
        });
    if (!added_plan) {
        return added_plan;
    }
    const proc::GeneratorDescriptor site_descriptor{
        proc::generatorId("buildings.site"),
        "buildings.site",
        {static_cast<std::uint16_t>(BuildingGeneratorVersion), 0, 0},
        foundation::stable_id("buildings.site.request"),
        foundation::stable_id("buildings.site.result"),
        true,
        proc::GeneratorExecutionPolicy::Cpu,
        proc::GeneratorCachePolicy::Artifact};
    return builder.addTyped<BuildingSiteGenerationRequest, BuildingGenerationResult>(
        site_descriptor,
        [](const BuildingSiteGenerationRequest& request,
           proc::GenerationContext& context)
            -> foundation::Result<std::shared_ptr<const BuildingGenerationResult>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const BuildingGenerationResult>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "building site generation canceled"});
            }
            auto generated = BuildingGenerator::generateSite(request.site, request.profile);
            if (!generated) {
                return foundation::Result<std::shared_ptr<const BuildingGenerationResult>,
                                          foundation::Error>::failure(generated.error());
            }
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const BuildingGenerationResult>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "building site generation canceled"});
            }
            return foundation::Result<std::shared_ptr<const BuildingGenerationResult>,
                                      foundation::Error>::success(
                std::make_shared<const BuildingGenerationResult>(std::move(generated.value())));
        });
}

} // namespace genomes::buildings
