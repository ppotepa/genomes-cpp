#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponState.hpp>

#include <cstdint>
#include <optional>

namespace genomes::weapons {

struct HandPoseTask final {
    HandOwnership owner{HandOwnership::Free};
    foundation::Vec3 target{};
    float weight{0.0F};
    float curl{0.0F};
    bool attached{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct WeaponPoseTasks final {
    WeaponId weapon_id{0};
    HandPoseTask primary{};
    HandPoseTask support{};
    foundation::Vec3 muzzle{};
    foundation::Vec3 aim_direction{0.0F, 0.0F, 1.0F};
    float readiness{0.0F};
    float recoil{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

struct FireIntent final {
    foundation::StableId entity{0};
    WeaponId weapon_id{0};
    foundation::StableId ammunition_id{0};
    std::uint64_t shot_sequence{0};
    foundation::Vec3 origin{};
    foundation::Vec3 direction{0.0F, 0.0F, 1.0F};
    foundation::SimulationTick tick{};

    [[nodiscard]] bool valid() const noexcept;
};

struct WeaponStepOutput final {
    WeaponPoseTasks pose{};
    std::optional<FireIntent> fire;
};

} // namespace genomes::weapons
