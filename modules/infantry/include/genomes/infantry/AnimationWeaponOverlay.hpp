#pragma once

#include <genomes/foundation/Types.hpp>

#include <array>
#include <cstdint>
#include <cmath>

namespace genomes::infantry {

enum class AnimationHandOwner : std::uint8_t {
    Free,
    Primary,
    Support,
};

struct AnimationHandOverlayTask final {
    AnimationHandOwner owner{AnimationHandOwner::Free};
    foundation::Vec3 target{};
    float weight{0.0F};
    float curl{0.0F};
    bool attached{false};

    [[nodiscard]] bool valid() const noexcept {
        return std::isfinite(target.x) && std::isfinite(target.y) &&
               std::isfinite(target.z) && std::isfinite(weight) && weight >= 0.0F &&
               weight <= 1.0F && std::isfinite(curl) && curl >= 0.0F && curl <= 1.0F;
    }
};

// Targets are root-local metres by default. Scene adapters that receive
// world-space weapon tasks set targets_are_world and root_position. Keeping
// this type infantry-owned avoids a dependency from the animator to weapons.
struct AnimationWeaponOverlay final {
    std::uint64_t weapon_id{0U};
    AnimationHandOverlayTask primary{};
    AnimationHandOverlayTask support{};
    foundation::Vec3 root_position{};
    foundation::Vec3 aim_direction{0.0F, 0.0F, 1.0F};
    float readiness{0.0F};
    float recoil{0.0F};
    bool targets_are_world{false};

    [[nodiscard]] bool valid() const noexcept {
        const float aim_length = std::sqrt(aim_direction.x * aim_direction.x +
                                           aim_direction.y * aim_direction.y +
                                           aim_direction.z * aim_direction.z);
        return primary.valid() && support.valid() && std::isfinite(root_position.x) &&
               std::isfinite(root_position.y) && std::isfinite(root_position.z) &&
               std::isfinite(aim_direction.x) && std::isfinite(aim_direction.y) &&
               std::isfinite(aim_direction.z) && aim_length > 1.0e-6F &&
               std::isfinite(readiness) && readiness >= 0.0F && readiness <= 1.0F &&
               std::isfinite(recoil) && recoil >= 0.0F;
    }
};

} // namespace genomes::infantry
