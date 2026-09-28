#pragma once

#include <genomes/render/ShaderAsset.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace genomes::render {

enum class PipelineResourceClass : std::uint8_t {
    Static,
    Mutable,
    Dynamic,
    Indexed,
};

struct VertexAttribute final {
    std::uint32_t location{0};
    std::uint32_t components{0};
    std::uint32_t byte_offset{0};
};

struct PipelineBinding final {
    std::string name;
    std::uint32_t set{0};
    std::uint32_t binding{0};
    PipelineResourceClass resource_class{PipelineResourceClass::Static};
};

struct PipelineAsset final {
    ShaderAssetKey vertex_shader{};
    ShaderAssetKey pixel_shader{};
    ShaderAssetKey compute_shader{};
    std::vector<VertexAttribute> vertex_layout;
    std::vector<PipelineBinding> bindings;
    std::uint32_t state_flags{0};

    [[nodiscard]] std::uint64_t key() const noexcept {
        std::uint64_t hash = 14695981039346656037ull;
        const auto append_number = [&hash](std::uint64_t value) noexcept {
            for (unsigned int shift = 0; shift < 64; shift += 8) {
                hash ^= (value >> shift) & 0xFFu;
                hash *= 1099511628211ull;
            }
        };
        append_number(vertex_shader.digest);
        append_number(pixel_shader.digest);
        append_number(compute_shader.digest);
        append_number(state_flags);
        for (const VertexAttribute& attribute : vertex_layout) {
            append_number(attribute.location);
            append_number(attribute.components);
            append_number(attribute.byte_offset);
        }
        std::vector<PipelineBinding> sorted = bindings;
        std::sort(sorted.begin(), sorted.end(), [](const PipelineBinding& left,
                                                   const PipelineBinding& right) {
            if (left.set != right.set) {
                return left.set < right.set;
            }
            if (left.binding != right.binding) {
                return left.binding < right.binding;
            }
            return left.name < right.name;
        });
        for (const PipelineBinding& binding : sorted) {
            append_number(foundation::stable_id(binding.name));
            append_number(binding.set);
            append_number(binding.binding);
            append_number(static_cast<std::uint64_t>(binding.resource_class));
        }
        return hash == 0 ? 1 : hash;
    }
};

} // namespace genomes::render
