#pragma once

#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/world/WorldGenerationTask.hpp>
#include <genomes/world/WorldPlan.hpp>
#include <genomes/world/WorldPosition.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <vector>

namespace genomes::world {

struct WorldRegion final {
    RegionId id{};
    RegionCoord coord{};
    WorldPosition origin{};
    WorldPlan plan{};
};

class WorldRegionStreamer final {
public:
    WorldRegionStreamer(WorldId world_id,
                        WorldGenerationRequest request,
                        WorldCoordinateConfig coordinates,
                        jobs::JobSystem& jobs,
                        std::uint32_t load_radius = 1);

    WorldRegionStreamer(const WorldRegionStreamer&) = delete;
    WorldRegionStreamer& operator=(const WorldRegionStreamer&) = delete;

    void update(WorldPosition observer);
    void poll();

    [[nodiscard]] std::vector<WorldRegion> take_ready();
    [[nodiscard]] std::size_t pending_count() const noexcept;
    [[nodiscard]] std::size_t loaded_count() const noexcept;
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] foundation::Error error() const noexcept { return error_; }

private:
    struct PendingRegion final {
        RegionId id{};
        RegionCoord coord{};
        WorldPosition origin{};
        WorldGenerationTask task{};
        bool wanted{false};
    };

    void schedule(RegionCoord coord, RegionId id);

    WorldId world_id_{};
    WorldGenerationRequest request_{};
    WorldCoordinateConfig coordinates_{};
    std::shared_ptr<proc::ArtifactCache> cache_;
    WorldGenerationService generation_service_;
    std::uint32_t load_radius_{1};
    std::map<std::uint64_t, PendingRegion> pending_;
    std::map<std::uint64_t, RegionCoord> resident_;
    std::vector<WorldRegion> ready_;
    foundation::Error error_{};
    bool failed_{false};
};

} // namespace genomes::world
