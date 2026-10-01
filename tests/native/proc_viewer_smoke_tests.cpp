#include "ProcViewerApp.hpp"

#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <memory>
#include <thread>

int main() {
    using namespace genomes;
    auto loaded_building_profile = buildings::loadBuildingProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/building.json");
    assert(loaded_building_profile);
    proc_viewer::ProcViewerApp viewer(
        std::make_shared<const buildings::FrozenBuildingProfile>(
            std::move(loaded_building_profile.value())),
        2U);
    assert(proc_viewer::ProcViewerApp::modes().size() == 7U);
    assert(viewer.selectMode(proc_viewer::ViewerMode::RoadsCity));
    const auto profile = world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    assert(profile);
    const world::WorldGenerationRequest request =
        profile.value().makeRequest(0x7777ULL);
    assert(viewer.regenerate(request));
    for (std::size_t attempt = 0U; attempt < 10000U && viewer.artifact() == nullptr; ++attempt) {
        assert(viewer.poll());
        std::this_thread::yield();
    }
    assert(viewer.artifact() != nullptr);
    const auto hash = viewer.artifact()->semantic.content_hash;
    world::WorldGenerationRequest invalid = request;
    invalid.map_size_m = 64U;
    assert(!viewer.regenerate(invalid));
    assert(viewer.artifact() != nullptr && viewer.artifact()->semantic.content_hash == hash);
    assert(viewer.trace().size() == 1U);
    return 0;
}
