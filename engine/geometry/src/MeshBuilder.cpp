#include <genomes/geometry/MeshBuilder.hpp>

#include <algorithm>

namespace genomes::geometry {

MeshBuilder::VertexIndex MeshBuilder::appendPosition(foundation::Vec3 position) {
    const auto result = static_cast<VertexIndex>(positions_.size());
    positions_.push_back(position);
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
    indices_.clear();
}

} // namespace genomes::geometry
