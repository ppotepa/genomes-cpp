#pragma once

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <cstdint>

namespace genomes::world {

// A resolved world is an atomic handoff: all of its terrain, semantic plan,
// building resolutions and presentation products must be derived from this
// revision. It derives from the canonical plan rather than a presentation
// cache key, so render, collision and navigation agree on one identity.
inline constexpr std::uint32_t WorldArtifactContractVersion = 1U;

using WorldArtifactRevision = foundation::StableId;

[[nodiscard]] inline WorldArtifactRevision artifactRevision(const WorldPlan& plan) noexcept {
    std::uint64_t value = foundation::stableHashU64(WorldArtifactContractVersion);
    value = foundation::stableHashCombine(value, plan.generator_version);
    value = foundation::stableHashCombine(value, plan.seed);
    value = foundation::stableHashCombine(value, plan.content_hash);
    return WorldArtifactRevision{value};
}

} // namespace genomes::world
