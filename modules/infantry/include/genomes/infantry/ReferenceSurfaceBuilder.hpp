#pragma once

#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::infantry {

// Numeric builder used by the JS-parity surface port.  Geometry stays in
// Float64 height-relative space until finalization, matching JavaScript's
// number arrays followed by Float32 BufferAttributes.
struct ReferenceVec3 final {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct ReferenceColor final {
    double r{1.0};
    double g{1.0};
    double b{1.0};
};

struct ReferenceInfluence final {
    std::uint16_t bone_index{kInvalidBoneIndex};
    double weight{0.0};
};

class ReferenceSurfaceBuilder final {
public:
    using VertexIndex = std::uint32_t;
    using Ring = std::vector<VertexIndex>;

    ReferenceSurfaceBuilder(double height, std::size_t bone_count);
    void setOmitFacialMorphs(bool value) noexcept { omit_facial_morphs_=value; }

    void setTag(std::string_view name);
    [[nodiscard]] VertexIndex vertex(ReferenceVec3 position,
                                     std::span<const ReferenceInfluence> influences,
                                     ReferenceColor color,
                                     ReferenceVec3 hint,
                                     std::array<double, 2U> uv = {},
                                     std::uint16_t material_region = 0U);
    [[nodiscard]] ReferenceVec3 point(VertexIndex index) const noexcept;
    void triangle(VertexIndex a, VertexIndex b, VertexIndex c,
                  std::uint16_t material = 0U);
    void bridge(std::span<const VertexIndex> first,
                std::span<const VertexIndex> second,
                std::uint16_t material = 0U);
    void cap(std::span<const VertexIndex> loop,
             std::span<const ReferenceInfluence> influences,
             ReferenceColor color,
             ReferenceVec3 normal,
             std::uint16_t material = 0U,
             std::uint16_t material_region = 0U);
    void morph(std::string_view name, VertexIndex index, ReferenceVec3 delta);
    [[nodiscard]] std::vector<foundation::Vec3> morphPositions(std::string_view name) const;
    [[nodiscard]] std::vector<foundation::Vec3> morphNormals(
        std::string_view name, const AppearanceMesh& base) const;

    [[nodiscard]] AppearanceMesh finalize();
    [[nodiscard]] std::size_t vertexCount() const noexcept { return vertices_.size(); }

private:
    struct RawVertex final {
        ReferenceVec3 position{};
        ReferenceVec3 hint{};
        std::array<double, 2U> uv{};
        ReferenceColor color{};
        std::array<ReferenceInfluence, 4U> influences{};
        std::uint8_t influence_count{0U};
        std::uint16_t material_region{0U};
    };

    struct RawTag final {
        std::string name;
        std::vector<VertexIndex> vertices;
    };

    double height_{1.0};
    std::size_t bone_count_{0U};
    std::string current_tag_;
    std::vector<RawVertex> vertices_;
    std::array<std::vector<VertexIndex>, 3U> triangles_{};
    std::vector<RawTag> tags_;
    std::array<std::vector<std::optional<ReferenceVec3>>,4U> morphs_{};
    bool omit_facial_morphs_{false};
};

} // namespace genomes::infantry
