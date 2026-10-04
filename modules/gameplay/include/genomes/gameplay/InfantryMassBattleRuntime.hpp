#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/api/Api.hpp>
#include <genomes/foundation/PublishedSnapshotExchange.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/infantry/InfantrySimulation.hpp>
#include <genomes/infantry/InfantryUnitController.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#include <genomes/simulation/EntityStore.hpp>
#include <genomes/simulation/EntityController.hpp>
#include <genomes/simulation/SimulationSnapshot.hpp>
#include <genomes/world/WorldArtifactRevision.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

struct ResolvedWorldArtifacts;

struct InfantryMassBattleConfig final {
    std::uint64_t seed{0x1F4A77U};
    std::uint32_t map_size_m{2000U};
    std::uint32_t units_per_team{1000U};
    float fixed_step_seconds{1.0F / 60.0F};

    [[nodiscard]] bool valid() const noexcept {
        return seed != 0U && map_size_m >= 512U && map_size_m <= 4096U &&
               units_per_team > 0U && units_per_team <= 10000U &&
               fixed_step_seconds > 0.0F && fixed_step_seconds <= 0.25F;
    }
};

struct InfantryMassBattleRenderState final {
    simulation::EntityId entity{};
    infantry::Team team{infantry::Team::Blue};
    foundation::Vec3 position{};
    float heading{0.0F};
    float height{1.75F};
    infantry::AgentState state{infantry::AgentState::Advance};
    float animation_phase{0.0F};
    float animation_speed{1.0F};
    std::uint8_t animation_variant{0U};
};

struct InfantryMassBattleSnapshot final {
    std::uint64_t tick{0U};
    std::size_t total_units{0U};
    std::size_t blue_units{0U};
    std::size_t red_units{0U};
    std::uint64_t direction_changes{0U};
    float average_speed_mps{0.0F};
};

// Presentation-only data published alongside the authoritative simulation
// snapshot.  Consumers receive immutable consumer-shaped state rather than a
// view into the runtime's mutable ECS/update buffers.
struct InfantryMassBattlePresentationSnapshot final {
    foundation::SnapshotMetadata metadata{};
    std::vector<InfantryMassBattleRenderState> states;
};

using InfantryMassBattlePresentationSnapshotExchange =
    foundation::PublishedSnapshotExchange<InfantryMassBattlePresentationSnapshot>;

class InfantryMassBattleRuntime final : public api::SimulationFacade {
public:
    InfantryMassBattleRuntime(const InfantryMassBattleRuntime&) = delete;
    InfantryMassBattleRuntime& operator=(const InfantryMassBattleRuntime&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                                             foundation::Error>
    start(const InfantryMassBattleConfig& config = {}, jobs::JobSystem* jobs = nullptr);

    [[nodiscard]] bool fixedUpdate(const simulation::TickContext& context) noexcept;
    [[nodiscard]] bool advance(const simulation::TickContext& context) noexcept override {
        return fixedUpdate(context);
    }
    [[nodiscard]] api::CommandReceipt submit(api::CommandEnvelope command) override;
    [[nodiscard]] api::SnapshotView snapshotView() const noexcept override {
        return {foundation::SimulationTick{simulation_snapshot_.metadata.tick},
                simulation_snapshot_.metadata.scene_epoch,
                simulation_snapshot_.semantic_hash,
                api_snapshot_bytes_ ? std::span<const std::uint8_t>{*api_snapshot_bytes_}
                                    : std::span<const std::uint8_t>{},
                api_snapshot_bytes_};
    }
    [[nodiscard]] api::SnapshotView query(api::ApiId query_id,
                                          const api::EncodedValue& arguments) const override {
        if (query_id == foundation::stable_id("world.snapshot")) {
            return snapshotView();
        }
        if (query_id == foundation::stable_id("world.query") &&
            arguments.type == api::ValueType::String && arguments.valid()) {
            const auto key = arguments.asString();
            if (!key || !api_world_snapshot_values_) return {};
            const auto found = api_world_snapshot_values_->find(*key);
            if (found == api_world_snapshot_values_->end()) return {};
            auto result = std::make_shared<const std::vector<std::uint8_t>>(found->second);
            return {foundation::SimulationTick{simulation_snapshot_.metadata.tick},
                    simulation_snapshot_.metadata.scene_epoch,
                    simulation_snapshot_.semantic_hash,
                    std::span<const std::uint8_t>{*result}, std::move(result)};
        }
        return {};
    }

    void setSceneEpoch(std::uint64_t epoch) noexcept {
        // Scene epochs identify a lifetime, so they may advance but must never
        // move backwards when a stale lifecycle notification arrives.
        const std::uint64_t next_epoch = std::max(scene_epoch_, epoch);
        scene_epoch_ = next_epoch;
        simulation_snapshot_exchange_.rejectBeforeSceneEpoch(next_epoch);
        presentation_snapshot_exchange_.rejectBeforeSceneEpoch(next_epoch);
        if (presentation_snapshot_.metadata.tick != 0U &&
            presentation_snapshot_.metadata.scene_epoch < next_epoch) {
            presentation_snapshot_ = {};
        }
        if (simulation_snapshot_.metadata.tick != 0U &&
            simulation_snapshot_.metadata.scene_epoch < next_epoch) {
            simulation_snapshot_ = {};
            api_snapshot_bytes_.reset();
            api_world_snapshot_values_.reset();
        }
    }

    [[nodiscard]] const InfantryMassBattleConfig& config() const noexcept { return config_; }
    [[nodiscard]] const InfantryMassBattleSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] const std::vector<InfantryMassBattleRenderState>& renderStates() const noexcept {
        return presentation_snapshot_.states;
    }
    [[nodiscard]] const InfantryMassBattlePresentationSnapshot& presentationSnapshot() const noexcept {
        return presentation_snapshot_;
    }
    [[nodiscard]] InfantryMassBattlePresentationSnapshotExchange&
    presentationSnapshotExchange() noexcept {
        return presentation_snapshot_exchange_;
    }
    [[nodiscard]] const simulation::SimulationSnapshot& simulationSnapshot() const noexcept {
        return simulation_snapshot_;
    }
    [[nodiscard]] simulation::SimulationSnapshotExchange& simulationSnapshotExchange() noexcept {
        return simulation_snapshot_exchange_;
    }
    [[nodiscard]] const simulation::EntityStore& entities() const noexcept { return entities_; }
    [[nodiscard]] bool bindWorldArtifact(
        std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept;
    [[nodiscard]] world::WorldArtifactRevision worldArtifactRevision() const noexcept;

private:
    struct Unit final {
        simulation::EntityId entity{};
        infantry::Team team{infantry::Team::Blue};
        float base_speed_mps{1.0F};
        float animation_speed{1.0F};
        float animation_phase{0.0F};
        std::uint8_t animation_variant{0U};
        foundation::Vec3 goal{};
        std::uint64_t direction_changes{0U};
        std::uint64_t last_direction_epoch{0U};
    };

    struct UnitUpdate final {
        foundation::Vec3 position{};
        foundation::Vec3 velocity{};
        float heading{0.0F};
        float animation_phase{0.0F};
        foundation::Vec3 goal{};
        std::uint8_t animation_variant{0U};
        std::uint64_t direction_changes{0U};
        std::uint64_t last_direction_epoch{0U};
        bool valid{false};
    };

    explicit InfantryMassBattleRuntime(InfantryMassBattleConfig config,
                                       jobs::JobSystem* scheduler) noexcept
        : config_{config}, jobs_{scheduler} {}

    [[nodiscard]] foundation::Result<void, foundation::Error> initialize();
    [[nodiscard]] UnitUpdate updateUnit(const Unit&,
                                        const simulation::EntityReadView&,
                                        const simulation::TickContext&) const noexcept;
    [[nodiscard]] bool rebuildRenderStates(std::uint64_t tick) noexcept;
    [[nodiscard]] static float unitRandom01(std::uint64_t seed,
                                            std::uint64_t entity,
                                            std::uint64_t stream) noexcept;
    [[nodiscard]] static float wrappedAngle(float angle) noexcept;
    void applyApiCommands(foundation::SimulationTick tick) noexcept;

    InfantryMassBattleConfig config_{};
    simulation::EntityStore entities_{};
    infantry::InfantryUnitController infantry_controller_{};
    std::vector<Unit> units_;
    std::vector<simulation::EntityReadView> input_states_;
    std::vector<UnitUpdate> unit_updates_;
    InfantryMassBattlePresentationSnapshot presentation_snapshot_{};
    InfantryMassBattlePresentationSnapshotExchange presentation_snapshot_exchange_{};
    simulation::SimulationSnapshot simulation_snapshot_;
    simulation::SimulationSnapshotExchange simulation_snapshot_exchange_{};
    std::shared_ptr<const std::vector<std::uint8_t>> api_snapshot_bytes_;
    InfantryMassBattleSnapshot snapshot_{};
    std::uint64_t scene_epoch_{0U};
    api::CommandQueue api_commands_{};
    // Opaque module-owned values accepted through world.set.  The map keeps
    // stable key order so the published semantic hash is worker-independent.
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> api_world_values_;
    std::shared_ptr<const std::map<std::string, std::vector<std::uint8_t>, std::less<>>>
        api_world_snapshot_values_;
    jobs::JobSystem* jobs_{nullptr};
    std::shared_ptr<const ResolvedWorldArtifacts> world_artifact_;
};

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
