#include <genomes/world/WorldStreamer.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <thread>

int main() {
    using namespace genomes;
    jobs::JobSystem jobs(2U);
    const auto profile = world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    assert(profile);
    world::WorldGenerationRequest generation =
        profile.value().makeRequest(0x11112222ULL);
    generation.map_size_m = 256U;
    world::WorldCoordinateConfig coordinates{};
    coordinates.region_size_m = 256.0;
    world::WorldStreamer streamer(world::WorldId(1U), generation, coordinates, jobs);
    const world::RegionCoord origin{0, 0, 0};
    const world::RegionId first = world::regionId(world::WorldId(1U), origin);
    constexpr world::ResidencyMask semantic_simulation =
        static_cast<world::ResidencyMask>(world::ResidencyAxis::Semantic) |
        static_cast<world::ResidencyMask>(world::ResidencyAxis::Simulation);
    assert(streamer.setDesired({first, origin, semantic_simulation,
                                world::ResidencyReason::Simulation, 10U, 0U, {0U}}));
    for (std::size_t attempt = 0U; attempt < 10000U && streamer.residentCount() == 0U; ++attempt) {
        assert(streamer.poll({static_cast<std::uint64_t>(attempt)}));
        std::this_thread::yield();
    }
    assert(streamer.residentCount() == 1U);
    assert(streamer.takeReady().size() == 1U);
    const auto* loaded = streamer.state(first);
    assert(loaded != nullptr && (loaded->current & semantic_simulation) == semantic_simulation);

    const world::ResidencyMask all_axes = semantic_simulation |
        static_cast<world::ResidencyMask>(world::ResidencyAxis::Render) |
        static_cast<world::ResidencyMask>(world::ResidencyAxis::Physics) |
        static_cast<world::ResidencyMask>(world::ResidencyAxis::Navigation);
    assert(streamer.setDesired({first, origin, all_axes, world::ResidencyReason::Camera, 10U, 0U,
                                {10U}}));
    assert(streamer.setDesired({first, origin, semantic_simulation,
                                world::ResidencyReason::Simulation, 10U, 0U, {11U}}));
    assert(streamer.evict());
    loaded = streamer.state(first);
    assert(loaded != nullptr && (loaded->current & semantic_simulation) == semantic_simulation);
    assert((loaded->current & static_cast<world::ResidencyMask>(world::ResidencyAxis::Render)) == 0U);

    assert(streamer.setDesired({first, origin, 0U, world::ResidencyReason::Camera, 0U, 1U, {12U}}));
    assert(streamer.evict());
    assert(streamer.state(first)->current != 0U);
    assert(streamer.setDesired({first, origin, 0U, world::ResidencyReason::Camera, 0U, 0U, {13U}}));
    assert(streamer.evict());
    assert(streamer.state(first)->current == 0U);

    const world::RegionCoord stale_coord{1, 0, 0};
    const world::RegionId stale = world::regionId(world::WorldId(1U), stale_coord);
    assert(streamer.setDesired({stale, stale_coord,
                                static_cast<world::ResidencyMask>(world::ResidencyAxis::Semantic),
                                world::ResidencyReason::Preload, 1U, 0U, {20U}}));
    assert(streamer.setDesired({stale, stale_coord, 0U, world::ResidencyReason::Camera, 0U, 0U,
                                {21U}}));
    for (std::size_t attempt = 0U; attempt < 10000U && streamer.pendingCount() != 0U; ++attempt) {
        assert(streamer.poll({static_cast<std::uint64_t>(attempt + 21U)}));
        std::this_thread::yield();
    }
    assert(streamer.residentCount() == 0U);
    return 0;
}
