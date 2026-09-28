#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/world/WorldQuerySnapshot.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace genomes::combat {

struct LOSRequest final {
    foundation::StableId observer_id{0};
    foundation::StableId target_id{0};
    foundation::Vec3 eye{};
    foundation::Vec3 aim{};
    std::uint32_t collision_mask{0xFFFF'FFFFu};

    [[nodiscard]] bool valid() const noexcept;
};

struct LOSResult final {
    bool visible{false};
    world::QueryCompleteness completeness{world::QueryCompleteness::Missing};
    std::optional<foundation::StableId> blocker;
    float hit_distance{0.0F};
    std::uint64_t world_revision{0};
};

struct LOSBatchStats final {
    std::uint32_t requests{0};
    std::uint32_t visible{0};
    std::uint32_t blocked{0};
    std::uint32_t stale{0};
};

class LineOfSight final {
public:
    [[nodiscard]] static foundation::Result<std::vector<LOSResult>, foundation::Error> query(
        const world::WorldQuerySnapshot&, std::span<const LOSRequest>);

    [[nodiscard]] static foundation::Result<LOSResult, foundation::Error> queryOne(
        const world::WorldQuerySnapshot&, const LOSRequest&);

    [[nodiscard]] static LOSBatchStats lastStats() noexcept;
};

} // namespace genomes::combat
