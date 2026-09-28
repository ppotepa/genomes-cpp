#pragma once

#include <genomes/render/RenderTypes.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace genomes::render {

// A snapshot owns only immutable-after-publish presentation data. It contains
// no ECS pointers or backend handles, so a renderer may keep reading it after
// simulation has advanced to another tick.
struct PresentationSnapshot final {
    std::uint64_t frame_number{0};
    std::uint64_t simulation_tick{0};
    std::uint64_t previous_simulation_tick{0};
    double interpolation_alpha{0.0};
    foundation::StableId world_origin_id{0};
    std::uint64_t world_origin_revision{0};
    RenderCamera camera{};
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

    void clear() {
        frame_number = 0;
        simulation_tick = 0;
        previous_simulation_tick = 0;
        interpolation_alpha = 0.0;
        world_origin_id = 0;
        world_origin_revision = 0;
        camera = {};
        instances.clear();
        instance_prototypes.clear();
        skinned_prototypes.clear();
        skinned_palettes.clear();
        terrain_mesh.reset();
        world_mesh.reset();
        infantry_mesh.reset();
    }
};

} // namespace genomes::render
