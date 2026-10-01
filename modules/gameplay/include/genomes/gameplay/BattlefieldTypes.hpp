#pragma once

#include <genomes/combat/TacticalAI.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace genomes::gameplay {

// Configuration and presentation-neutral state are shared by the production
// runtime and the legacy scenario facade.  Keeping these value types outside
// either owner prevents the facade from becoming the storage owner again.
struct BattlefieldScenarioConfig final {
    std::uint64_t seed{0xC0FFEEU};
    std::uint32_t map_size_m{25U};
    float fixed_step_seconds{1.0F / 60.0F};
    std::uint32_t max_ticks{240U};
    combat::TacticalAIProfile tactical_ai_profile{};

    [[nodiscard]] bool valid() const noexcept {
        return seed != 0U && map_size_m == 25U && fixed_step_seconds > 0.0F &&
               max_ticks > 0U && tactical_ai_profile.valid();
    }
};

struct BattlefieldScenarioSnapshot final {
    std::uint64_t tick{0U};
    std::uint32_t map_size_m{25U};
    std::size_t ecs_entities{0U};
    std::size_t spawned{0U};
    std::size_t perceived{0U};
    std::size_t intents{0U};
    std::size_t fired{0U};
    std::size_t active_projectiles{0U};
    std::uint64_t physics_steps{0U};
    std::size_t impacts{0U};
    std::size_t accepted_damage{0U};
    std::size_t deaths{0U};
    std::size_t alive_units{0U};
    float destruction_damage{0.0F};
    std::size_t destruction_holes{0U};
    bool complete{false};
    std::string error;
};

} // namespace genomes::gameplay
