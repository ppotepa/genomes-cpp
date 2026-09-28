#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>
#include <genomes/simulation/WorldEcs.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace genomes::simulation {

enum EntityFlags : std::uint32_t {
    EntityNone = 0,
    EntityAlive = 1u << 0u,
    EntityVisible = 1u << 1u,
    EntityNeedsNavigation = 1u << 2u,
};

struct EntitySpawn final {
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float heading{0.0F};
    float health{100.0F};
    std::uint32_t flags{EntityAlive | EntityVisible};
};

struct EntityReadView final {
    EntityId id{};
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float heading{0.0F};
    float health{0.0F};
    std::uint32_t flags{EntityNone};
    EntityId target{};
};

// Hot simulation state is stored in parallel arrays. Cold domain data such as
// a genome, rig or verbose history stays in owning modules keyed by EntityId.
class EntityStore final {
public:
    EntityStore();

    [[nodiscard]] foundation::Result<EntityId, foundation::Error> create(
        const EntitySpawn&);
    void destroy(EntityId) noexcept;
    void clear() noexcept;

    [[nodiscard]] bool contains(EntityId) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool read(EntityId, EntityReadView&) const noexcept;
    [[nodiscard]] foundation::Vec3* position(EntityId) noexcept;
    [[nodiscard]] foundation::Vec3* velocity(EntityId) noexcept;
    [[nodiscard]] float* heading(EntityId) noexcept;
    [[nodiscard]] float* health(EntityId) noexcept;
    [[nodiscard]] std::uint32_t* flags(EntityId) noexcept;
    [[nodiscard]] EntityId* target(EntityId) noexcept;

    // Migration boundary: systems that need structural commands can commit to
    // the authoritative archetype world without knowing EntityStore internals.
    [[nodiscard]] WorldEcs& ecs() noexcept { return ecs_; }
    [[nodiscard]] const WorldEcs& ecs() const noexcept { return ecs_; }

    template <class Function>
    void forEachLive(Function&& function) const {
        ecs_.forEachEntity(std::forward<Function>(function));
    }

private:
    [[nodiscard]] static const ArchetypeKey& entityKey() noexcept;

    [[nodiscard]] static ComponentTypeId positionType() noexcept;
    [[nodiscard]] static ComponentTypeId velocityType() noexcept;
    [[nodiscard]] static ComponentTypeId headingType() noexcept;
    [[nodiscard]] static ComponentTypeId healthType() noexcept;
    [[nodiscard]] static ComponentTypeId flagsType() noexcept;
    [[nodiscard]] static ComponentTypeId targetType() noexcept;

    WorldEcs ecs_;
};

} // namespace genomes::simulation
