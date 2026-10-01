#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Handle.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::physics {

struct BodyTag;
using BodyHandle = foundation::Handle<BodyTag>;

enum class BodyType : std::uint8_t {
    Static,
    Dynamic,
    Kinematic,
};

enum class ShapeKind : std::uint8_t {
    Sphere,
    Box,
};

struct ShapeDesc final {
    ShapeKind kind{ShapeKind::Sphere};
    foundation::Vec3 half_extent{0.5F, 0.5F, 0.5F};
    float radius{0.5F};

    [[nodiscard]] bool valid() const noexcept;
};

struct BodyDesc final {
    BodyType type{BodyType::Dynamic};
    ShapeDesc shape{};
    foundation::Vec3 position{};
    foundation::Vec3 linear_velocity{};
    float mass{1.0F};
    std::uint32_t collision_layer{1};
    std::uint32_t collision_mask{0xFFFF'FFFFu};

    [[nodiscard]] bool valid() const noexcept;
};

struct BodyState final {
    BodyHandle handle{};
    BodyType type{BodyType::Static};
    foundation::Vec3 position{};
    foundation::Vec3 linear_velocity{};
    ShapeDesc shape{};
    float mass{1.0F};
    std::uint32_t collision_layer{1};
};

struct RaycastQuery final {
    foundation::Vec3 origin{};
    foundation::Vec3 direction{0.0F, -1.0F, 0.0F};
    float max_distance{1000.0F};
    std::uint32_t collision_mask{0xFFFF'FFFFu};

    [[nodiscard]] bool valid() const noexcept;
};

struct RaycastHit final {
    BodyHandle body{};
    float distance{0.0F};
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    bool ground{false};
};

using GroundHeightFunction = float (*)(void* context, float x, float z) noexcept;

struct GroundHeightQuery final {
    void* context{nullptr};
    GroundHeightFunction sample{nullptr};

    [[nodiscard]] bool valid() const noexcept { return context != nullptr && sample != nullptr; }
};

enum class PhysicsCommandKind : std::uint8_t {
    DestroyBody,
    SetLinearVelocity,
    ApplyImpulse,
};

struct PhysicsCommand final {
    PhysicsCommandKind kind{PhysicsCommandKind::DestroyBody};
    BodyHandle body{};
    foundation::Vec3 value{};
};

class PhysicsCommandBuffer final {
public:
    void clear() noexcept { commands_.clear(); }

    void destroy(BodyHandle body) {
        commands_.push_back({PhysicsCommandKind::DestroyBody, body, {}});
    }

    void setLinearVelocity(BodyHandle body, foundation::Vec3 velocity) {
        commands_.push_back({PhysicsCommandKind::SetLinearVelocity, body, velocity});
    }

    void applyImpulse(BodyHandle body, foundation::Vec3 impulse) {
        commands_.push_back({PhysicsCommandKind::ApplyImpulse, body, impulse});
    }

    [[nodiscard]] const std::vector<PhysicsCommand>& commands() const noexcept {
        return commands_;
    }

private:
    std::vector<PhysicsCommand> commands_;
};

class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    [[nodiscard]] virtual foundation::Result<BodyHandle, foundation::Error> createBody(
        const BodyDesc&) = 0;
    virtual void destroyBody(BodyHandle) noexcept = 0;
    virtual void apply(const PhysicsCommandBuffer&) noexcept = 0;
    virtual void step(float dt) noexcept = 0;

    [[nodiscard]] virtual bool readBody(BodyHandle, BodyState&) const noexcept = 0;
    [[nodiscard]] virtual bool raycast(const RaycastQuery&, RaycastHit&) const noexcept = 0;
};

// Deterministic CPU fallback used by headless runs and as the contract oracle
// for future Jolt adapters. It intentionally implements only the small subset
// needed by the first vertical slice; the public boundary is solver-agnostic.
class SimplePhysicsWorld final : public PhysicsWorld {
public:
    explicit SimplePhysicsWorld(foundation::Vec3 gravity = {0.0F, -9.81F, 0.0F}) noexcept;

    [[nodiscard]] foundation::Result<BodyHandle, foundation::Error> createBody(
        const BodyDesc&) override;
    void destroyBody(BodyHandle) noexcept override;
    void apply(const PhysicsCommandBuffer&) noexcept override;
    void step(float dt) noexcept override;
    [[nodiscard]] bool readBody(BodyHandle, BodyState&) const noexcept override;
    [[nodiscard]] bool raycast(const RaycastQuery&, RaycastHit&) const noexcept override;

    // Optional static ground provider.  The solver remains independent of the
    // terrain module; adapters can provide a height query or leave the
    // deterministic flat fallback at y=0.
    void setGroundHeightQuery(GroundHeightQuery query) noexcept { ground_query_ = query; }
    void bindWorldRevision(std::uint64_t revision) noexcept { world_revision_ = revision; }
    [[nodiscard]] std::uint64_t worldRevision() const noexcept { return world_revision_; }

    [[nodiscard]] std::size_t bodyCount() const noexcept { return live_body_count_; }

private:
    struct Slot final {
        BodyState state{};
        std::uint32_t generation{1};
        bool alive{false};
    };

    [[nodiscard]] Slot* get(BodyHandle) noexcept;
    [[nodiscard]] const Slot* get(BodyHandle) const noexcept;

    foundation::Vec3 gravity_{};
    GroundHeightQuery ground_query_{};
    std::uint64_t world_revision_{0U};
    std::vector<Slot> slots_;
    std::size_t live_body_count_{0};
};

} // namespace genomes::physics
