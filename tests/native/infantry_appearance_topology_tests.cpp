#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>

namespace {
using genomes::foundation::Vec3;
using genomes::infantry::AppearanceMesh;

[[nodiscard]] Vec3 subtract(Vec3 a, Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
[[nodiscard]] float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] bool finite(Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

[[nodiscard]] Vec3 triangleNormal(const AppearanceMesh& mesh,
                                  std::uint32_t i0, std::uint32_t i1,
                                  std::uint32_t i2) {
    return cross(subtract(mesh.vertices[i1].position, mesh.vertices[i0].position),
                 subtract(mesh.vertices[i2].position, mesh.vertices[i0].position));
}

struct WindingReport final {
    std::uint32_t triangles{0};
    std::uint32_t degenerate{0};
    std::uint32_t opposite_to_vertex_normal{0};
    std::uint32_t nonfinite{0};
};

[[nodiscard]] WindingReport auditWinding(const AppearanceMesh& mesh) {
    WindingReport report{};
    for (const auto& vertex : mesh.vertices) {
        if (!finite(vertex.position) || !finite(vertex.normal)) {
            ++report.nonfinite;
        }
    }
    assert(mesh.indices.size() % 3U == 0U);
    report.triangles = static_cast<std::uint32_t>(mesh.indices.size() / 3U);
    for (std::size_t offset = 0U; offset < mesh.indices.size(); offset += 3U) {
        const auto i0 = mesh.indices[offset];
        const auto i1 = mesh.indices[offset + 1U];
        const auto i2 = mesh.indices[offset + 2U];
        assert(i0 < mesh.vertices.size() && i1 < mesh.vertices.size() &&
               i2 < mesh.vertices.size());
        const Vec3 normal = triangleNormal(mesh, i0, i1, i2);
        const float area2 = dot(normal, normal);
        if (!(area2 > 1.0e-12F) || !std::isfinite(area2)) {
            ++report.degenerate;
        }
        const Vec3 average{
            (mesh.vertices[i0].normal.x + mesh.vertices[i1].normal.x +
             mesh.vertices[i2].normal.x) / 3.0F,
            (mesh.vertices[i0].normal.y + mesh.vertices[i1].normal.y +
             mesh.vertices[i2].normal.y) / 3.0F,
            (mesh.vertices[i0].normal.z + mesh.vertices[i1].normal.z +
             mesh.vertices[i2].normal.z) / 3.0F};
        if (dot(normal, average) < -1.0e-6F) {
            ++report.opposite_to_vertex_normal;
        }
    }
    return report;
}
} // namespace

int main() {
    genomes::infantry::InfantryModelCompiler compiler;
    genomes::infantry::InfantryModelRequest request{};
    request.seed = 0x5EED2026U;
    request.uniform_color = genomes::infantry::kDefaultUniformColor;
    const auto model = compiler.compile(request);
    assert(model);
    const auto body = auditWinding(model.value().artifact->appearance.body);
    const auto hair = auditWinding(model.value().artifact->appearance.hair);
    assert(body.nonfinite == 0U && hair.nonfinite == 0U);
    assert(body.degenerate == 0U && hair.degenerate == 0U);
    assert(body.opposite_to_vertex_normal == 0U);
    assert(hair.opposite_to_vertex_normal == 0U);
    assert(model.value().artifact->appearance.has_eye_openings);
    assert(model.value().artifact->appearance.has_mouth_opening);
    const auto anatomy = genomes::infantry::FaceAnatomyEvaluator::resolve(
        model.value().artifact->phenotype);
    assert(anatomy);
    for (std::size_t index = 1U; index < anatomy.value().head_sections.size(); ++index) {
        assert(anatomy.value().head_sections[index].y >
               anatomy.value().head_sections[index - 1U].y + 1.0e-5F);
    }
    return 0;
}
