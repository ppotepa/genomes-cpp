#pragma once

#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

// Presentation consumers must not depend on which authoritative battlefield
// implementation produced a unit. This immutable, consumer-shaped state is
// the common boundary for tactical and mass-battle sessions.
struct BattlefieldUnitPresentation final {
    simulation::EntityId entity{};
    infantry::Team team{infantry::Team::Blue};
    foundation::Vec3 position{};
    float heading{0.0F};
    float height{1.75F};
    infantry::AgentState state{infantry::AgentState::Idle};
    foundation::StableId action{0U};
    float animation_phase{0.0F};
    float animation_speed{1.0F};
    std::uint8_t animation_variant{0U};
};

struct BattlefieldSessionPresentationSnapshot final {
    foundation::SnapshotMetadata metadata{};
    std::vector<BattlefieldUnitPresentation> states;
};

// Module-owned façade for a complete battlefield lifetime. It deliberately
// exposes only SimulationFacade, world binding and immutable presentation
// products; ECS and concrete runtime types stay inside gameplay.
class BattlefieldSession final : public api::SimulationFacade {
public:
    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>
    startTactical(const BattlefieldScenarioConfig& config, jobs::JobSystem* jobs,
                  BattlefieldExecutionMode execution_mode,
                  proc::ProceduralRuntime* procedural_runtime);
    [[nodiscard]] static foundation::Result<std::unique_ptr<BattlefieldSession>, foundation::Error>
    startMassBattle(const InfantryMassBattleConfig& config, jobs::JobSystem* jobs);

    [[nodiscard]] bool advance(const simulation::TickContext& context) noexcept override;
    [[nodiscard]] api::CommandReceipt submit(api::CommandEnvelope command) override;
    [[nodiscard]] api::SnapshotView snapshotView() const noexcept override;
    [[nodiscard]] api::SnapshotView query(api::ApiId query,
                                          const api::EncodedValue& arguments) const override;

    void setSceneEpoch(std::uint64_t epoch) noexcept;
    [[nodiscard]] bool bindWorldArtifact(
        std::shared_ptr<const ResolvedWorldArtifacts> artifact) noexcept;
    [[nodiscard]] world::WorldArtifactRevision worldArtifactRevision() const noexcept;
    [[nodiscard]] bool isMassBattle() const noexcept;
    [[nodiscard]] std::optional<InfantryMassBattleSnapshot> massBattleSnapshot() const noexcept;
    [[nodiscard]] const BattlefieldSessionPresentationSnapshot& presentationSnapshot() const noexcept {
        return presentation_snapshot_;
    }

private:
    using Runtime = std::variant<std::unique_ptr<BattlefieldRuntime>,
                                 std::unique_ptr<InfantryMassBattleRuntime>>;

    explicit BattlefieldSession(Runtime runtime) noexcept : runtime_(std::move(runtime)) {}
    void refreshPresentationSnapshot() noexcept;

    Runtime runtime_;
    BattlefieldSessionPresentationSnapshot presentation_snapshot_{};
};

} // namespace genomes::gameplay

#endif
