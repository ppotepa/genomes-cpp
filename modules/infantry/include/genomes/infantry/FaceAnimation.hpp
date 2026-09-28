#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/ExpressionProfile.hpp>
#include <genomes/infantry/FacePhenotype.hpp>
#include <genomes/proc/Seed.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace genomes::infantry {

struct FaceState final {
    proc::Seed unit_seed{0};
    std::array<float, kFaceExpressionCount> expression_weights{};
    std::array<float, kFaceChannelCount> channels{};
    foundation::Vec3 look_target{};
    foundation::Vec3 root_position{};
    foundation::Vec3 saccade_offset{};
    bool has_look_target{false};
    float blink_timer{0.0F};
    float next_blink_seconds{2.5F};
    float blink_time{-1.0F};
    std::uint64_t blink_event_index{0};
    float saccade_timer{0.0F};
    float next_saccade_seconds{0.65F};
    std::uint64_t saccade_event_index{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct FaceOutput final {
    std::array<float, kFaceChannelCount> channels{};
    float eyelids_close{0.0F};
    float eyelids_arc{0.0F};
    float jaw_rotation{0.0F};
    float head_yaw{0.0F};
    float head_pitch{0.0F};
    float eye_yaw{0.0F};
    float eye_pitch{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

class FaceAnimator final {
public:
    [[nodiscard]] static foundation::Result<FaceAnimator, foundation::Error> create(
        proc::Seed unit_seed, const FacePhenotype& identity);

    [[nodiscard]] const FacePhenotype& identity() const noexcept { return identity_; }
    [[nodiscard]] const FaceState& state() const noexcept { return state_; }
    [[nodiscard]] const FaceOutput& output() const noexcept { return output_; }

    [[nodiscard]] foundation::Result<void, foundation::Error> setExpression(
        FaceExpression, float weight) noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setLookTarget(
        std::optional<foundation::Vec3> world_target,
        foundation::Vec3 root_position) noexcept;
    void clearExpressions() noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> step(
        float fixed_dt_seconds) noexcept;

private:
    FaceAnimator(proc::Seed unit_seed, FacePhenotype identity) noexcept
        : identity_(identity) {
        state_.unit_seed = unit_seed;
        state_.expression_weights[static_cast<std::size_t>(FaceExpression::Neutral)] = 1.0F;
    }

    void updateBlink(float fixed_dt_seconds) noexcept;
    void updateSaccade(float fixed_dt_seconds) noexcept;
    void evaluateOutput(float fixed_dt_seconds) noexcept;

    FacePhenotype identity_{};
    FaceState state_{};
    FaceOutput output_{};
};

} // namespace genomes::infantry
