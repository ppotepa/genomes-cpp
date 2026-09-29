#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace genomes::infantry {

enum class LocomotionFamily : std::uint8_t {
    Biped,
    Prone,
    Seated,
    Rest,
};

enum class BipedPreset : std::uint8_t {
    Idle,
    Walk,
    Run,
    Crouch,
    CrouchWalk,
};

enum class LocomotionLimitReason : std::uint8_t {
    None,
    RequestedCrouchClamped,
    RequestedSpeedClamped,
    FamilyDoesNotMove,
};

struct LocomotionState final {
    LocomotionFamily family{LocomotionFamily::Biped};
    BipedPreset preset{BipedPreset::Idle};
    bool family_moving{false};
    float requested_crouch{0.0F};
    float requested_speed_mps{0.0F};
    float target_crouch{0.0F};
    float actual_crouch{0.0F};
    float crouch_velocity{0.0F};
    float target_speed_mps{0.0F};
    float actual_speed_mps{0.0F};
    float run_weight{0.0F};
    float sprint_weight{0.0F};
    double cycle_m{0.0};
    float duty{0.62F};
    float lift_m{0.0F};
    float amplitude{0.0F};
    float cadence{0.0F};
    double cycle_velocity{0.0};
    float duty_velocity{0.0F};
    float run_velocity{0.0F};
    float sprint_velocity{0.0F};
    float lift_velocity{0.0F};
    double phase{0.0};
    LocomotionLimitReason limit_reason{LocomotionLimitReason::None};

    [[nodiscard]] bool valid() const noexcept;
};

struct LocomotionRequest final {
    std::optional<float> crouch;
    std::optional<float> speed_mps;
};

} // namespace genomes::infantry
