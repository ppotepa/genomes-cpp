#include <genomes/render/SkinnedDeformer.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::render {

namespace {

[[nodiscard]] foundation::Vec3 transformPoint(const std::array<float, 16U>& matrix,
                                              foundation::Vec3 value) noexcept {
    return {matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z + matrix[12],
            matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z + matrix[13],
            matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z + matrix[14]};
}

[[nodiscard]] foundation::Vec3 transformVector(const std::array<float, 16U>& matrix,
                                                foundation::Vec3 value) noexcept {
    return {matrix[0] * value.x + matrix[4] * value.y + matrix[8] * value.z,
            matrix[1] * value.x + matrix[5] * value.y + matrix[9] * value.z,
            matrix[2] * value.x + matrix[6] * value.y + matrix[10] * value.z};
}

[[nodiscard]] foundation::Vec3 normalize(foundation::Vec3 value,
                                         foundation::Vec3 fallback) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (!(length_squared > 1.0e-12F) || !std::isfinite(length_squared)) {
        return fallback;
    }
    const float inverse_length = 1.0F / std::sqrt(length_squared);
    return {value.x * inverse_length, value.y * inverse_length, value.z * inverse_length};
}

} // namespace

RenderMesh deformSkinnedCPU(const SkinnedMeshPrototype& prototype,
                            std::span<const std::array<float, 16U>> palette,
                            std::span<const float> morph_weights) {
    RenderMesh result{};
    result.mesh_id = prototype.mesh_id;
    result.revision = prototype.revision;
    result.indices = prototype.indices;
    result.vertices.reserve(prototype.vertices.size());
    for (const auto& source : prototype.vertices) {
        RenderMeshVertex target{};
        target.normal = source.normal;
        target.uv = source.uv;
        target.color = source.color;
        target.material_region = source.material_region;
        foundation::Vec3 morphed_position = source.position;
        foundation::Vec3 morphed_normal = source.normal;
        const auto vertex_index = result.vertices.size();
        for (std::size_t morph = 0; morph < prototype.morph_target_count && morph < 4U;
             ++morph) {
            const float morph_weight = morph < morph_weights.size()
                                           ? morph_weights[morph]
                                           : prototype.morph_weights[morph];
            const auto& morph_target = prototype.morphs[morph];
            if (!std::isfinite(morph_weight) || morph_weight == 0.0F ||
                vertex_index >= morph_target.position_deltas.size()) {
                continue;
            }
            const auto& position_delta = morph_target.position_deltas[vertex_index];
            const auto& normal_delta = morph_target.normal_deltas[vertex_index];
            morphed_position.x += position_delta.x * morph_weight;
            morphed_position.y += position_delta.y * morph_weight;
            morphed_position.z += position_delta.z * morph_weight;
            morphed_normal.x += normal_delta.x * morph_weight;
            morphed_normal.y += normal_delta.y * morph_weight;
            morphed_normal.z += normal_delta.z * morph_weight;
        }
        foundation::Vec3 deformed_normal{};
        float total_weight = 0.0F;
        for (std::size_t i = 0; i < source.bone_weights.size(); ++i) {
            const auto index = source.bone_indices[i];
            const float weight = source.bone_weights[i];
            if (!(weight > 0.0F) || !std::isfinite(weight) || index >= palette.size()) {
                continue;
            }
            const auto& matrix = palette[index];
            const foundation::Vec3 transformed_position = transformPoint(matrix, morphed_position);
            const foundation::Vec3 transformed_normal = transformVector(matrix, morphed_normal);
            target.position.x += transformed_position.x * weight;
            target.position.y += transformed_position.y * weight;
            target.position.z += transformed_position.z * weight;
            deformed_normal.x += transformed_normal.x * weight;
            deformed_normal.y += transformed_normal.y * weight;
            deformed_normal.z += transformed_normal.z * weight;
            total_weight += weight;
        }
        if (total_weight <= 0.0F) {
            target.position = morphed_position;
            deformed_normal = morphed_normal;
        } else if (total_weight < 1.0F) {
            const float remainder = 1.0F - total_weight;
            target.position.x += morphed_position.x * remainder;
            target.position.y += morphed_position.y * remainder;
            target.position.z += morphed_position.z * remainder;
            deformed_normal.x += morphed_normal.x * remainder;
            deformed_normal.y += morphed_normal.y * remainder;
            deformed_normal.z += morphed_normal.z * remainder;
        } else if (total_weight > 1.0F) {
            const float inverse_total = 1.0F / total_weight;
            target.position.x *= inverse_total;
            target.position.y *= inverse_total;
            target.position.z *= inverse_total;
            deformed_normal.x *= inverse_total;
            deformed_normal.y *= inverse_total;
            deformed_normal.z *= inverse_total;
        }
        target.normal = normalize(deformed_normal, {0.0F, 1.0F, 0.0F});
        result.vertices.push_back(target);
    }
    return result;
}

} // namespace genomes::render
