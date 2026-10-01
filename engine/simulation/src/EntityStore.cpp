#include <genomes/simulation/EntityStore.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace genomes::simulation {

namespace {

using PositionComponent = foundation::Vec3;
using VelocityComponent = foundation::Vec3;
using HeadingComponent = float;
using HealthComponent = float;
using FlagsComponent = std::uint32_t;
using TargetComponent = EntityId;

[[nodiscard]] ArchetypeKey makeEntityKey() {
    ArchetypeKey key{{foundation::stable_id("simulation.entity.position"),
                      foundation::stable_id("simulation.entity.velocity"),
                      foundation::stable_id("simulation.entity.heading"),
                      foundation::stable_id("simulation.entity.health"),
                      foundation::stable_id("simulation.entity.flags"),
                      foundation::stable_id("simulation.entity.target")}};
    std::sort(key.components.begin(), key.components.end());
    return key;
}

[[nodiscard]] bool finite_position(const foundation::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

EntityStore::EntityStore() {
    (void)ecs_.registerComponent(
        makeComponentType<PositionComponent>("simulation.entity.position"));
    (void)ecs_.registerComponent(
        makeComponentType<VelocityComponent>("simulation.entity.velocity"));
    (void)ecs_.registerComponent(
        makeComponentType<HeadingComponent>("simulation.entity.heading"));
    (void)ecs_.registerComponent(makeComponentType<HealthComponent>("simulation.entity.health"));
    (void)ecs_.registerComponent(makeComponentType<FlagsComponent>("simulation.entity.flags"));
    (void)ecs_.registerComponent(makeComponentType<TargetComponent>("simulation.entity.target"));
    (void)ecs_.freeze();
}

ComponentTypeId EntityStore::positionType() noexcept {
    return foundation::stable_id("simulation.entity.position");
}

ComponentTypeId EntityStore::velocityType() noexcept {
    return foundation::stable_id("simulation.entity.velocity");
}

ComponentTypeId EntityStore::headingType() noexcept {
    return foundation::stable_id("simulation.entity.heading");
}

ComponentTypeId EntityStore::healthType() noexcept {
    return foundation::stable_id("simulation.entity.health");
}

ComponentTypeId EntityStore::flagsType() noexcept {
    return foundation::stable_id("simulation.entity.flags");
}

ComponentTypeId EntityStore::targetType() noexcept {
    return foundation::stable_id("simulation.entity.target");
}

const ArchetypeKey& EntityStore::entityKey() noexcept {
    static const ArchetypeKey key = makeEntityKey();
    return key;
}

foundation::Result<EntityId, foundation::Error> EntityStore::create(const EntitySpawn& spawn) {
    if (!finite_position(spawn.position) || !finite_position(spawn.velocity) || !std::isfinite(spawn.heading) ||
        !std::isfinite(spawn.health) || spawn.health < 0.0F) {
        return foundation::Result<EntityId, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "entity position, velocity and health must be finite"});
    }

    const std::array<ComponentInit, 5> initial{{
        {positionType(), &spawn.position},
        {velocityType(), &spawn.velocity},
        {headingType(), &spawn.heading},
        {healthType(), &spawn.health},
        {flagsType(), &spawn.flags},
    }};
    // Target is part of every entity archetype and remains default constructed
    // as an invalid handle until perception selects a contact.
    return ecs_.create(entityKey(), initial);
}

void EntityStore::destroy(EntityId entity) noexcept {
    (void)ecs_.destroy(entity);
}

void EntityStore::clear() noexcept {
    ecs_.clear();
}

bool EntityStore::contains(EntityId entity) const noexcept {
    return ecs_.contains(entity);
}

std::size_t EntityStore::size() const noexcept {
    return ecs_.entityCount();
}

bool EntityStore::read(EntityId entity, EntityReadView& output) const noexcept {
    if (!contains(entity)) {
        return false;
    }
    const auto* position = ecs_.component<PositionComponent>(entity, positionType());
    const auto* velocity = ecs_.component<VelocityComponent>(entity, velocityType());
    const auto* heading = ecs_.component<HeadingComponent>(entity, headingType());
    const auto* health = ecs_.component<HealthComponent>(entity, healthType());
    const auto* flags = ecs_.component<FlagsComponent>(entity, flagsType());
    const auto* target = ecs_.component<TargetComponent>(entity, targetType());
    if (position == nullptr || velocity == nullptr || heading == nullptr || health == nullptr ||
        flags == nullptr || target == nullptr) {
        return false;
    }
    output = {entity, *position, *velocity, *heading, *health, *flags, *target};
    return true;
}

foundation::Vec3* EntityStore::position(EntityId entity) noexcept {
    return ecs_.component<PositionComponent>(entity, positionType());
}

foundation::Vec3* EntityStore::velocity(EntityId entity) noexcept {
    return ecs_.component<VelocityComponent>(entity, velocityType());
}

float* EntityStore::heading(EntityId entity) noexcept {
    return ecs_.component<HeadingComponent>(entity, headingType());
}

float* EntityStore::health(EntityId entity) noexcept {
    return ecs_.component<HealthComponent>(entity, healthType());
}

std::uint32_t* EntityStore::flags(EntityId entity) noexcept {
    return ecs_.component<FlagsComponent>(entity, flagsType());
}

EntityId* EntityStore::target(EntityId entity) noexcept {
    return ecs_.component<TargetComponent>(entity, targetType());
}

} // namespace genomes::simulation
