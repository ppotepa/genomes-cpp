#pragma once

#include <genomes/combat/CombatSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/jobs/ParallelFor.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/Cadence.hpp>
#include <genomes/spatial/SpatialGrid.hpp>
#include <genomes/weapons/WeaponController.hpp>

#include <cstdint>
#include <map>
#include <vector>

namespace genomes::infantry {

enum class Team : std::uint8_t {
    Blue,
    Red,
};

enum class AgentState : std::uint8_t {
    Idle,
    Advance,
    Engage,
    Dead,
};

struct InfantrySpawn final {
    Team team{Team::Blue};
    foundation::Vec3 position{};
    InfantryGenome genome{};
    std::uint32_t squad_id{0};
};

struct InfantryRenderState final {
    simulation::EntityId entity{};
    Team team{Team::Blue};
    foundation::Vec3 position{};
    float heading{0.0F};
    float height{1.75F};
    AgentState state{AgentState::Idle};
};

class InfantrySimulation final {
public:
    explicit InfantrySimulation(simulation::EntityStore& entities,
                                navigation::NavigationWorld* navigation = nullptr,
                                physics::PhysicsWorld* physics = nullptr,
                                jobs::JobSystem* jobs = nullptr) noexcept;

    [[nodiscard]] foundation::Result<simulation::EntityId, foundation::Error> spawn(
        const InfantrySpawn&);
    void remove(simulation::EntityId) noexcept;
    void fixedUpdate(double dt, foundation::SimulationTick tick) noexcept;
    void emitCombatEvents(foundation::SimulationTick tick, combat::DamageBuffer&) noexcept;

    [[nodiscard]] const std::vector<InfantryRenderState>& renderStates() const noexcept {
        return render_states_;
    }
    [[nodiscard]] std::size_t activeCount() const noexcept { return active_count_; }

private:
    struct Agent final {
        InfantryGenome genome{};
        Team team{Team::Blue};
        simulation::EntityId target{};
        physics::BodyHandle body{};
        simulation::CadenceState perception_cadence{};
        weapons::WeaponSpec weapon{};
        weapons::WeaponState weapon_state{};
        AgentState state{AgentState::Idle};
        bool active{false};
        std::vector<foundation::Vec3> route;
        std::size_t route_cursor{0};
        simulation::CadenceState path_cadence{};
        // Perception is sampled at a lower cadence than movement. Keep the
        // last authoritative contact so an agent can finish a route toward a
        // vanished target instead of snapping immediately to idle.
        foundation::Vec3 last_known_target_position{};
        foundation::SimulationTick last_contact_tick{};
        bool has_contact_memory{false};
        std::uint32_t squad_id{0};
    };

    struct SquadContact final {
        simulation::EntityId source{};
        foundation::Vec3 position{};
        foundation::SimulationTick last_seen{};
        bool valid{false};
    };

    void perceive(foundation::SimulationTick tick) noexcept;
    void perceiveRange(const std::vector<simulation::EntityId>& observers,
                       std::size_t begin,
                       std::size_t end,
                       foundation::SimulationTick tick) noexcept;
    void publishSquadContacts(foundation::SimulationTick tick) noexcept;
    void rebuildSpatialIndex() noexcept;
    void steer(double dt,
               foundation::SimulationTick tick,
               physics::PhysicsCommandBuffer*) noexcept;
    void syncPhysics() noexcept;
    void buildRenderStates() noexcept;
    [[nodiscard]] static bool contactMemoryFresh(const Agent&,
                                                 foundation::SimulationTick) noexcept;
    [[nodiscard]] Agent* agent(simulation::EntityId) noexcept;
    [[nodiscard]] const Agent* agent(simulation::EntityId) const noexcept;

    simulation::EntityStore& entities_;
    navigation::NavigationWorld* navigation_{nullptr};
    physics::PhysicsWorld* physics_{nullptr};
    spatial::UniformGrid spatial_index_{16.0F};
    jobs::JobSystem* jobs_{nullptr};
    std::vector<Agent> agents_;
    std::map<std::uint32_t, SquadContact> squad_contacts_;
    std::vector<InfantryRenderState> render_states_;
    std::size_t active_count_{0};
};

} // namespace genomes::infantry
