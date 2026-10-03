#pragma once

#include <genomes/foundation/PublishedSnapshotExchange.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstdint>
#include <vector>

namespace genomes::simulation {

struct SimulationSnapshotEntity final {
    EntityId entity{};
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float heading_radians{0.0F};
    std::uint32_t flags{0};
};

// Consumer-shaped authoritative state. It deliberately does not clone ECS
// storage or expose mutable component pointers.
struct SimulationSnapshot final {
    foundation::SnapshotMetadata metadata{};
    // Canonical consumer-facing hash used to compare serial/parallel commits;
    // it is not a replacement for the authoritative ECS state.
    std::uint64_t semantic_hash{0U};
    std::vector<SimulationSnapshotEntity> entities;

    void clear() {
        metadata = {};
        semantic_hash = 0U;
        entities.clear();
    }
};

using SimulationSnapshotExchange = foundation::PublishedSnapshotExchange<SimulationSnapshot>;

} // namespace genomes::simulation
