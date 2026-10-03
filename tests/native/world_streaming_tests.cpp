#include <genomes/world/WorldStreamer.hpp>
#include <genomes/world/WorldRegionStreamer.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <memory>
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

    // Leaving a region's desired set cancels its generation task.  That
    // deliberate supersession must not be reported as a streamer failure.
    world::WorldRegionStreamer region_streamer(
        world::WorldId(2U), generation, coordinates, jobs, 0U);
    region_streamer.update({0.0, 0.0, 0.0});
    region_streamer.update({coordinates.region_size_m * 20.0, 0.0, 0.0});
    for (std::size_t attempt = 0U;
         attempt < 10000U && region_streamer.pending_count() != 0U; ++attempt) {
        region_streamer.poll();
        std::this_thread::yield();
    }
    assert(!region_streamer.failed());

    proc::GeneratorRegistry::Builder failing_builder;
    const proc::GeneratorDescriptor failing_descriptor{
        proc::generatorId("world.plan"), "world.plan", {world::WorldGeneratorVersion, 0, 0},
        foundation::stable_id("world.generation.request"), foundation::stable_id("world.plan"),
        true, proc::GeneratorExecutionPolicy::Cpu, proc::GeneratorCachePolicy::None};
    assert((failing_builder.addTyped<world::WorldGenerationRequest, world::WorldPlan>(
        failing_descriptor,
        [](const world::WorldGenerationRequest&, proc::GenerationContext&)
            -> foundation::Result<std::shared_ptr<const world::WorldPlan>, foundation::Error> {
            return foundation::Result<std::shared_ptr<const world::WorldPlan>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "synthetic world generation failure"});
        })));
    auto failing_registry = std::move(failing_builder).freeze();
    assert(failing_registry);
    world::WorldStreamer failing_streamer(world::WorldId(3U), generation, coordinates, jobs, {},
                                          std::move(failing_registry.value()));
    const world::RegionId failing_region = world::regionId(world::WorldId(3U), origin);
    assert(failing_streamer.setDesired({failing_region, origin, semantic_simulation,
                                        world::ResidencyReason::Simulation, 0U, 0U, {0U}}));
    bool saw_failure = false;
    for (std::size_t attempt = 0U; attempt < 10000U && !saw_failure; ++attempt) {
        const auto polled = failing_streamer.poll({static_cast<std::uint64_t>(attempt)});
        if (!polled) {
            saw_failure = true;
            assert(polled.error().code == foundation::ErrorCode::Internal);
            assert(failing_streamer.lastError().message == "synthetic world generation failure");
        }
        std::this_thread::yield();
    }
    assert(saw_failure);
    return 0;
}
