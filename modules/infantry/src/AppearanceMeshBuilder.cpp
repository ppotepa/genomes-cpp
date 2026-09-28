#include <genomes/infantry/AppearanceMeshBuilder.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::infantry {

NormalizedInfluences normalizeTopFour(std::span<const SkinInfluence> candidates) noexcept {
    NormalizedInfluences result{};
    std::array<SkinInfluence, 16U> combined{};
    std::size_t combined_count = 0U;
    for (const SkinInfluence candidate : candidates) {
        if (candidate.bone_index == kInvalidBoneIndex || !(candidate.weight > 0.0F) ||
            !std::isfinite(candidate.weight)) {
            continue;
        }
        auto duplicate = std::find_if(combined.begin(), combined.begin() + combined_count,
                                       [candidate](SkinInfluence value) {
                                           return value.bone_index == candidate.bone_index;
                                       });
        if (duplicate != combined.begin() + combined_count) {
            duplicate->weight += candidate.weight;
        } else if (combined_count < combined.size()) {
            combined[combined_count++] = candidate;
        }
    }
    if (combined_count == 0U) {
        result.values[0] = {0U, 1.0F};
        result.count = 1U;
        return result;
    }
    std::stable_sort(combined.begin(), combined.begin() + combined_count,
                     [](SkinInfluence left, SkinInfluence right) {
                         if (left.weight != right.weight) {
                             return left.weight > right.weight;
                         }
                         return left.bone_index < right.bone_index;
                     });
    result.count = static_cast<std::uint8_t>(std::min<std::size_t>(4U, combined_count));
    float total = 0.0F;
    for (std::size_t index = 0U; index < result.count; ++index) {
        result.values[index] = combined[index];
        total += result.values[index].weight;
    }
    if (!(total > 0.0F) || !std::isfinite(total)) {
        result = {};
        result.values[0] = {0U, 1.0F};
        result.count = 1U;
        return result;
    }
    for (std::size_t index = 0U; index < result.count; ++index) {
        result.values[index].weight /= total;
    }
    return result;
}

AppearanceMeshBuilder::VertexIndex
AppearanceMeshBuilder::appendVertex(const AppearanceVertexSpec& spec) {
    const NormalizedInfluences normalized = normalizeTopFour(spec.influences);
    const VertexIndex index = static_cast<VertexIndex>(mesh_.vertices.size());
    mesh_.vertices.push_back({spec.position, spec.normal, spec.uv, spec.color,
                              normalized.values, normalized.count, spec.material_region});
    (void)topology_.appendPosition(spec.position);
    if (mesh_.vertices.size() == 1U) {
        mesh_.minimum = spec.position;
        mesh_.maximum = spec.position;
    } else {
        mesh_.minimum.x = std::min(mesh_.minimum.x, spec.position.x);
        mesh_.minimum.y = std::min(mesh_.minimum.y, spec.position.y);
        mesh_.minimum.z = std::min(mesh_.minimum.z, spec.position.z);
        mesh_.maximum.x = std::max(mesh_.maximum.x, spec.position.x);
        mesh_.maximum.y = std::max(mesh_.maximum.y, spec.position.y);
        mesh_.maximum.z = std::max(mesh_.maximum.z, spec.position.z);
    }
    return index;
}

AppearanceMeshBuilder::VertexIndex AppearanceMeshBuilder::vertex(
    foundation::Vec3 position, foundation::Vec3 normal, foundation::Vec2 uv,
    foundation::Color color, std::array<SkinInfluence, 4U> influences,
    std::uint8_t influence_count, std::uint16_t material_region) {
    return appendVertex({position, normal, uv, color, material_region,
                         std::span<const SkinInfluence>(influences.data(), influence_count)});
}

void AppearanceMeshBuilder::triangle(VertexIndex a, VertexIndex b, VertexIndex c) {
    const auto& vertices = mesh_.vertices;
    const foundation::Vec3 edge_a{
        vertices[b].position.x - vertices[a].position.x,
        vertices[b].position.y - vertices[a].position.y,
        vertices[b].position.z - vertices[a].position.z};
    const foundation::Vec3 edge_b{
        vertices[c].position.x - vertices[a].position.x,
        vertices[c].position.y - vertices[a].position.y,
        vertices[c].position.z - vertices[a].position.z};
    const foundation::Vec3 face{
        edge_a.y * edge_b.z - edge_a.z * edge_b.y,
        edge_a.z * edge_b.x - edge_a.x * edge_b.z,
        edge_a.x * edge_b.y - edge_a.y * edge_b.x};
    const foundation::Vec3 average{
        (vertices[a].normal.x + vertices[b].normal.x + vertices[c].normal.x) / 3.0F,
        (vertices[a].normal.y + vertices[b].normal.y + vertices[c].normal.y) / 3.0F,
        (vertices[a].normal.z + vertices[b].normal.z + vertices[c].normal.z) / 3.0F};
    if (face.x * average.x + face.y * average.y + face.z * average.z < 0.0F) {
        std::swap(b, c);
    }
    appendTriangle(a, b, c);
}

void AppearanceMeshBuilder::appendTriangle(VertexIndex a, VertexIndex b, VertexIndex c,
                                           geometry::Winding winding) {
    topology_.appendTriangle(a, b, c, winding);
}

AppearanceMeshBuilder::Ring
AppearanceMeshBuilder::appendRing(std::span<const AppearanceVertexSpec> specs) {
    Ring result{};
    result.indices.reserve(specs.size());
    for (const auto& spec : specs) {
        result.indices.push_back(appendVertex(spec));
    }
    return result;
}

void AppearanceMeshBuilder::bridge(const Ring& first, const Ring& second,
                                   geometry::BridgeOptions options) {
    topology_.bridgeLoops(first.indices, second.indices, options);
}

AppearanceMesh AppearanceMeshBuilder::finalize() && {
    mesh_.indices = topology_.indices();
    return std::move(mesh_);
}

} // namespace genomes::infantry
