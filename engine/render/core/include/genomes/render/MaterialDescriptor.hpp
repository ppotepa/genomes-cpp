#pragma once
#include <genomes/foundation/Types.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace genomes::render {

enum class MaterialAlphaMode : std::uint8_t { Opaque, Mask, Blend };

struct MaterialDescriptor final {
    foundation::StableId material_id{0};
    std::uint64_t revision{0};
    foundation::Color base_color{1.0F, 1.0F, 1.0F, 1.0F};
    float roughness{0.8F};
    float metalness{0.0F};
    float opacity{1.0F};
    float alpha_cutoff{0.5F};
    MaterialAlphaMode alpha_mode{MaterialAlphaMode::Opaque};
    bool double_sided{false};
    bool vertex_color{true};
    bool instance_tint{false};
    foundation::StableId base_color_texture{0};
    foundation::StableId normal_texture{0};
    foundation::StableId roughness_metalness_texture{0};

    [[nodiscard]] bool valid() const noexcept {
        return material_id != 0 && std::isfinite(base_color.r) && std::isfinite(base_color.g) &&
               std::isfinite(base_color.b) && std::isfinite(base_color.a) &&
               std::isfinite(roughness) && roughness >= 0.0F && roughness <= 1.0F &&
               std::isfinite(metalness) && metalness >= 0.0F && metalness <= 1.0F &&
               std::isfinite(opacity) && opacity >= 0.0F && opacity <= 1.0F &&
               std::isfinite(alpha_cutoff) && alpha_cutoff >= 0.0F && alpha_cutoff <= 1.0F;
    }
};

struct MeshMaterialGroup final {
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::uint16_t material_index{0};

    [[nodiscard]] bool valid(std::size_t total_indices, std::size_t material_count) const noexcept {
        return index_count > 0U && index_count % 3U == 0U &&
               static_cast<std::size_t>(first_index) + index_count <= total_indices &&
               material_index < material_count;
    }
};

} // namespace genomes::render
