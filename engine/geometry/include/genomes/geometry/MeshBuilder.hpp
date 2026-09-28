#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace genomes::geometry {

enum class Winding : std::uint8_t {
    CounterClockwise,
    Clockwise,
};

struct Ring final {
    std::vector<std::uint32_t> indices;
};

struct BridgeOptions final {
    bool close_loop{true};
    bool flip{false};
};

// This builder owns only positions and topology. Domain modules attach their
// own vertex payloads through a callback and therefore geometry stays unaware
// of bones, materials, render backends and anatomy.
class MeshBuilder final {
public:
    using VertexIndex = std::uint32_t;

    [[nodiscard]] VertexIndex appendPosition(foundation::Vec3 position);
    void appendTriangle(VertexIndex a, VertexIndex b, VertexIndex c,
                        Winding winding = Winding::CounterClockwise);
    void bridgeLoops(std::span<const VertexIndex> first,
                     std::span<const VertexIndex> second,
                     BridgeOptions options = {});

    [[nodiscard]] const std::vector<foundation::Vec3>& positions() const noexcept {
        return positions_;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept {
        return indices_;
    }
    void clear() noexcept;

private:
    std::vector<foundation::Vec3> positions_;
    std::vector<std::uint32_t> indices_;
};

} // namespace genomes::geometry
