#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>

#include <cstdint>

namespace genomes::geometry {

struct BoxSpec final {
    foundation::Vec3 size{1.0F, 1.0F, 1.0F};
};

struct PlaneSpec final {
    float width{1.0F};
    float depth{1.0F};
    std::uint32_t segments_x{1U};
    std::uint32_t segments_z{1U};
};

struct UvSphereSpec final {
    float radius{1.0F};
    std::uint32_t segments{32U};
    std::uint32_t rings{16U};
};

struct CylinderSpec final {
    float radius{0.5F};
    float height{1.0F};
    std::uint32_t segments{32U};
    bool caps{true};
};

struct ConeSpec final {
    float radius{0.5F};
    float height{1.0F};
    std::uint32_t segments{32U};
    bool cap{true};
};

struct CapsuleSpec final {
    float radius{0.25F};
    float height{1.0F};
    std::uint32_t segments{32U};
    std::uint32_t hemisphere_rings{8U};
    std::uint32_t body_rings{1U};
};

struct GridSpec final {
    float width{1.0F};
    float depth{1.0F};
    std::uint32_t segments_x{1U};
    std::uint32_t segments_z{1U};
};

[[nodiscard]] MeshData makeBox(const BoxSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeBoxResult(const BoxSpec& spec = {});

[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makePlaneResult(const PlaneSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeUvSphereResult(const UvSphereSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeCylinderResult(const CylinderSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeConeResult(const ConeSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeCapsuleResult(const CapsuleSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeGridResult(const GridSpec& spec = {});

} // namespace genomes::geometry
