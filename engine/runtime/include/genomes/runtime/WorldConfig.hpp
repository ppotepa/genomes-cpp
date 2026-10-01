#pragma once

#include <genomes/world/WorldPlan.hpp>

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>

namespace genomes::runtime {

// Runtime screens expose the same request object consumed by the world
// generator. Keeping one contract prevents UI defaults from drifting away
// from the procedural pipeline.
using WorldGenerationConfig = world::WorldGenerationRequest;

enum class WorldSeedMode : std::uint8_t {
    Explicit,
    Auto,
};

struct WorldSeedInput final {
    WorldSeedMode mode{WorldSeedMode::Explicit};
    proc::Seed explicit_seed{0x5EED2026ULL};

    [[nodiscard]] static WorldSeedInput automatic() noexcept {
        return {WorldSeedMode::Auto, 0U};
    }

    [[nodiscard]] static WorldSeedInput explicitValue(proc::Seed seed) noexcept {
        return {WorldSeedMode::Explicit, seed};
    }

    [[nodiscard]] foundation::Result<proc::Seed, foundation::Error> resolve(
        std::uint64_t auto_entropy) const noexcept;
};

} // namespace genomes::runtime
