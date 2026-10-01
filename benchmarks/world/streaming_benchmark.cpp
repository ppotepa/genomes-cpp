#include <genomes/world/WorldStreamer.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

void run(std::size_t count, const genomes::world::FrozenWorldGenerationProfile& profile) {
    using namespace genomes;
    jobs::JobSystem jobs(4U);
    world::WorldGenerationRequest request =
        profile.makeRequest(0xABCD0000ULL + static_cast<std::uint64_t>(count));
    request.map_size_m = 128U;
    world::WorldCoordinateConfig coordinates{};
    coordinates.region_size_m = 128.0;
    world::WorldStreamerConfig config{};
    config.max_pending = static_cast<std::uint32_t>(count + 1U);
    world::WorldStreamer streamer(world::WorldId(2U), request, coordinates, jobs, config);
    const auto begin = std::chrono::steady_clock::now();
    const auto semantic = static_cast<world::ResidencyMask>(world::ResidencyAxis::Semantic);
    for (std::size_t index = 0U; index < count; ++index) {
        const world::RegionCoord coordinate{static_cast<std::int64_t>(index), 0, 0};
        (void)streamer.setDesired({world::regionId(world::WorldId(2U), coordinate), coordinate,
                                   semantic, world::ResidencyReason::Preload, 1U, 0U, {0U}});
    }
    for (std::size_t attempt = 0U; attempt < 100000U && streamer.pendingCount() != 0U; ++attempt) {
        (void)streamer.poll({static_cast<std::uint64_t>(attempt)});
        std::this_thread::yield();
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "world_streaming regions=" << count << " ms=" << elapsed.count()
              << " resident=" << streamer.residentCount() << std::endl;
}

} // namespace

int main() {
    const auto profile = genomes::world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    if (!profile) {
        std::cerr << "world profile loading failed: " << profile.error().message << '\n';
        return 1;
    }
    run(32U, profile.value());
    run(128U, profile.value());
    run(256U, profile.value());
    return 0;
}
