#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <memory>
#include <cstdint>
#include <span>
#include <unordered_map>

namespace genomes::world_render {

struct WorldMeshDrawRange final {
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
};

struct WorldMeshArtifact final {
    std::shared_ptr<const render::RenderMesh> mesh;
    std::unordered_map<foundation::StableId, WorldMeshDrawRange> part_draw_ranges;
};

// Compiles the semantic world plan into one presentation mesh.  The compiler
// reads the authoritative terrain and plan but does not mutate either; later
// versions can replace the box fallback with building/road/vegetation mesh
// compilers without changing BattlefieldScene or the renderer backend.
class WorldMeshCompiler final {
public:
    [[nodiscard]] static foundation::Result<WorldMeshArtifact, foundation::Error>
    compile(const world::WorldPlan&, const terrain::HeightField&,
            std::span<const buildings::BuildingGenerationResult> resolved_buildings);
};

} // namespace genomes::world_render
