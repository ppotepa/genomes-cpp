#pragma once

#include <genomes/destruction/Material.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t MaterialAssemblyVersion = 1;

struct MaterialFrame final {
    foundation::Vec3 origin{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec3 tangent{1.0F, 0.0F, 0.0F};
    foundation::Vec3 bitangent{0.0F, 0.0F, 1.0F};

    [[nodiscard]] static constexpr MaterialFrame identity() noexcept { return {}; }
    [[nodiscard]] bool valid(float tolerance = 1.0e-4F) const noexcept;
    [[nodiscard]] foundation::Vec3 worldToMaterial(foundation::Vec3 point) const noexcept;
    [[nodiscard]] foundation::Vec3 materialToWorld(foundation::Vec3 point) const noexcept;
};

enum class LayerKind : std::uint8_t {
    Solid,
    Void,
};

struct Layer final {
    MaterialId material{};
    PhysicalSolidId physical_solid{};
    float start{0.0F};
    float end{0.0F};
    LayerKind kind{LayerKind::Solid};

    [[nodiscard]] bool isVoid() const noexcept { return kind == LayerKind::Void; }
    [[nodiscard]] float thickness() const noexcept { return end - start; }
};

struct TraversalSegment final {
    std::size_t layer_index{0};
    Layer layer{};
    float start{0.0F};
    float end{0.0F};
    bool reversed{false};
};

struct InterfaceGroup final {
    PhysicalSolidId physical_solid{};
    float start{0.0F};
    float end{0.0F};
    std::vector<std::size_t> layer_indices;
};

} // namespace genomes::destruction
