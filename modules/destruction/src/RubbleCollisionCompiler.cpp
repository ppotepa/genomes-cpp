#include <genomes/destruction/DestructionPhysicsAdapter.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

foundation::Result<RubbleColliderRecipe, foundation::Error> compileRubbleTile(
    const RubbleField& field, TileCoord coordinate) {
    if (!field.spec().valid()) {
        return foundation::Result<RubbleColliderRecipe, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid rubble field specification"});
    }
    const auto iterator = field.tiles().find(coordinate);
    if (iterator == field.tiles().end()) {
        return foundation::Result<RubbleColliderRecipe, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "rubble tile is not present"});
    }

    const RubbleTile& tile = iterator->second;
    RubbleColliderRecipe recipe{};
    recipe.tile = coordinate;
    recipe.empty = tile.totalVolume() <= 1.0e-12;
    if (recipe.empty) {
        return foundation::Result<RubbleColliderRecipe, foundation::Error>::success(recipe);
    }

    const RubbleFieldSpec& spec = field.spec();
    float bottom = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::lowest();
    for (std::uint32_t z = 0; z < tile.cellsPerSide(); ++z) {
        for (std::uint32_t x = 0; x < tile.cellsPerSide(); ++x) {
            if (tile.cellVolume(x, z) <= 1.0e-12) {
                continue;
            }
            const auto global_x = static_cast<std::int64_t>(coordinate.x) *
                                      static_cast<std::int64_t>(spec.cells_per_tile) + x;
            const auto global_z = static_cast<std::int64_t>(coordinate.z) *
                                      static_cast<std::int64_t>(spec.cells_per_tile) + z;
            const float world_x = (static_cast<float>(global_x) + 0.5F) * spec.cell_size_m;
            const float world_z = (static_cast<float>(global_z) + 0.5F) * spec.cell_size_m;
            const float surface = field.surfaceHeightAt(world_x, world_z);
            if (!std::isfinite(surface)) {
                return foundation::Result<RubbleColliderRecipe, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "rubble surface is not finite"});
            }
            bottom = std::min(bottom, surface - field.pileHeightAt(world_x, world_z));
            top = std::max(top, surface);
        }
    }
    if (!(top >= bottom) || !std::isfinite(bottom) || !std::isfinite(top)) {
        recipe.empty = true;
        return foundation::Result<RubbleColliderRecipe, foundation::Error>::success(recipe);
    }

    const float height = std::max(0.05F, top - bottom);
    recipe.bottom_height = bottom;
    recipe.top_height = top;
    recipe.position = {static_cast<float>(coordinate.x) * spec.tile_size_m +
                           spec.tile_size_m * 0.5F,
                       bottom + height * 0.5F,
                       static_cast<float>(coordinate.z) * spec.tile_size_m +
                           spec.tile_size_m * 0.5F};
    recipe.shape = {physics::ShapeKind::Box,
                    {spec.tile_size_m * 0.5F, height * 0.5F, spec.tile_size_m * 0.5F},
                    0.0F};
    if (!finite(recipe.position) || !recipe.shape.valid()) {
        return foundation::Result<RubbleColliderRecipe, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid rubble collider recipe"});
    }
    return foundation::Result<RubbleColliderRecipe, foundation::Error>::success(recipe);
}

} // namespace genomes::destruction
