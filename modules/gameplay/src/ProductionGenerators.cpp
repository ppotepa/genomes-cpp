#include <genomes/gameplay/ProductionGenerators.hpp>

#include <genomes/buildings/BuildingProcedural.hpp>
#include <genomes/hydrology/HydrologyProcedural.hpp>
#include <genomes/terrain/TerrainProcedural.hpp>
#include <genomes/world/CityPlan.hpp>
#include <genomes/world/WorldGenerationTask.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <memory>

#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantryProcedural.hpp>
#include <genomes/weapons/WeaponProcedural.hpp>
#endif

namespace genomes::gameplay {

foundation::Result<proc::GeneratorRegistry, foundation::Error>
makeProductionGeneratorRegistry() {
    proc::GeneratorRegistry::Builder builder;
    const auto append = [](foundation::Result<void, foundation::Error> result)
        -> foundation::Result<void, foundation::Error> {
        if (!result) return result;
        return foundation::Result<void, foundation::Error>::success();
    };
    if (const auto result = append(buildings::registerBuildingGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
    const proc::GeneratorDescriptor roads_descriptor{
        proc::generatorId("roads.graph"), "roads.graph", {1, 0, 0},
        foundation::stable_id("world.city-generation-request"),
        foundation::stable_id("roads.graph"), true, proc::GeneratorExecutionPolicy::Cpu,
        proc::GeneratorCachePolicy::Artifact};
    if (const auto result = append(builder.addTyped<world::CityGenerationRequest, roads::RoadGraph>(
            roads_descriptor,
            [](const world::CityGenerationRequest& request, proc::GenerationContext& context)
                -> foundation::Result<std::shared_ptr<const roads::RoadGraph>, foundation::Error> {
                if (context.cancellationRequested()) {
                    return foundation::Result<std::shared_ptr<const roads::RoadGraph>,
                                              foundation::Error>::failure(
                        {foundation::ErrorCode::InvalidState, "roads generation canceled"});
                }
                const auto generated = world::CityGenerator::generate(request);
                if (!generated) {
                    return foundation::Result<std::shared_ptr<const roads::RoadGraph>,
                                              foundation::Error>::failure(generated.error());
                }
                return foundation::Result<std::shared_ptr<const roads::RoadGraph>,
                                          foundation::Error>::success(
                    std::make_shared<const roads::RoadGraph>(generated.value().road_graph));
            },
            [](const world::CityGenerationRequest& request) {
                std::uint64_t hash = foundation::stableHashU64(request.seed);
                hash = foundation::stableHashCombine(hash, request.map_size_m);
                hash = foundation::stableHashCombine(
                    hash, foundation::stableHashFloat(request.buildings));
                hash = foundation::stableHashCombine(
                    hash, foundation::stableHashFloat(request.fenced_parcels));
                return hash;
            }));
        !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
#if GENOMES_HAS_INFANTRY
    if (const auto result = append(infantry::registerInfantryGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
    if (const auto result = append(weapons::registerWeaponGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
#endif
    if (const auto result = append(terrain::registerTerrainGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
    if (const auto result = append(hydrology::registerHydrologyGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
    if (const auto result = append(world::registerWorldGenerator(builder)); !result) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            result.error());
    }
    auto frozen = std::move(builder).freeze();
    if (!frozen) {
        return foundation::Result<proc::GeneratorRegistry, foundation::Error>::failure(
            frozen.error());
    }
    return foundation::Result<proc::GeneratorRegistry, foundation::Error>::success(
        std::move(frozen.value()));
}

} // namespace genomes::gameplay
