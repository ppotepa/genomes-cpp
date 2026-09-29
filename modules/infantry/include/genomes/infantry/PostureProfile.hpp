#pragma once

#include <cstdint>

#include <genomes/infantry/BodyPhenotype.hpp>

namespace genomes::infantry {

struct PostureSample final {
    float crouch{0.0F};
    float hip_height{1.0F};
    float torso_pitch{0.0F};
    float knee_bend{0.0F};
    float ankle_pitch{0.0F};
    float arm_relax{0.0F};
    float pelvis_pitch{0.0F};
    float hip_y{0.0F};
    float hip_z{0.0F};
    float lower_pitch{0.0F};
    float upper_pitch{0.0F};
    float chest_pitch{0.0F};
    float stance_half{0.0F};
    float foot_z{0.0F};
    float foot_yaw{0.0F};
    float knee_half{0.0F};
};

struct GaitSample final {
    double cycle_m{0.0};
    float run{0.0F};
    float sprint{0.0F};
    float duty{0.0F};
    float lift_m{0.0F};
    float amplitude{0.0F};
    float cadence{0.0F};
};

struct FootSample final {
    float z{0.0F};
    float lift{0.0F};
    float plant{0.0F};
    float support{0.0F};
    float foot_pitch{0.0F};
    float toe_pitch{0.0F};
    float swing{0.0F};
    float recovery_z{0.0F};
    bool stance{false};
};

class PostureProfile final {
public:
    [[nodiscard]] static PostureSample sample(float crouch) noexcept;
    [[nodiscard]] static PostureSample sample(float crouch,
                                              const BodyPhenotype&) noexcept;
    [[nodiscard]] static float curve(float crouch, std::uint32_t column) noexcept;
    [[nodiscard]] static float speedLimit(float crouch, const BodyPhenotype&) noexcept;
    [[nodiscard]] static float depthAtSpeed(float speed, const BodyPhenotype&) noexcept;
    [[nodiscard]] static GaitSample gait(float crouch, float speed,
                                         const BodyPhenotype&) noexcept;
    [[nodiscard]] static FootSample sampleLowContact(double phase, float duty,
                                                     double cycle_distance,
                                                     float clearance,
                                                     float sprint = 0.0F) noexcept;
};

} // namespace genomes::infantry
