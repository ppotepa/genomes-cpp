#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/physics/PhysicsWorld.hpp>

#include <cstdint>

namespace genomes::ballistics {

struct Segment final {
    foundation::Vec3 origin{};
    foundation::Vec3 end{};

    [[nodiscard]] bool valid() const noexcept;
};

struct TraceHit final {
    physics::BodyHandle body{};
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    float distance{0.0F};
    bool ground{false};
};

struct Projectile final {
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    foundation::Vec3 gravity{0.0F, -9.81F, 0.0F};
    float remaining_seconds{2.0F};
    std::uint32_t collision_mask{0xFFFF'FFFFu};

    [[nodiscard]] bool valid() const noexcept;
};

class BallisticsSolver final {
public:
    [[nodiscard]] static foundation::Result<TraceHit, foundation::Error> trace(
        const physics::PhysicsWorld&, const Segment&, std::uint32_t collision_mask);

    [[nodiscard]] static foundation::Result<Projectile, foundation::Error> advance(
        const Projectile&, float dt, const physics::PhysicsWorld&, TraceHit* hit = nullptr);
};

} // namespace genomes::ballistics
