#pragma once

#include <genomes/destruction/RubbleField.hpp>
#include <genomes/world/DirtyBounds.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>

namespace genomes::destruction {

enum class InvalidationConsumer : std::uint8_t {
    Navigation = 0,
    StaticQuery = 1,
    RenderRepresentation = 2,
    Physics = 3,
};

struct DestructionInvalidationConfig final {
    world::WorldCoordinateConfig coordinates{};
    float coalesce_distance_m{0.5F};
    float max_expansion_m{64.0F};

    [[nodiscard]] bool valid() const noexcept {
        return coordinates.valid() && std::isfinite(coalesce_distance_m) &&
               coalesce_distance_m >= 0.0F && std::isfinite(max_expansion_m) &&
               max_expansion_m >= 0.0F;
    }
};

class DestructionInvalidationQueue final {
public:
    explicit DestructionInvalidationQueue(DestructionInvalidationConfig config = {}) noexcept
        : config_{config} {}

    [[nodiscard]] const DestructionInvalidationConfig& config() const noexcept { return config_; }

    // A queue may be attached to one resolved world revision. Zero means that
    // the queue is unbound (the legacy standalone behavior). Once bound, every
    // emitted event must carry the same semantic revision.
    void bindWorldRevision(std::uint64_t revision) noexcept { world_revision_ = revision; }
    void clearWorldRevision() noexcept { world_revision_ = 0U; }
    [[nodiscard]] std::uint64_t worldRevision() const noexcept { return world_revision_; }

    // Emits one semantic change and partitions it into all overlapping world regions.
    [[nodiscard]] bool emit(world::WorldId world_id,
                             foundation::StableId source_id,
                             std::uint64_t semantic_revision,
                             world::Aabb3 bounds,
                             world::DirtyReasonMask reasons,
                             std::int32_t layer = 0);

    // Accepts an already regionized event, useful when loading persisted dirty state.
    [[nodiscard]] bool emit(const world::DirtyBounds& event);

    [[nodiscard]] bool emitRubbleTile(world::WorldId world_id,
                                      foundation::StableId source_id,
                                      std::uint64_t semantic_revision,
                                      const RubbleField& field,
                                      TileCoord tile);

    // Coalescing is explicit so a consumer can inspect/retry the queue without losing data.
    void coalesce(InvalidationConsumer consumer);
    [[nodiscard]] std::vector<world::DirtyBounds> pending(
        InvalidationConsumer consumer) const;
    [[nodiscard]] std::size_t pendingCount(InvalidationConsumer consumer) const noexcept;

    // A consumer acknowledges only after publishing its derived representation. Until then
    // the dirty item remains available for retry.
    [[nodiscard]] bool acknowledge(InvalidationConsumer consumer,
                                    const world::DirtyBounds& event);

private:
    static constexpr std::size_t consumer_count = 4;

    [[nodiscard]] static std::size_t index(InvalidationConsumer consumer) noexcept {
        return static_cast<std::size_t>(consumer);
    }
    void enqueue(const world::DirtyBounds& event);
    [[nodiscard]] bool canMerge(const world::DirtyBounds& left,
                                const world::DirtyBounds& right) const noexcept;
    [[nodiscard]] static bool sameEvent(const world::DirtyBounds& left,
                                        const world::DirtyBounds& right) noexcept;

    DestructionInvalidationConfig config_{};
    std::uint64_t world_revision_{0U};
    std::array<std::vector<world::DirtyBounds>, consumer_count> queues_{};
};

} // namespace genomes::destruction
