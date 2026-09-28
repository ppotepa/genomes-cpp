#include <genomes/render/ColorSpace.hpp>

#include <cassert>
#include <cmath>

int main() {
    using genomes::render::linearToSrgb;
    using genomes::render::srgbToLinear;
    for (const float value : {0.0F, 0.018F, 0.18F, 0.5F, 1.0F}) {
        assert(std::abs(linearToSrgb(srgbToLinear(value)) - value) < 1.0e-5F);
    }
    const auto color = linearToSrgb(srgbToLinear({0.2F, 0.4F, 0.8F, 0.7F}));
    assert(std::abs(color.r - 0.2F) < 1.0e-5F);
    assert(std::abs(color.g - 0.4F) < 1.0e-5F);
    assert(std::abs(color.b - 0.8F) < 1.0e-5F);
    assert(color.a == 0.7F);
    return 0;
}
