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
    float requested_crouch{0.0F};
    float requested_speed_mps{0.0F};
    float target_crouch{0.0F};
    float actual_crouch{0.0F};
    float target_speed_mps{0.0F};
    float actual_speed_mps{0.0F};
    float run_weight{0.0F};
    float sprint_weight{0.0F};
    float phase{0.0F};
    LocomotionLimitReason limit_reason{LocomotionLimitReason::None};

    [[nodiscard]] bool valid() const noexcept;
};

struct LocomotionRequest final {
    std::optional<float> crouch;
    std::optional<float> speed_mps;
};

} // namespace genomes::infantry
