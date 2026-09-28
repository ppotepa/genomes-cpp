#include <genomes/render/SkinnedDeformer.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <limits>

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

[[nodiscard]] std::array<float, 16U> identity() noexcept {
    return {1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
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
                                  17U});
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
    assert(result.vertices[0].material_region == 17U);

    // Morph deltas are applied before the skin matrix, preserving the same
    // contract used by the GPU vertex shader.
    prototype.morph_target_count = 1U;
    prototype.morphs[0].position_deltas = {{1.0F, 0.0F, 0.0F}};
    prototype.morphs[0].normal_deltas = {{0.0F, 1.0F, 0.0F}};
    const auto morph_identity = identity();
    const std::array<float, 1U> half_morph{0.5F};
    const auto morphed = genomes::render::deformSkinnedCPU(
        prototype, {&morph_identity, 1U}, half_morph);
    assert(near(morphed.vertices[0].position.x, 1.5F));
    assert(near(morphed.vertices[0].normal.x, 1.0F / std::sqrt(1.25F)));
    assert(near(morphed.vertices[0].normal.y, 0.5F / std::sqrt(1.25F)));

    // A two-bone blend is normalized defensively and remains finite.
    prototype.morph_target_count = 0U;
    prototype.vertices[0].bone_indices = {0U, 1U, 0U, 0U};
    prototype.vertices[0].bone_weights = {0.5F, 0.5F, 0.0F, 0.0F};
    auto translated = identity();
    translated[12] = 3.0F;
    const std::array<std::array<float, 16U>, 2U> two_bone_palette{identity(), translated};
    const auto blended = genomes::render::deformSkinnedCPU(prototype, two_bone_palette);
    assert(near(blended.vertices[0].position.x, 2.5F));

    // A zero-weight vertex remains valid and keeps its source normal.
    prototype.vertices[0].bone_weights = {};
    const auto fallback = genomes::render::deformSkinnedCPU(prototype, {&matrix, 1U});
    assert(near(fallback.vertices[0].position.x, 1.0F));
    assert(near(fallback.vertices[0].position.y, 0.0F));
    assert(near(fallback.vertices[0].normal.x, 1.0F));
    assert(near(fallback.vertices[0].normal.y, 0.0F));

    // Invalid and non-normalized influences must not produce NaNs or scale
    // the result. The CPU fallback follows the same defensive contract as
    // the generator and GPU path.
    prototype.vertices[0].bone_weights = {2.0F, 0.0F, 0.0F, 0.0F};
    const auto overweight = genomes::render::deformSkinnedCPU(prototype, {&matrix, 1U});
    assert(near(overweight.vertices[0].position.x, 2.0F));
    assert(near(overweight.vertices[0].position.y, 4.0F));
    prototype.vertices[0].bone_weights = {
        std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F, 0.0F};
    const auto invalid = genomes::render::deformSkinnedCPU(prototype, {&matrix, 1U});
    assert(near(invalid.vertices[0].position.x, 1.0F));
    assert(near(invalid.vertices[0].normal.x, 1.0F));
    return 0;
}
