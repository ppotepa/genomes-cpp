#include <genomes/infantry/PostureProfile.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

PostureSample PostureProfile::sample(float crouch) noexcept {
    const float value = std::clamp(std::isfinite(crouch) ? crouch : 0.0F, 0.0F, 1.0F);
    const float smooth = value * value * (3.0F - 2.0F * value);
    const float smooth2 = smooth * smooth;
    return {value,
            1.0F - 0.23F * smooth - 0.04F * smooth2,
            0.24F * smooth + 0.08F * smooth2,
            0.36F * smooth + 0.44F * smooth2,
            -0.10F * smooth - 0.12F * smooth2,
            smooth};
}

} // namespace genomes::infantry
