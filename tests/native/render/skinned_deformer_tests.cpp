#include <genomes/render/SkinnedDeformer.hpp>

#include <array>
#include <cassert>
#include <cmath>

namespace {

[[nodiscard]] bool near(float left, float right, float epsilon = 1.0e-5F) noexcept {
    return std::abs(left - right) <= epsilon;
}

[[nodiscard]] std::array<float, 16U> quarter_turn_z() noexcept {
    // Column-major matrix, matching RenderTypes and the renderer shaders.
    return {0.0F, 1.0F, 0.0F, 0.0F,
            -1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            2.0F, 3.0F, 4.0F, 1.0F};
}

} // namespace

int main() {
    genomes::render::SkinnedMeshPrototype prototype{};
    prototype.vertices.push_back({{1.0F, 0.0F, 0.0F},
                                  {1.0F, 0.0F, 0.0F},
                                  {},
                                  {},
                                  {0U, 0U, 0U, 0U},
                                  {1.0F, 0.0F, 0.0F, 0.0F},
                                  0U});
    prototype.indices = {0U};

    const auto matrix = quarter_turn_z();
    const auto result = genomes::render::deformSkinnedCPU(prototype, {&matrix, 1U});
    assert(result.vertices.size() == 1U);
    assert(near(result.vertices[0].position.x, 2.0F));
    assert(near(result.vertices[0].position.y, 4.0F));
    assert(near(result.vertices[0].position.z, 4.0F));
    assert(near(result.vertices[0].normal.x, 0.0F));
    assert(near(result.vertices[0].normal.y, 1.0F));
    assert(near(result.vertices[0].normal.z, 0.0F));

    // A zero-weight vertex remains valid and keeps its source normal.
    prototype.vertices[0].bone_weights = {};
    const auto fallback = genomes::render::deformSkinnedCPU(prototype, {&matrix, 1U});
    assert(near(fallback.vertices[0].position.x, 1.0F));
    assert(near(fallback.vertices[0].position.y, 0.0F));
    assert(near(fallback.vertices[0].normal.x, 1.0F));
    assert(near(fallback.vertices[0].normal.y, 0.0F));
    return 0;
}
