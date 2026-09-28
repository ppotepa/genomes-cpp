#pragma once

#include <cstdint>

namespace genomes::render::graph {

enum class GraphResourceKind : std::uint8_t {
    Texture,
    Buffer,
};

enum class GraphAccess : std::uint8_t {
    Read,
    Write,
    ReadWrite,
    ColorAttachment,
    DepthAttachment,
    Vertex,
    Index,
    Uniform,
    Storage,
};

struct GraphResourceHandle final {
    std::uint32_t index{0xFFFF'FFFFu};
    std::uint32_t generation{0};
    GraphResourceKind kind{GraphResourceKind::Buffer};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return index != 0xFFFF'FFFFu && generation != 0;
    }

    friend constexpr auto operator<=>(const GraphResourceHandle&,
                                      const GraphResourceHandle&) noexcept = default;
};

using GraphTexture = GraphResourceHandle;
using GraphBuffer = GraphResourceHandle;

struct GraphResourceDesc final {
    GraphResourceKind kind{GraphResourceKind::Buffer};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t format{0};
    std::uint64_t byte_size{0};
    bool imported{false};
    bool exported{false};

    [[nodiscard]] bool valid() const noexcept {
        if (kind == GraphResourceKind::Texture) {
            return width > 0 && height > 0;
        }
        return byte_size > 0;
    }

    [[nodiscard]] bool compatibleTransient(const GraphResourceDesc& other) const noexcept {
        return !imported && !other.imported && kind == other.kind && width == other.width &&
               height == other.height && format == other.format && byte_size == other.byte_size;
    }
};

struct GraphResourceUse final {
    GraphResourceHandle resource{};
    GraphAccess access{GraphAccess::Read};

    [[nodiscard]] constexpr bool writes() const noexcept {
        return access == GraphAccess::Write || access == GraphAccess::ReadWrite ||
               access == GraphAccess::ColorAttachment ||
               access == GraphAccess::DepthAttachment || access == GraphAccess::Storage;
    }

    [[nodiscard]] constexpr bool reads() const noexcept {
        return access == GraphAccess::Read || access == GraphAccess::ReadWrite ||
               access == GraphAccess::Vertex || access == GraphAccess::Index ||
               access == GraphAccess::Uniform || access == GraphAccess::Storage;
    }
};

struct GraphBarrier final {
    GraphResourceHandle resource{};
    GraphAccess before{GraphAccess::Read};
    GraphAccess after{GraphAccess::Read};
    std::uint32_t from_pass{0xFFFF'FFFFu};
    std::uint32_t to_pass{0xFFFF'FFFFu};
};

} // namespace genomes::render::graph
