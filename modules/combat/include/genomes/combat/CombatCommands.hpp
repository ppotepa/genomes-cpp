#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>
#include <genomes/weapons/WeaponPoseTasks.hpp>

#include <cstdint>
#include <vector>

namespace genomes::combat {

enum class CombatPhase : std::uint8_t {
    Orders,
    Pose,
    Weapon,
    FireCommit,
    Ballistics,
    Impact,
    Damage,
    Presentation,
};

struct AimIntent final {
    simulation::EntityId source{};
    foundation::Vec3 world_target{};
    foundation::SimulationTick tick{};
    std::uint64_t sequence{0};
};

struct WeaponIntent final {
    simulation::EntityId source{};
    foundation::StableId weapon_id{0};
    bool trigger{false};
    foundation::SimulationTick tick{};
    std::uint64_t sequence{0};
};

struct FireRequest final {
    foundation::StableId source{0};
    foundation::StableId weapon_id{0};
    foundation::StableId ammunition_id{0};
    std::uint64_t shot_sequence{0};
    foundation::Vec3 origin{};
    foundation::Vec3 direction{0.0F, 0.0F, 1.0F};
    foundation::SimulationTick tick{};
    foundation::StableId seed{0};

    [[nodiscard]] bool valid() const noexcept;
};

class CombatCommandBuffer final {
public:
    void clear() noexcept { fire_intents_.clear(); aim_intents_.clear(); weapon_intents_.clear(); }
    void pushFireIntent(const weapons::FireIntent& intent) { fire_intents_.push_back(intent); }
    void pushAim(AimIntent intent) { aim_intents_.push_back(intent); }
    void pushWeapon(WeaponIntent intent) { weapon_intents_.push_back(intent); }

    [[nodiscard]] const std::vector<weapons::FireIntent>& fireIntents() const noexcept {
        return fire_intents_;
    }
    [[nodiscard]] const std::vector<AimIntent>& aimIntents() const noexcept { return aim_intents_; }
    [[nodiscard]] const std::vector<WeaponIntent>& weaponIntents() const noexcept {
        return weapon_intents_;
    }

private:
    std::vector<weapons::FireIntent> fire_intents_;
    std::vector<AimIntent> aim_intents_;
    std::vector<WeaponIntent> weapon_intents_;
};

} // namespace genomes::combat
