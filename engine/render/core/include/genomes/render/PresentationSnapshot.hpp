#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/camera/Camera.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace genomes::render {

// A snapshot owns only immutable-after-publish presentation data. It contains
// no ECS pointers or backend handles, so a renderer may keep reading it after
// simulation has advanced to another tick.
struct PresentationSnapshot final {
    std::uint64_t frame_number{0};
    std::uint64_t scene_epoch{0};
    std::uint64_t simulation_tick{0};
    std::uint64_t previous_simulation_tick{0};
    double interpolation_alpha{0.0};
    foundation::StableId world_origin_id{0};
    std::uint64_t world_origin_revision{0};
    RenderCamera camera{};
    camera::CameraRequest camera_request{};
    bool has_camera_request{false};
    camera::ResolvedCamera resolved_camera{};
    bool has_resolved_camera{false};
    CharacterLightRig character_lights{};
    std::vector<DebugLine> debug_lines;
    std::vector<RenderInstance> instances;
    // Immutable geometry prototypes referenced by RenderInstance::mesh_id.
    // The renderer may resolve these once and draw each mesh/material batch
    // without rebuilding a CPU mesh for every instance.
    std::vector<std::shared_ptr<const RenderMesh>> instance_prototypes;
    std::vector<std::shared_ptr<const SkinnedMeshPrototype>> skinned_prototypes;
    std::vector<SkinnedBonePalette> skinned_palettes;
    std::shared_ptr<const RenderMesh> terrain_mesh;
    std::shared_ptr<const RenderMesh> world_mesh;
    std::shared_ptr<const RenderMesh> infantry_mesh;

    void clear_scene_payload() {
        debug_lines.clear();
        instances.clear();
        instance_prototypes.clear();
        skinned_prototypes.clear();
        skinned_palettes.clear();
        terrain_mesh.reset();
        world_mesh.reset();
        infantry_mesh.reset();
        camera_request = {};
        has_camera_request = false;
    }

    // Full reset is owned by SceneDirector and is not a scene extraction API.
    void clear() {
        frame_number = 0;
        scene_epoch = 0;
        simulation_tick = 0;
        previous_simulation_tick = 0;
        interpolation_alpha = 0.0;
        world_origin_id = 0;
        world_origin_revision = 0;
        camera = {};
        camera_request = {};
        has_camera_request = false;
        resolved_camera = {};
        has_resolved_camera = false;
        character_lights = {};
        clear_scene_payload();
    }
};

} // namespace genomes::render
