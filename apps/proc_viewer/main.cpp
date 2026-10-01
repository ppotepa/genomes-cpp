#include "ProcViewerApp.hpp"

#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <iostream>
#include <memory>
#include <thread>
#include <utility>

int main() {
    using namespace genomes;
    auto loaded_world_profile = world::loadWorldGenerationProfile(
        "mods/core/profiles/world-generation.json");
    if (!loaded_world_profile) {
        std::cerr << "world profile load failed: "
                  << loaded_world_profile.error().message << '\n';
        return 1;
    }
    auto loaded_building_profile = buildings::loadBuildingProfile(
        "mods/core/profiles/building.json");
    if (!loaded_building_profile) {
        std::cerr << "building profile load failed: "
                  << loaded_building_profile.error().message << '\n';
        return 1;
    }
    proc_viewer::ProcViewerApp viewer(
        std::make_shared<const buildings::FrozenBuildingProfile>(
            std::move(loaded_building_profile.value())));
    const world::WorldGenerationRequest request =
        loaded_world_profile.value().makeRequest(0x50524F43ULL);
    if (!viewer.regenerate(request)) {
        return 1;
    }
    for (std::size_t attempt = 0U; attempt < 100000U && viewer.artifact() == nullptr; ++attempt) {
        const auto result = viewer.poll();
        if (!result) {
            return 1;
        }
        std::this_thread::yield();
    }
    if (viewer.artifact() == nullptr) {
        return 1;
    }
    const auto& semantic = viewer.artifact()->semantic;
    std::cout << "proc_viewer hash=" << semantic.content_hash
              << " seed=" << request.seed
              << " map=" << semantic.map_size_m
              << " features=" << semantic.feature_count
              << " roads=" << semantic.road_count
              << " parcels=" << semantic.parcel_count
              << " buildings=" << semantic.building_count
              << " sites=" << semantic.building_site_count
              << " vegetation=" << semantic.vegetation_count
              << " rivers=" << semantic.river_count
              << " terrain_samples=" << semantic.terrain_sample_count << '\n';
    return 0;
}
