#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/Seed.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <cstdint>

// World generation is an application concern.  The neutral runtime transports
// lifecycle and presentation only; this header is owned by the product scene
// layer and is the sole home of the configuration-screen seed contract.
namespace genomes::application {

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

} // namespace genomes::application
