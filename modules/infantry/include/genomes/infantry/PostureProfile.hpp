#pragma once

#include <cstdint>

namespace genomes::infantry {

struct PostureSample final {
    float crouch{0.0F};
    float hip_height{1.0F};
    float torso_pitch{0.0F};
    float knee_bend{0.0F};
    float ankle_pitch{0.0F};
    float arm_relax{0.0F};
};

class PostureProfile final {
public:
    [[nodiscard]] static PostureSample sample(float crouch) noexcept;
};

} // namespace genomes::infantry
