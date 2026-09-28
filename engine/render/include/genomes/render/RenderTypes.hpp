#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/render/RenderCapabilities.hpp>
#include <genomes/ui/UiDocument.hpp>

#include <cstdint>
#include <cmath>
#include <memory>
#include <string_view>
#include <vector>
#include <array>

namespace genomes::render {

// Flags are renderer-facing presentation metadata, not gameplay state. They
// let one persistent GPU scene feed separate passes (for example menu
// preview versus dynamic battlefield actors) without teaching the renderer
// about domain modules.
inline constexpr std::uint32_t RenderInstanceFlagPreview = 1U << 1U;
inline constexpr std::uint32_t RenderInstanceFlagDynamic = 1U << 2U;
inline constexpr std::uint32_t RenderInstanceFlagTeamRed = 1U << 3U;

struct RenderInstance {
    foundation::StableId object_id{0};
    foundation::StableId mesh_id{0};
    foundation::StableId material_id{0};
    foundation::Vec3 position{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    // Zero means that the producer has no cheaper revision source and the
    // extractor may compare the small render record directly.
    std::uint64_t revision{0};
    std::uint32_t flags{0};

    [[nodiscard]] bool operator==(const RenderInstance& other) const noexcept {
        return object_id == other.object_id && mesh_id == other.mesh_id &&
               material_id == other.material_id && position.x == other.position.x &&
               position.y == other.position.y && position.z == other.position.z &&
               scale.x == other.scale.x && scale.y == other.scale.y &&
               scale.z == other.scale.z && rotation_y == other.rotation_y &&
               revision == other.revision && flags == other.flags;
    }
};

// GPU-facing mesh data is deliberately kept independent of the terrain
// module.  Procedural systems publish immutable render resources and the
// renderer owns the lifetime of their device buffers.
struct RenderMeshVertex {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct RenderMesh {
    foundation::StableId mesh_id{0};
    std::uint64_t revision{0};
    std::vector<RenderMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct SkinnedMeshVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<std::uint16_t, 4U> bone_indices{};
    std::array<float, 4U> bone_weights{};
    std::uint16_t material_region{0};
};

struct SkinnedMorphTarget final {
    std::vector<foundation::Vec3> position_deltas;
    std::vector<foundation::Vec3> normal_deltas;
};

struct SkinnedMeshPrototype final {
    foundation::StableId mesh_id{0};
    std::uint64_t revision{0};
    std::vector<SkinnedMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t morph_target_count{0};
    std::array<float, 4U> morph_weights{};
    std::array<SkinnedMorphTarget, 4U> morphs{};
};

struct SkinnedBonePalette final {
    foundation::StableId instance_id{0};
    std::vector<std::array<float, 16U>> matrices;
};

struct RenderCamera final {
    bool enabled{false};
    foundation::Vec3 position{0.0F, 20.0F, 20.0F};
    foundation::Vec3 target{};
    foundation::Vec3 up{0.0F, 1.0F, 0.0F};
    float vertical_fov{0.9F};
    float near_plane{0.1F};
    float far_plane{10'000.0F};

    [[nodiscard]] bool valid() const noexcept {
        const auto finite = [](foundation::Vec3 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                   std::isfinite(value.z);
        };
        const float dx = position.x - target.x;
        const float dy = position.y - target.y;
        const float dz = position.z - target.z;
        return finite(position) && finite(target) && finite(up) &&
               (dx * dx + dy * dy + dz * dz) > 0.000001F &&
               std::isfinite(vertical_fov) && std::isfinite(near_plane) &&
               std::isfinite(far_plane) && vertical_fov > 0.05F && vertical_fov < 3.0F &&
               near_plane > 0.0F && far_plane > near_plane;
    }
};

struct PresentationSnapshot;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void begin_frame() = 0;
    virtual void submit(const PresentationSnapshot&, const ui::UiDocument&) = 0;
    virtual void end_frame() = 0;
    [[nodiscard]] virtual RenderCapabilities capabilities() const noexcept { return {}; }
};

} // namespace genomes::render
