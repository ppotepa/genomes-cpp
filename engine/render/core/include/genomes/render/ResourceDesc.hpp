#pragma once

#include <cstdint>

namespace genomes::render {

enum class ResourceUsage : std::uint32_t {
    None = 0,
    Vertex = 1u << 0u,
    Index = 1u << 1u,
    Uniform = 1u << 2u,
    Storage = 1u << 3u,
    TransferSource = 1u << 4u,
    TransferDestination = 1u << 5u,
    RenderTarget = 1u << 6u,
    DepthStencil = 1u << 7u,
};

[[nodiscard]] constexpr ResourceUsage operator|(ResourceUsage left,
                                                 ResourceUsage right) noexcept {
    return static_cast<ResourceUsage>(static_cast<std::uint32_t>(left) |
                                      static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool has_usage(ResourceUsage value, ResourceUsage required) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(required)) != 0;
}

struct BufferDesc final {
    std::uint64_t byte_size{0};
    ResourceUsage usage{ResourceUsage::None};
    bool cpu_writable{false};

    [[nodiscard]] bool valid() const noexcept {
        return byte_size > 0 && usage != ResourceUsage::None;
    }
};

struct TextureDesc final {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t mip_levels{1};
    std::uint32_t format{0};
    ResourceUsage usage{ResourceUsage::None};

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 && mip_levels > 0 && format != 0 &&
               usage != ResourceUsage::None;
    }
};

} // namespace genomes::render
