#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/GenerationClient.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/world/WorldGenerationTask.hpp>
#include <genomes/world/WorldPosition.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace genomes::world {

enum class ResidencyAxis : std::uint8_t {
    Semantic = 1U << 0U,
    Simulation = 1U << 1U,
    Render = 1U << 2U,
    Physics = 1U << 3U,
    Navigation = 1U << 4U
};
using ResidencyMask = std::uint8_t;

[[nodiscard]] constexpr ResidencyMask residency(ResidencyAxis axis) noexcept {
    return static_cast<ResidencyMask>(axis);
}

enum class ResidencyReason : std::uint8_t {
    Camera,
    Simulation,
    PathCorridor,
    Combat,
    Preload,
    Persistence
};

struct ResidencyRequest final {
    RegionId id{};
    RegionCoord coordinate{};
    ResidencyMask desired{0U};
    ResidencyReason reason{ResidencyReason::Preload};
    std::uint8_t priority{0U};
    std::uint32_t pin_count{0U};
    foundation::SimulationTick tick{};
};

struct ResidencyState final {
    RegionId id{};
    RegionCoord coordinate{};
    ResidencyMask desired{0U};
    ResidencyMask current{0U};
    std::uint32_t pin_count{0U};
    std::uint64_t generation{0U};
    std::uint64_t content_hash{0U};
    std::size_t semantic_bytes{0U};
    std::size_t render_bytes{0U};
    std::size_t physics_bytes{0U};
    std::size_t navigation_bytes{0U};
    foundation::SimulationTick last_access{};
    bool pending{false};

    [[nodiscard]] bool valid() const noexcept { return id.isValid(); }
};

struct StreamedRegion final {
    RegionId id{};
    RegionCoord coordinate{};
    WorldPosition origin{};
    std::uint64_t generation{0U};
    WorldPlan plan{};
};

struct WorldStreamerConfig final {
    std::size_t semantic_budget_bytes{256U * 1024U * 1024U};
    std::size_t render_budget_bytes{256U * 1024U * 1024U};
    std::size_t physics_budget_bytes{64U * 1024U * 1024U};
    std::size_t navigation_budget_bytes{64U * 1024U * 1024U};
    std::uint32_t max_pending{256U};

    [[nodiscard]] bool valid() const noexcept { return max_pending > 0U; }
};

class WorldStreamer final {
public:
    WorldStreamer(WorldId world_id, WorldGenerationRequest request,
                   WorldCoordinateConfig coordinates, jobs::JobSystem& jobs,
                   WorldStreamerConfig config = {},
                   proc::GeneratorRegistry registry = {});
    WorldStreamer(WorldId world_id, WorldGenerationRequest request,
                  WorldCoordinateConfig coordinates, proc::GenerationClient generation,
                  WorldStreamerConfig config = {});
    ~WorldStreamer() noexcept;

    WorldStreamer(const WorldStreamer&) = delete;
    WorldStreamer& operator=(const WorldStreamer&) = delete;

    [[nodiscard]] foundation::Result<void, foundation::Error> setDesired(
        const ResidencyRequest& request);
    [[nodiscard]] foundation::Result<std::uint32_t, foundation::Error> poll(
        foundation::SimulationTick tick);
    [[nodiscard]] foundation::Result<std::uint32_t, foundation::Error> evict();

    [[nodiscard]] const ResidencyState* state(RegionId id) const noexcept;
    [[nodiscard]] std::vector<StreamedRegion> takeReady();
    [[nodiscard]] std::size_t pendingCount() const noexcept { return pending_.size(); }
    [[nodiscard]] std::size_t residentCount() const noexcept;
    [[nodiscard]] std::size_t residentBytes(ResidencyAxis axis) const noexcept;
    [[nodiscard]] foundation::Error lastError() const noexcept { return last_error_; }

private:
    struct Pending final {
        RegionCoord coordinate{};
        WorldPosition origin{};
        std::uint64_t generation{0U};
        WorldGenerationTask task{};
    };

    [[nodiscard]] foundation::Result<void, foundation::Error> schedule(
        ResidencyState& state);
    [[nodiscard]] static bool validMask(ResidencyMask mask) noexcept;
    static void translatePlan(WorldPlan& plan, WorldPosition origin) noexcept;
    void clearAxis(ResidencyState& state, ResidencyAxis axis) noexcept;

    WorldId world_id_{};
    WorldGenerationRequest request_{};
    WorldCoordinateConfig coordinates_{};
    WorldStreamerConfig config_{};
    std::shared_ptr<proc::ArtifactCache> cache_;
    WorldGenerationService generation_service_;
    std::map<std::uint64_t, ResidencyState> states_;
    std::map<std::uint64_t, Pending> pending_;
    std::vector<StreamedRegion> ready_;
    foundation::Error last_error_{};
};

} // namespace genomes::world
