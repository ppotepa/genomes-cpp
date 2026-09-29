#pragma once

#include <cstdint>

namespace genomes::render {

enum class RenderBackendKind : std::uint8_t {
    Null,
    OpenGL,
    D3D12,
    Vulkan,
};

// Kept separate from Diligent feature structs. Values are frozen at backend
// creation and are safe for higher layers to cache for the lifetime of a
// renderer.
struct RenderFeatureLimits final {
    std::uint32_t max_texture_dimension{0};
    std::uint32_t max_bindless_resources{0};
    std::uint64_t uniform_buffer_alignment{1};
};

struct RenderCapabilities final {
    RenderBackendKind backend{RenderBackendKind::Null};
    bool initialized{false};
    bool headless{true};
    bool compute{false};
    bool presentation{false};
    bool resource_indexing{false};
    bool async_compute{false};
    bool indirect_draw{false};
    bool mesh_shaders{false};
    bool ray_tracing{false};
    bool timestamp_queries{false};
    bool instanced_rendering{false};
    bool gpu_skinning{false};
    std::uint32_t api_major{0};
    std::uint32_t api_minor{0};
    RenderFeatureLimits limits{};
};

} // namespace genomes::render
