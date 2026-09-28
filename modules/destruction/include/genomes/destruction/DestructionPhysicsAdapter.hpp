#pragma once

#include <genomes/destruction/RubbleField.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/physics/PhysicsWorld.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t DestructionPhysicsAdapterVersion = 1;

struct HeroBodyRequest final {
    foundation::StableId debris_id{0};
    foundation::Vec3 position{};
    foundation::Vec3 linear_velocity{};
    foundation::Vec3 half_extent{0.25F, 0.25F, 0.25F};
    float mass_kg{1.0F};
    bool shape_representable{true};

    [[nodiscard]] bool valid() const noexcept;
};

struct RubbleColliderRecipe final {
    TileCoord tile{};
    physics::ShapeDesc shape{};
    foundation::Vec3 position{};
    float bottom_height{0.0F};
    float top_height{0.0F};
    bool empty{true};

    [[nodiscard]] bool valid() const noexcept;
};

[[nodiscard]] foundation::Result<RubbleColliderRecipe, foundation::Error>
compileRubbleTile(const RubbleField&, TileCoord);

enum class DestructionPhysicsCommandKind : std::uint8_t {
    CreateHero,
    ApplyImpact,
    BakeRubbleTile,
    RemoveHero,
};

struct DestructionPhysicsCommand final {
    DestructionPhysicsCommandKind kind{DestructionPhysicsCommandKind::CreateHero};
    HeroBodyRequest hero{};
    foundation::StableId impact_id{0};
    foundation::StableId debris_id{0};
    foundation::Vec3 impulse{};
    TileCoord tile{};
};

class DestructionPhysicsCommandBuffer final {
public:
    void clear() noexcept { commands_.clear(); }

    void createHero(HeroBodyRequest request) {
        commands_.push_back({DestructionPhysicsCommandKind::CreateHero, request, 0, 0, {}, {}});
    }

    void applyImpact(foundation::StableId impact_id,
                     foundation::StableId debris_id,
                     foundation::Vec3 impulse) {
        DestructionPhysicsCommand command{};
        command.kind = DestructionPhysicsCommandKind::ApplyImpact;
        command.impact_id = impact_id;
        command.debris_id = debris_id;
        command.impulse = impulse;
        commands_.push_back(command);
    }

    void bakeRubbleTile(TileCoord tile) {
        DestructionPhysicsCommand command{};
        command.kind = DestructionPhysicsCommandKind::BakeRubbleTile;
        command.tile = tile;
        commands_.push_back(command);
    }

    void removeHero(foundation::StableId debris_id) {
        DestructionPhysicsCommand command{};
        command.kind = DestructionPhysicsCommandKind::RemoveHero;
        command.debris_id = debris_id;
        commands_.push_back(command);
    }

    [[nodiscard]] const std::vector<DestructionPhysicsCommand>& commands() const noexcept {
        return commands_;
    }

private:
    std::vector<DestructionPhysicsCommand> commands_;
};

struct DestructionPhysicsSyncResult final {
    std::uint32_t heroes_created{0};
    std::uint32_t heroes_fallback{0};
    std::uint32_t impacts_applied{0};
    std::uint32_t rubble_rebuilt{0};
    std::uint32_t rubble_rebuild_failed{0};
    std::uint32_t bodies_retired{0};
    std::uint32_t invalid_commands{0};
    std::vector<foundation::StableId> fallback_debris;
    std::vector<TileCoord> failed_tiles;
};

class DestructionPhysicsAdapter final {
public:
    DestructionPhysicsAdapter(physics::PhysicsWorld& world, const RubbleField& rubble) noexcept
        : world_{&world}, rubble_{&rubble} {}

    [[nodiscard]] DestructionPhysicsSyncResult sync(
        const DestructionPhysicsCommandBuffer&) noexcept;

    [[nodiscard]] physics::BodyHandle heroBody(foundation::StableId) const noexcept;
    [[nodiscard]] physics::BodyHandle rubbleBody(TileCoord) const noexcept;
    [[nodiscard]] std::size_t heroBodyCount() const noexcept { return hero_bodies_.size(); }
    [[nodiscard]] std::size_t rubbleBodyCount() const noexcept { return rubble_bodies_.size(); }
    [[nodiscard]] bool impactWasApplied(foundation::StableId) const noexcept;

private:
    physics::PhysicsWorld* world_{nullptr};
    const RubbleField* rubble_{nullptr};
    std::map<foundation::StableId, physics::BodyHandle> hero_bodies_;
    std::map<TileCoord, physics::BodyHandle> rubble_bodies_;
    std::set<foundation::StableId> applied_impacts_;
};

} // namespace genomes::destruction
