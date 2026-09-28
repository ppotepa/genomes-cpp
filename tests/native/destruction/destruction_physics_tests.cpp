#include <genomes/destruction/DestructionPhysicsAdapter.hpp>

#include <cassert>
#include <cmath>
#include <utility>

int main() {
    using namespace genomes::destruction;
    using genomes::physics::SimplePhysicsWorld;

    const auto field_result = RubbleField::create();
    assert(field_result);
    RubbleField field = std::move(field_result.value());
    const MaterialId brick = MaterialId::fromName("brick");
    assert(field.deposit({7U, {0.0F, 0.0F, 0.0F}, 0.0F, {{brick, 2.0}}}));

    const auto recipe = compileRubbleTile(field, {0, 0});
    assert(recipe);
    assert(recipe.value().valid());
    assert(recipe.value().shape.kind == genomes::physics::ShapeKind::Box);
    assert(recipe.value().top_height > recipe.value().bottom_height);

    SimplePhysicsWorld physics;
    DestructionPhysicsAdapter adapter{physics, field};
    DestructionPhysicsCommandBuffer commands;
    commands.createHero({11U,
                         {0.0F, 2.0F, 0.0F},
                         {},
                         {0.5F, 0.5F, 0.5F},
                         2.0F,
                         true});
    commands.applyImpact(101U, 11U, {2.0F, 0.0F, 0.0F});
    commands.applyImpact(101U, 11U, {2.0F, 0.0F, 0.0F});
    commands.bakeRubbleTile({0, 0});
    const auto first = adapter.sync(commands);
    assert(first.heroes_created == 1U);
    assert(first.impacts_applied == 1U);
    assert(first.rubble_rebuilt == 1U);
    assert(first.invalid_commands == 0U);
    assert(adapter.heroBody(11U).isValid());
    assert(adapter.rubbleBody({0, 0}).isValid());
    assert(adapter.impactWasApplied(101U));
    assert(physics.bodyCount() == 2U);

    genomes::physics::BodyState hero_state{};
    assert(physics.readBody(adapter.heroBody(11U), hero_state));
    assert(std::abs(hero_state.linear_velocity.x - 1.0F) < 1.0e-6F);

    commands.clear();
    commands.applyImpact(101U, 11U, {2.0F, 0.0F, 0.0F});
    const auto duplicate = adapter.sync(commands);
    assert(duplicate.impacts_applied == 0U);
    assert(duplicate.invalid_commands == 0U);
    assert(physics.readBody(adapter.heroBody(11U), hero_state));
    assert(std::abs(hero_state.linear_velocity.x - 1.0F) < 1.0e-6F);

    const auto old_rubble = adapter.rubbleBody({0, 0});
    assert(field.deposit({8U, {0.5F, 0.0F, 0.0F}, 0.0F, {{brick, 0.5}}}));
    commands.clear();
    commands.bakeRubbleTile({0, 0});
    const auto replacement = adapter.sync(commands);
    assert(replacement.rubble_rebuilt == 1U);
    const auto new_rubble = adapter.rubbleBody({0, 0});
    assert(new_rubble.isValid());
    assert(new_rubble != old_rubble);
    genomes::physics::BodyState old_state{};
    assert(!physics.readBody(old_rubble, old_state));
    assert(physics.bodyCount() == 2U);

    commands.clear();
    commands.bakeRubbleTile({99, 99});
    const auto failed = adapter.sync(commands);
    assert(failed.rubble_rebuild_failed == 1U);
    assert(failed.failed_tiles.size() == 1U);
    assert(adapter.rubbleBody({0, 0}) == new_rubble);

    commands.clear();
    commands.createHero({12U, {}, {}, {}, 1.0F, false});
    const auto fallback = adapter.sync(commands);
    assert(fallback.heroes_created == 0U);
    assert(fallback.heroes_fallback == 1U);
    assert(fallback.fallback_debris.size() == 1U);

    commands.clear();
    commands.removeHero(11U);
    const auto removed = adapter.sync(commands);
    assert(removed.bodies_retired == 1U);
    assert(!adapter.heroBody(11U).isValid());
    assert(physics.bodyCount() == 1U);
    return 0;
}
