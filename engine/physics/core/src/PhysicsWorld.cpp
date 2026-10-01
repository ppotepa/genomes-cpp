#include <genomes/physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace genomes::physics {

namespace {

[[nodiscard]] float physics_dot(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return physics_dot(value, value);
}

[[nodiscard]] foundation::Vec3 normalize(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(0.000001F, length_squared(value)));
    return multiply(value, 1.0F / length);
}

[[nodiscard]] float bounding_radius(const ShapeDesc& shape) noexcept {
    if (shape.kind == ShapeKind::Sphere) {
        return shape.radius;
    }
    return std::sqrt(length_squared(shape.half_extent));
}

} // namespace

bool ShapeDesc::valid() const noexcept {
    if (kind == ShapeKind::Sphere) {
        return std::isfinite(radius) && radius > 0.0F;
    }
    return std::isfinite(half_extent.x) && std::isfinite(half_extent.y) &&
           std::isfinite(half_extent.z) && half_extent.x > 0.0F && half_extent.y > 0.0F &&
           half_extent.z > 0.0F;
}

bool BodyDesc::valid() const noexcept {
    return shape.valid() && std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z) && std::isfinite(linear_velocity.x) &&
           std::isfinite(linear_velocity.y) && std::isfinite(linear_velocity.z) &&
           (type != BodyType::Dynamic || (std::isfinite(mass) && mass > 0.0F));
}

bool RaycastQuery::valid() const noexcept {
    return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z) &&
           std::isfinite(direction.x) && std::isfinite(direction.y) &&
           std::isfinite(direction.z) && length_squared(direction) > 0.000001F &&
           std::isfinite(max_distance) && max_distance > 0.0F;
}

SimplePhysicsWorld::SimplePhysicsWorld(foundation::Vec3 gravity) noexcept : gravity_{gravity} {}

foundation::Result<BodyHandle, foundation::Error> SimplePhysicsWorld::createBody(
    const BodyDesc& description) {
    if (!description.valid()) {
        return foundation::Result<BodyHandle, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid physics body description"});
    }

    std::uint32_t index = foundation::Handle<BodyTag>::InvalidIndex;
    for (std::uint32_t candidate = 0; candidate < slots_.size(); ++candidate) {
        if (!slots_[candidate].alive) {
            index = candidate;
            break;
        }
    }
    if (index == foundation::Handle<BodyTag>::InvalidIndex) {
        if (slots_.size() >= foundation::Handle<BodyTag>::InvalidIndex) {
            return foundation::Result<BodyHandle, foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange, "physics body handle capacity exhausted"});
        }
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back({});
    }

    Slot& slot = slots_[index];
    slot.alive = true;
    slot.state = {BodyHandle{index, slot.generation}, description.type, description.position,
                  description.linear_velocity, description.shape, description.mass,
                  description.collision_layer};
    ++live_body_count_;
    return foundation::Result<BodyHandle, foundation::Error>::success(slot.state.handle);
}

void SimplePhysicsWorld::destroyBody(BodyHandle body) noexcept {
    Slot* slot = get(body);
    if (slot == nullptr) {
        return;
    }
    slot->alive = false;
    slot->state = {};
    ++slot->generation;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
    --live_body_count_;
}

void SimplePhysicsWorld::apply(const PhysicsCommandBuffer& commands) noexcept {
    for (const PhysicsCommand& command : commands.commands()) {
        Slot* slot = get(command.body);
        if (slot == nullptr) {
            continue;
        }
        switch (command.kind) {
        case PhysicsCommandKind::DestroyBody:
            destroyBody(command.body);
            break;
        case PhysicsCommandKind::SetLinearVelocity:
            slot->state.linear_velocity = command.value;
            break;
        case PhysicsCommandKind::ApplyImpulse:
            if (slot->state.type == BodyType::Dynamic) {
                const float inverse_mass = 1.0F / std::max(0.0001F, slot->state.mass);
                slot->state.linear_velocity =
                    add(slot->state.linear_velocity, multiply(command.value, inverse_mass));
            }
            break;
        }
    }
}

void SimplePhysicsWorld::step(float dt) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0F) {
        return;
    }
    for (Slot& slot : slots_) {
        if (!slot.alive || slot.state.type != BodyType::Dynamic) {
            continue;
        }
        slot.state.linear_velocity = add(slot.state.linear_velocity, multiply(gravity_, dt));
        slot.state.position = add(slot.state.position, multiply(slot.state.linear_velocity, dt));
        const float radius = bounding_radius(slot.state.shape);
        float ground_height = 0.0F;
        if (ground_query_.valid()) {
            const float sampled = ground_query_.sample(ground_query_.context,
                                                       slot.state.position.x,
                                                       slot.state.position.z);
            if (std::isfinite(sampled)) {
                ground_height = sampled;
            }
        }
        if (slot.state.position.y < ground_height + radius) {
            slot.state.position.y = ground_height + radius;
            if (slot.state.linear_velocity.y < 0.0F) {
                slot.state.linear_velocity.y *= -0.25F;
            }
        }
    }
}

bool SimplePhysicsWorld::readBody(BodyHandle body, BodyState& output) const noexcept {
    const Slot* slot = get(body);
    if (slot == nullptr) {
        return false;
    }
    output = slot->state;
    return true;
}

bool SimplePhysicsWorld::raycast(const RaycastQuery& query, RaycastHit& output) const noexcept {
    if (!query.valid()) {
        return false;
    }
    const foundation::Vec3 direction = normalize(query.direction);
    float best_distance = query.max_distance;
    bool found = false;
    RaycastHit best{};
    if (ground_query_.valid() && direction.y < -0.000001F &&
        (query.collision_mask & 1u) != 0u) {
        const float approximate_t =
            std::max(0.0F, -query.origin.y / direction.y);
        if (approximate_t <= query.max_distance) {
            const float hit_x = query.origin.x + direction.x * approximate_t;
            const float hit_z = query.origin.z + direction.z * approximate_t;
            const float sampled = ground_query_.sample(ground_query_.context, hit_x, hit_z);
            if (std::isfinite(sampled)) {
                const float terrain_t = (query.origin.y - sampled) / -direction.y;
                if (terrain_t >= 0.0F && terrain_t <= best_distance) {
                    best_distance = terrain_t;
                    best.body = {};
                    best.distance = terrain_t;
                    best.point = add(query.origin, multiply(direction, terrain_t));
                    best.ground = true;
                    const float left = ground_query_.sample(
                        ground_query_.context, best.point.x - 0.5F, best.point.z);
                    const float right = ground_query_.sample(
                        ground_query_.context, best.point.x + 0.5F, best.point.z);
                    const float back = ground_query_.sample(
                        ground_query_.context, best.point.x, best.point.z - 0.5F);
                    const float front = ground_query_.sample(
                        ground_query_.context, best.point.x, best.point.z + 0.5F);
                    best.normal = normalize({left - right, 1.0F, back - front});
                    found = true;
                }
            }
        }
    }
    for (const Slot& slot : slots_) {
        if (!slot.alive || (slot.state.collision_layer & query.collision_mask) == 0) {
            continue;
        }
        const foundation::Vec3 to_center = {slot.state.position.x - query.origin.x,
                                            slot.state.position.y - query.origin.y,
                                            slot.state.position.z - query.origin.z};
        const float projected = physics_dot(to_center, direction);
        if (projected < 0.0F || projected > best_distance) {
            continue;
        }
        const foundation::Vec3 closest = add(query.origin, multiply(direction, projected));
        const foundation::Vec3 offset = {slot.state.position.x - closest.x,
                                         slot.state.position.y - closest.y,
                                         slot.state.position.z - closest.z};
        const float radius = bounding_radius(slot.state.shape);
        const float radius_squared = radius * radius;
        const float distance_squared = length_squared(offset);
        if (distance_squared > radius_squared) {
            continue;
        }
        const float penetration = std::sqrt(std::max(0.0F, radius_squared - distance_squared));
        const float hit_distance = std::max(0.0F, projected - penetration);
        if (!found || hit_distance < best_distance ||
            (hit_distance == best_distance && slot.state.handle.packed() < best.body.packed())) {
            best_distance = hit_distance;
            best.body = slot.state.handle;
            best.distance = hit_distance;
            best.point = add(query.origin, multiply(direction, hit_distance));
            best.ground = false;
            best.normal = normalize({best.point.x - slot.state.position.x,
                                     best.point.y - slot.state.position.y,
                                     best.point.z - slot.state.position.z});
            found = true;
        }
    }
    if (found) {
        output = best;
    }
    return found;
}

SimplePhysicsWorld::Slot* SimplePhysicsWorld::get(BodyHandle body) noexcept {
    if (!body.isValid() || body.index >= slots_.size()) {
        return nullptr;
    }
    Slot& slot = slots_[body.index];
    return slot.alive && slot.generation == body.generation ? &slot : nullptr;
}

const SimplePhysicsWorld::Slot* SimplePhysicsWorld::get(BodyHandle body) const noexcept {
    if (!body.isValid() || body.index >= slots_.size()) {
        return nullptr;
    }
    const Slot& slot = slots_[body.index];
    return slot.alive && slot.generation == body.generation ? &slot : nullptr;
}

} // namespace genomes::physics
