#include <genomes/destruction/DestructionPhysicsAdapter.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::uint8_t command_order(DestructionPhysicsCommandKind kind) noexcept {
    switch (kind) {
    case DestructionPhysicsCommandKind::CreateHero:
        return 0;
    case DestructionPhysicsCommandKind::ApplyImpact:
        return 1;
    case DestructionPhysicsCommandKind::BakeRubbleTile:
        return 2;
    case DestructionPhysicsCommandKind::RemoveHero:
        return 3;
    }
    return 255;
}

[[nodiscard]] physics::BodyDesc hero_body_desc(const HeroBodyRequest& request) noexcept {
    return {physics::BodyType::Dynamic,
            {physics::ShapeKind::Box, request.half_extent, 0.0F},
            request.position,
            request.linear_velocity,
            request.mass_kg,
            2U,
            0xFFFF'FFFFU};
}

} // namespace

bool HeroBodyRequest::valid() const noexcept {
    return debris_id != 0 && finite(position) && finite(linear_velocity) &&
           finite(half_extent) && half_extent.x > 0.0F && half_extent.y > 0.0F &&
           half_extent.z > 0.0F && std::isfinite(mass_kg) && mass_kg > 0.0F &&
           shape_representable;
}

bool RubbleColliderRecipe::valid() const noexcept {
    return !empty && std::isfinite(bottom_height) && std::isfinite(top_height) &&
           top_height >= bottom_height && finite(position) && shape.valid();
}

DestructionPhysicsSyncResult DestructionPhysicsAdapter::sync(
    const DestructionPhysicsCommandBuffer& command_buffer) noexcept {
    DestructionPhysicsSyncResult result{};
    if (world_ == nullptr || rubble_ == nullptr) {
        result.invalid_commands = static_cast<std::uint32_t>(command_buffer.commands().size());
        return result;
    }

    std::vector<DestructionPhysicsCommand> commands = command_buffer.commands();
    std::stable_sort(commands.begin(), commands.end(), [](const auto& left, const auto& right) {
        const std::uint8_t left_order = command_order(left.kind);
        const std::uint8_t right_order = command_order(right.kind);
        if (left_order != right_order) {
            return left_order < right_order;
        }
        if (left.kind == DestructionPhysicsCommandKind::CreateHero) {
            return left.hero.debris_id < right.hero.debris_id;
        }
        if (left.kind == DestructionPhysicsCommandKind::ApplyImpact) {
            return left.impact_id < right.impact_id;
        }
        if (left.kind == DestructionPhysicsCommandKind::BakeRubbleTile) {
            return left.tile < right.tile;
        }
        return left.debris_id < right.debris_id;
    });

    physics::PhysicsCommandBuffer backend_commands;
    std::vector<std::pair<TileCoord, physics::BodyHandle>> replacements;
    std::vector<TileCoord> replacement_tiles;
    std::vector<foundation::StableId> removals;

    for (const DestructionPhysicsCommand& command : commands) {
        if (command.kind != DestructionPhysicsCommandKind::CreateHero) {
            continue;
        }
        const HeroBodyRequest& request = command.hero;
        if (!request.valid() || hero_bodies_.contains(request.debris_id)) {
            ++result.heroes_fallback;
            result.fallback_debris.push_back(request.debris_id);
            continue;
        }
        const auto created = world_->createBody(hero_body_desc(request));
        if (!created) {
            ++result.heroes_fallback;
            result.fallback_debris.push_back(request.debris_id);
            continue;
        }
        hero_bodies_.emplace(request.debris_id, created.value());
        ++result.heroes_created;
    }

    for (const DestructionPhysicsCommand& command : commands) {
        if (command.kind != DestructionPhysicsCommandKind::ApplyImpact) {
            continue;
        }
        if (command.impact_id == 0 || !finite(command.impulse)) {
            ++result.invalid_commands;
            continue;
        }
        if (applied_impacts_.contains(command.impact_id)) {
            continue;
        }
        const auto body = hero_bodies_.find(command.debris_id);
        if (body == hero_bodies_.end()) {
            ++result.invalid_commands;
            continue;
        }
        backend_commands.applyImpulse(body->second, command.impulse);
        applied_impacts_.insert(command.impact_id);
        ++result.impacts_applied;
    }

    for (const DestructionPhysicsCommand& command : commands) {
        if (command.kind != DestructionPhysicsCommandKind::BakeRubbleTile ||
            std::binary_search(replacement_tiles.begin(), replacement_tiles.end(), command.tile)) {
            continue;
        }
        replacement_tiles.push_back(command.tile);
        const auto recipe = compileRubbleTile(*rubble_, command.tile);
        if (!recipe) {
            ++result.rubble_rebuild_failed;
            result.failed_tiles.push_back(command.tile);
            continue;
        }
        const auto old = rubble_bodies_.find(command.tile);
        if (recipe.value().empty) {
            if (old != rubble_bodies_.end()) {
                backend_commands.destroy(old->second);
                replacements.emplace_back(command.tile, physics::BodyHandle{});
            }
            continue;
        }
        const auto created = world_->createBody({physics::BodyType::Static,
                                                 recipe.value().shape,
                                                 recipe.value().position,
                                                 {},
                                                 0.0F,
                                                 4U,
                                                 0xFFFF'FFFFU});
        if (!created) {
            ++result.rubble_rebuild_failed;
            result.failed_tiles.push_back(command.tile);
            continue;
        }
        if (old != rubble_bodies_.end()) {
            backend_commands.destroy(old->second);
            ++result.bodies_retired;
        }
        replacements.emplace_back(command.tile, created.value());
        ++result.rubble_rebuilt;
    }

    for (const DestructionPhysicsCommand& command : commands) {
        if (command.kind != DestructionPhysicsCommandKind::RemoveHero) {
            continue;
        }
        if (const auto body = hero_bodies_.find(command.debris_id); body != hero_bodies_.end()) {
            backend_commands.destroy(body->second);
            removals.push_back(command.debris_id);
        } else {
            ++result.invalid_commands;
        }
    }

    world_->apply(backend_commands);
    for (const auto& [tile, body] : replacements) {
        if (body.isValid()) {
            rubble_bodies_[tile] = body;
        } else {
            rubble_bodies_.erase(tile);
        }
    }
    for (const foundation::StableId debris_id : removals) {
        hero_bodies_.erase(debris_id);
        ++result.bodies_retired;
    }
    std::sort(result.fallback_debris.begin(), result.fallback_debris.end());
    std::sort(result.failed_tiles.begin(), result.failed_tiles.end());
    return result;
}

physics::BodyHandle DestructionPhysicsAdapter::heroBody(foundation::StableId id) const noexcept {
    const auto iterator = hero_bodies_.find(id);
    return iterator == hero_bodies_.end() ? physics::BodyHandle{} : iterator->second;
}

physics::BodyHandle DestructionPhysicsAdapter::rubbleBody(TileCoord tile) const noexcept {
    const auto iterator = rubble_bodies_.find(tile);
    return iterator == rubble_bodies_.end() ? physics::BodyHandle{} : iterator->second;
}

bool DestructionPhysicsAdapter::impactWasApplied(foundation::StableId id) const noexcept {
    return applied_impacts_.contains(id);
}

} // namespace genomes::destruction
