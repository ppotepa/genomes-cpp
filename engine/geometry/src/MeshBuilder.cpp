#include <genomes/geometry/MeshBuilder.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace genomes::geometry {

MeshBuilder::VertexIndex MeshBuilder::appendPosition(foundation::Vec3 position) {
    const auto result = static_cast<VertexIndex>(positions_.size());
    positions_.push_back(position);
    return result;
}

MeshBuilder::VertexIndex MeshBuilder::appendVertex(const MeshVertex& vertex) {
    const auto result = static_cast<VertexIndex>(vertices_.size());
    vertices_.push_back(vertex);
    positions_.push_back(vertex.position);
    return result;
}

void MeshBuilder::appendTriangle(VertexIndex a, VertexIndex b, VertexIndex c,
                                 Winding winding) {
    indices_.push_back(a);
    indices_.push_back(winding == Winding::CounterClockwise ? b : c);
    indices_.push_back(winding == Winding::CounterClockwise ? c : b);
}

void MeshBuilder::bridgeLoops(std::span<const VertexIndex> first,
                              std::span<const VertexIndex> second,
                              BridgeOptions options) {
    const std::size_t count = std::min(first.size(), second.size());
    if (count < 2U) {
        return;
    }
    const std::size_t edge_count = options.close_loop ? count : count - 1U;
    for (std::size_t index = 0U; index < edge_count; ++index) {
        const std::size_t next = (index + 1U) % count;
        if (options.flip) {
            appendTriangle(first[index], first[next], second[index]);
            appendTriangle(first[next], second[next], second[index]);
        } else {
            appendTriangle(first[index], second[index], first[next]);
            appendTriangle(first[next], second[index], second[next]);
        }
    }
}

void MeshBuilder::clear() noexcept {
    positions_.clear();
    vertices_.clear();
    indices_.clear();
}

foundation::Result<void, foundation::Error> MeshBuilder::appendMesh(const MeshData& mesh) {
    if (!mesh.valid()) return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, "cannot append invalid mesh"});
    if (mesh.vertices.empty()) return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported, "stream-only append requires an attribute adapter"});
    if (vertices_.size() > static_cast<std::size_t>(std::numeric_limits<VertexIndex>::max()) - mesh.vertices.size())
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "mesh vertex index overflow"});
    const auto vertex_count = vertices_.size();
    const auto index_count = indices_.size();
    vertices_.reserve(vertex_count + mesh.vertices.size());
    positions_.reserve(vertex_count + mesh.vertices.size());
    indices_.reserve(index_count + mesh.indices.size());
    for (const auto& vertex : mesh.vertices) appendVertex(vertex);
    for (const auto index : mesh.indices) indices_.push_back(static_cast<VertexIndex>(vertex_count) + index);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<MeshData, foundation::Error> MeshBuilder::build() const {
    if (!vertices_.empty() && vertices_.size() != positions_.size()) return foundation::Result<MeshData, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidState, "builder vertex streams are inconsistent"});
    MeshData result{};
    result.vertices = vertices_;
    if (result.vertices.empty()) {
        result.vertices.reserve(positions_.size());
        for (const auto position : positions_) result.vertices.push_back({position, {0.0F, 1.0F, 0.0F}, {}});
    }
    result.indices = indices_;
    result.rebuildStreams();
    if (!result.valid()) return foundation::Result<MeshData, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidState, "builder produced invalid mesh"});
    result.submeshes.push_back({0U, static_cast<std::uint32_t>(result.indices.size()), 0U});
    return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

} // namespace genomes::geometry
