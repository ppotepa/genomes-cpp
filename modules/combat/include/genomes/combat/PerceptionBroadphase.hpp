#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>
#include <genomes/spatial/SpatialGrid.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace genomes::combat {

struct PerceptionProfile final {
    float range_m{100.0F};
    float horizontal_half_fov_radians{1.22173048F};
    std::uint32_t max_candidates{64U};
    std::uint32_t enemy_team_mask{0xFFFF'FFFFu};

    [[nodiscard]] bool valid() const noexcept;
};

struct PerceptionAgent final {
    simulation::EntityId id{};
    foundation::Vec3 position{};
    float heading_radians{0.0F};
    std::uint32_t team{0U};
    std::uint32_t category_mask{0xFFFF'FFFFu};
    bool alive{true};

    [[nodiscard]] bool valid() const noexcept;
};

struct PerceptionCandidate final {
    simulation::EntityId target{};
    foundation::Vec3 relative{};
    float distance_squared{0.0F};
};

struct PerceptionDiagnostics final {
    std::uint32_t grid_candidates{0U};
    std::uint32_t rejected_team{0U};
    std::uint32_t rejected_range{0U};
    std::uint32_t rejected_fov{0U};
    std::uint32_t accepted{0U};
    bool truncated{false};
};

class PerceptionBroadphase final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> rebuild(
        std::span<const PerceptionAgent> agents, spatial::UniformGrid& grid);

    [[nodiscard]] foundation::Result<std::vector<PerceptionCandidate>, foundation::Error> query(
        const PerceptionAgent& observer,
        const PerceptionProfile& profile,
        const spatial::UniformGrid& grid) const;

    [[nodiscard]] const PerceptionDiagnostics& diagnostics() const noexcept { return diagnostics_; }

private:
    std::vector<PerceptionAgent> agents_;
    mutable PerceptionDiagnostics diagnostics_{};
};

} // namespace genomes::combat
