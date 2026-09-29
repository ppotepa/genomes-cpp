#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/input/InputFrame.hpp>
#include <genomes/render/RenderCapabilities.hpp>
#include <genomes/render/MaterialDescriptor.hpp>
#include <genomes/render/SkeletonPrototype.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <array>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace genomes::render {

inline constexpr std::uint32_t RenderInstanceFlagPreview = 1U << 1U;
inline constexpr std::uint32_t RenderInstanceFlagDynamic = 1U << 2U;
inline constexpr std::uint32_t RenderInstanceFlagTeamRed = 1U << 3U;

struct DirectionalLight final {
    foundation::Vec3 direction{-0.35F, 0.80F, -0.25F};
    foundation::Color color{1.0F, 0.88F, 0.72F, 1.0F};
    float intensity{1.0F};
};
struct HemisphereLight final {
    foundation::Color sky{0.30F, 0.38F, 0.52F, 1.0F};
    foundation::Color ground{0.12F, 0.10F, 0.08F, 1.0F};
    float intensity{1.0F};
};
struct CharacterLightRig final {
    HemisphereLight hemisphere{};
    DirectionalLight key{};
    DirectionalLight fill{{0.55F, 0.25F, 0.65F}, {0.56F, 0.72F, 1.0F, 1.0F}, 0.35F};
};

struct RenderInstance {
    foundation::StableId object_id{0};
    foundation::StableId mesh_id{0};
    foundation::StableId material_id{0};
    foundation::Vec3 position{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    std::uint64_t revision{0};
    std::uint32_t flags{0};
    foundation::Color tint{1.0F, 1.0F, 1.0F, 1.0F};

    [[nodiscard]] bool operator==(const RenderInstance& other) const noexcept {
        return object_id == other.object_id && mesh_id == other.mesh_id &&
               material_id == other.material_id && position.x == other.position.x &&
               position.y == other.position.y && position.z == other.position.z &&
               scale.x == other.scale.x && scale.y == other.scale.y &&
               scale.z == other.scale.z && rotation_y == other.rotation_y &&
               revision == other.revision && flags == other.flags &&
               tint.r == other.tint.r && tint.g == other.tint.g &&
               tint.b == other.tint.b && tint.a == other.tint.a;
    }
};

// Colors in presentation buffers are linear. Backends perform display encoding
// once at the render-target boundary. This is a CPU record, not an implicit GPU
// layout: every input layout must use sizeof/offsetof, including unused fields.
struct RenderMeshVertex {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint16_t material_region{0};
};
struct RenderMesh {
    foundation::StableId mesh_id{0};
    std::uint64_t revision{0};
    std::vector<RenderMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MaterialDescriptor> materials;
    std::vector<MeshMaterialGroup> material_groups;
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

// One neutral local TRS contract is shared by the render skeleton, live pose
// palettes and both backends. Do not duplicate this structure: that made the
// threepp bind skeleton and live-pose path type-incompatible at compile time.
using SkinnedBoneTransform = BoneLocalTransform;
struct SkinnedBonePrototype final {
    std::uint16_t parent{0xffffU};
    SkinnedBoneTransform local_bind{};
};
struct SkinnedMeshPrototype final {
    foundation::StableId mesh_id{0};
    std::uint64_t revision{0};
    std::vector<SkinnedMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t morph_target_count{0};
    // Default weights are retained for CPU tools; live state belongs to palette.
    std::array<float, 4U> morph_weights{};
    std::array<SkinnedMorphTarget, 4U> morphs{};
    // Transitional compatibility for the legacy Diligent path.
    std::vector<SkinnedBonePrototype> bones;
    std::vector<MaterialDescriptor> materials;
    std::vector<MeshMaterialGroup> material_groups;
    std::shared_ptr<const RenderSkeletonPrototype> skeleton;
    // Shared local-space sphere large enough for bind geometry, all published
    // morph deltas and the procedural animation excursion budget. Backends may
    // use it for cheap per-instance frustum culling without CPU skinning.
    foundation::Vec3 conservative_bounds_center{};
    float conservative_bounds_radius{0.0F};
};
struct SkinnedBonePalette final {
    foundation::StableId instance_id{0};
    foundation::StableId skeleton_id{0};
    std::uint64_t pose_revision{0};
    std::vector<std::array<float, 16U>> matrices;
    // Same canonical order as SkinnedMeshPrototype::bones. Threepp and other
    // scene-graph renderers consume these directly; Diligent keeps using the
    // precomposed matrix palette until the legacy backend is retired.
    std::vector<SkinnedBoneTransform> local_poses;
    std::array<float, 4U> morph_weights{};
    // Presentation-only heatmap, -1 disables it. Never alters cached vertices.
    std::int32_t debug_weight_bone{-1};
};

struct RenderCamera final {
    bool enabled{false};
    foundation::Vec3 position{0.0F, 20.0F, 20.0F};
    foundation::Vec3 target{};
    foundation::Vec3 up{0.0F, 1.0F, 0.0F};
    float vertical_fov{0.9F};
    float near_plane{0.1F};
    float far_plane{10'000.0F};
    // Normalized framebuffer rectangle. Default preserves full-window scenes.
    float viewport_left{0.0F};
    float viewport_top{0.0F};
    float viewport_width{1.0F};
    float viewport_height{1.0F};
    // Interactive cameras are owned by the presentation backend between
    // explicit preset revisions. Domain scenes publish reset state only.
    bool interactive_orbit{false};
    std::uint64_t revision{0U};

    [[nodiscard]] bool valid() const noexcept {
        const auto finite = [](foundation::Vec3 v) noexcept {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        const float x = target.x - position.x;
        const float y = target.y - position.y;
        const float z = target.z - position.z;
        const float cx = y * up.z - z * up.y;
        const float cy = z * up.x - x * up.z;
        const float cz = x * up.y - y * up.x;
        return finite(position) && finite(target) && finite(up) &&
               x*x + y*y + z*z > 1.0e-6F && cx*cx + cy*cy + cz*cz > 1.0e-10F &&
               std::isfinite(vertical_fov) && vertical_fov > 0.05F && vertical_fov < 3.0F &&
               std::isfinite(near_plane) && near_plane > 0.0F &&
               std::isfinite(far_plane) && far_plane > near_plane &&
               std::isfinite(viewport_left) && std::isfinite(viewport_top) &&
               std::isfinite(viewport_width) && std::isfinite(viewport_height) &&
               viewport_left >= 0.0F && viewport_top >= 0.0F &&
               viewport_width > 0.0F && viewport_height > 0.0F &&
               viewport_left + viewport_width <= 1.00001F &&
               viewport_top + viewport_height <= 1.00001F;
    }
};

struct DebugLine final {
    foundation::Vec3 start{};
    foundation::Vec3 end{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct RenderUploadTelemetry final {
    std::uint64_t frame{0};
    std::uint64_t mesh_uploads{0};
    std::uint64_t mesh_upload_bytes{0};
    std::uint64_t palette_updates{0};
    std::uint64_t draw_calls{0};
    std::uint64_t ui_texture_uploads{0};
    std::uint64_t ui_texture_upload_bytes{0};
    std::uint64_t ui_buffer_grows{0};
    std::uint64_t ui_draw_calls{0};
    std::uint64_t total_mesh_uploads{0};
    std::uint64_t total_mesh_upload_bytes{0};
};

struct PresentationSnapshot;
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void begin_frame() = 0;
    virtual void submit(const PresentationSnapshot&, const ui::UiRenderFrame&) = 0;
    virtual void end_frame() = 0;
    virtual void handle_input(const input::InputFrame&) {}
    [[nodiscard]] virtual foundation::Result<void, foundation::Error> capture(
        const std::filesystem::path&) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Unsupported, "renderer capture is unsupported"});
    }
    [[nodiscard]] virtual RenderCapabilities capabilities() const noexcept { return {}; }
    [[nodiscard]] virtual RenderUploadTelemetry uploadTelemetry() const noexcept { return {}; }
    [[nodiscard]] virtual bool healthy() const noexcept { return true; }
    [[nodiscard]] virtual foundation::Error last_error() const noexcept { return {}; }
};

} // namespace genomes::render
