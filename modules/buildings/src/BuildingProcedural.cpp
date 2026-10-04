#include <genomes/buildings/BuildingProcedural.hpp>

#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <memory>

namespace genomes::buildings {

namespace {

[[nodiscard]] foundation::StableId buildingSpecHash(const BuildingSpec& spec) noexcept {
    std::uint64_t hash = foundation::stableHashCombine(spec.building_id, spec.seed);
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(spec.footprint.x));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(spec.footprint.y));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(spec.footprint.z));
    hash = foundation::stableHashCombine(hash, spec.floors);
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(spec.floor_height));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(spec.wall_thickness));
    hash = foundation::stableHashCombine(hash, spec.rooms_per_floor);
    return hash == 0U ? 1U : hash;
}

[[nodiscard]] foundation::StableId buildingSiteHash(
    const BuildingSiteGenerationRequest& request) noexcept {
    const auto& site = request.site;
    const auto& profile = request.profile;
    std::uint64_t hash = foundation::stableHashCombine(site.request_id, site.parcel_id);
    hash = foundation::stableHashCombine(hash, site.seed);
    for (const foundation::Vec2 point : site.buildable_polygon) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(point.x));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(point.y));
    }
    const auto combine_vec3 = [&hash](foundation::Vec3 value) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.x));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.y));
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value.z));
    };
    combine_vec3(site.preferred_position);
    combine_vec3(site.preferred_footprint);
    hash = foundation::stableHashCombine(
        hash, foundation::stableHashFloat(site.preferred_rotation));
    hash = foundation::stableHashCombine(hash, site.floors_min);
    hash = foundation::stableHashCombine(hash, site.floors_max);
    hash = foundation::stableHashCombine(
        hash, foundation::stableHashFloat(site.access_point.x));
    hash = foundation::stableHashCombine(
        hash, foundation::stableHashFloat(site.access_point.y));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(site.access_width));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(site.clearance_m));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(site.access_class));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(site.access_surface));
    hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(profile.floor_height));
    hash = foundation::stableHashCombine(
        hash, foundation::stableHashFloat(profile.wall_thickness));
    hash = foundation::stableHashCombine(
        hash, foundation::stableHashFloat(profile.target_room_width));
    hash = foundation::stableHashCombine(hash, profile.minimum_rooms_per_floor);
    hash = foundation::stableHashCombine(hash, profile.maximum_rooms_per_floor);
    return hash == 0U ? 1U : hash;
}

} // namespace

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
        },
        buildingSpecHash);
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
        },
        buildingSiteHash);
}

} // namespace genomes::buildings
