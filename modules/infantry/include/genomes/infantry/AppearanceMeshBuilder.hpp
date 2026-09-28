#pragma once

#include <genomes/geometry/MeshBuilder.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>

#include <array>
#include <span>

namespace genomes::infantry {

struct AppearanceVertexSpec final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint16_t material_region{0};
    std::span<const SkinInfluence> influences{};
};

struct NormalizedInfluences final {
    std::array<SkinInfluence, 4U> values{};
    std::uint8_t count{0};
};

[[nodiscard]] NormalizedInfluences normalizeTopFour(
    std::span<const SkinInfluence> candidates) noexcept;

class AppearanceMeshBuilder final {
public:
    using Ring = geometry::Ring;
    using VertexIndex = std::uint32_t;

    [[nodiscard]] VertexIndex appendVertex(const AppearanceVertexSpec& spec);
    [[nodiscard]] VertexIndex vertex(foundation::Vec3 position, foundation::Vec3 normal,
                                     foundation::Vec2 uv, foundation::Color color,
                                     std::array<SkinInfluence, 4U> influences,
                                     std::uint8_t influence_count,
                                     std::uint16_t material_region);
    void triangle(VertexIndex a, VertexIndex b, VertexIndex c);
    void appendTriangle(VertexIndex a, VertexIndex b, VertexIndex c,
                        geometry::Winding winding = geometry::Winding::CounterClockwise);
    [[nodiscard]] Ring appendRing(std::span<const AppearanceVertexSpec> specs);
    void bridge(const Ring& first, const Ring& second,
                geometry::BridgeOptions options = {});

    [[nodiscard]] AppearanceMesh finalize() &&;

    [[nodiscard]] AppearanceMesh& mesh() noexcept { return mesh_; }
    [[nodiscard]] const AppearanceMesh& mesh() const noexcept { return mesh_; }

private:
    geometry::MeshBuilder topology_;
    AppearanceMesh mesh_;
};

} // namespace genomes::infantry
