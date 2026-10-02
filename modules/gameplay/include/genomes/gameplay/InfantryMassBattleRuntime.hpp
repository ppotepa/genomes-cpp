#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/infantry/InfantrySimulation.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#include <genomes/simulation/EntityStore.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#if GENOMES_HAS_INFANTRY

namespace genomes::gameplay {

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

class InfantryMassBattleRuntime final {
public:
    InfantryMassBattleRuntime(const InfantryMassBattleRuntime&) = delete;
    InfantryMassBattleRuntime& operator=(const InfantryMassBattleRuntime&) = delete;

    [[nodiscard]] static foundation::Result<std::unique_ptr<InfantryMassBattleRuntime>,
                                             foundation::Error>
    start(const InfantryMassBattleConfig& config = {}, jobs::JobSystem* jobs = nullptr);

    void fixedUpdate(const simulation::TickContext& context) noexcept;

    [[nodiscard]] const InfantryMassBattleConfig& config() const noexcept { return config_; }
    [[nodiscard]] const InfantryMassBattleSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] const std::vector<InfantryMassBattleRenderState>& renderStates() const noexcept {
        return render_states_;
    }
    [[nodiscard]] const simulation::EntityStore& entities() const noexcept { return entities_; }

private:
    struct Unit final {
        simulation::EntityId entity{};
        infantry::Team team{infantry::Team::Blue};
        float base_speed_mps{1.0F};
        float animation_speed{1.0F};
        float animation_phase{0.0F};
        std::uint8_t animation_variant{0U};
        std::uint64_t direction_changes{0U};
        std::uint64_t last_direction_epoch{0U};
    };

    explicit InfantryMassBattleRuntime(InfantryMassBattleConfig config,
                                       jobs::JobSystem*) noexcept
        : config_{config} {}

    [[nodiscard]] foundation::Result<void, foundation::Error> initialize();
    void updateUnit(Unit&, const simulation::TickContext&) noexcept;
    void rebuildRenderStates(std::uint64_t tick) noexcept;
    [[nodiscard]] static float unitRandom01(std::uint64_t seed,
                                            std::uint64_t entity,
                                            std::uint64_t stream) noexcept;
    [[nodiscard]] static float wrappedAngle(float angle) noexcept;

    InfantryMassBattleConfig config_{};
    simulation::EntityStore entities_{};
    std::vector<Unit> units_;
    std::vector<InfantryMassBattleRenderState> render_states_;
    InfantryMassBattleSnapshot snapshot_{};
};

} // namespace genomes::gameplay

#endif // GENOMES_HAS_INFANTRY
