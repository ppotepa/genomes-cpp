#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/LocomotionState.hpp>
#include <genomes/infantry/PostureProfile.hpp>

namespace genomes::infantry {

struct LocomotionLimits final {
    float walk_speed_mps{2.0F};
    float run_speed_mps{4.0F};
    float sprint_speed_mps{5.0F};
    float walk_cycle_seconds{0.8F};
    float run_cycle_seconds{0.55F};
    float sprint_cycle_seconds{0.43F};
    float max_crouch{0.82F};

    [[nodiscard]] bool valid() const noexcept;
};

class LocomotionController final {
public:
    [[nodiscard]] static foundation::Result<LocomotionController, foundation::Error> create(
        const BodyPhenotype&);

    [[nodiscard]] const LocomotionLimits& limits() const noexcept { return limits_; }
    [[nodiscard]] const BodyPhenotype& body() const noexcept { return body_; }
    [[nodiscard]] double proneCycleMeters() const noexcept;
    [[nodiscard]] LocomotionState initialState() const noexcept;

    [[nodiscard]] foundation::Result<void, foundation::Error> setRequested(
        LocomotionState&, const LocomotionRequest&) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setState(
        LocomotionState&, AnimationState, bool immediate,
        const AnimationTransitionProfile&) const noexcept;
    // The short overload retains the profile already stored in the unit state.
    [[nodiscard]] foundation::Result<void, foundation::Error> setState(
        LocomotionState&, AnimationState, bool immediate = false) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setMotion(
        LocomotionState&, const LocomotionMotionContext&) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setTransitionProfile(
        LocomotionState&, const AnimationTransitionProfile&) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setPreset(
        LocomotionState&, BipedPreset, bool immediate = false) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setFamily(
        LocomotionState&, LocomotionFamily, bool moving = false) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> step(
        LocomotionState&, float fixed_dt_seconds) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> step(
        LocomotionState&, const LocomotionMotionContext&, float fixed_dt_seconds) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> sampleGait(
        LocomotionState&, float actual_speed_mps, float fixed_dt_seconds,
        bool immediate = false) const noexcept;
    [[nodiscard]] PostureSample posture(const LocomotionState&) const noexcept;

private:
    explicit LocomotionController(LocomotionLimits limits, BodyPhenotype body) noexcept
        : limits_(limits), body_(body) {}

    LocomotionLimits limits_{};
    BodyPhenotype body_{};
};

} // namespace genomes::infantry
