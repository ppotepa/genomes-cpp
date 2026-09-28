#include <genomes/combat/AIJobPipeline.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>
#include <atomic>
#include <vector>

namespace {

std::vector<genomes::combat::TacticalAIEntity> make_entities(
    std::size_t count, std::vector<genomes::combat::AIState>& states,
    std::vector<genomes::combat::VisibleTarget>& visible) {
    using namespace genomes;
    std::vector<combat::TacticalAIEntity> entities;
    entities.reserve(count);
    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    assert(rifle != nullptr);
    for (std::size_t index = 0U; index < count; ++index) {
        entities.push_back({{static_cast<std::uint32_t>(index + 1U), 1U},
                            {static_cast<float>(index), 1.0F, 0.0F}, 0.0F, rifle->id,
                            visible, &states[index]});
    }
    return entities;
}

} // namespace

int main() {
    using namespace genomes;
    constexpr std::size_t count = 128U;
    std::vector<combat::VisibleTarget> serial_visible{
        {{0xFFFFU, 1U}, {0.0F, 1.0F, 20.0F}, 400.0F, true}};
    std::vector<combat::VisibleTarget> parallel_visible = serial_visible;
    std::vector<combat::AIState> serial_states(count);
    std::vector<combat::AIState> parallel_states(count);
    auto serial_entities = make_entities(count, serial_states, serial_visible);
    auto parallel_entities = make_entities(count, parallel_states, parallel_visible);

    combat::TacticalAISystem serial_model;
    combat::TacticalAISystem parallel_model;
    assert(serial_model.registerDefaults());
    assert(parallel_model.registerDefaults());
    jobs::JobSystem jobs(2U);
    combat::AIJobPipeline pipeline;
    std::atomic_uint32_t broadphase_calls{0U};
    std::atomic_uint32_t los_calls{0U};
    std::atomic_uint32_t squad_calls{0U};
    combat::AIJobPipelineConfig parallel_config{32U, true};
    parallel_config.broadphase_stage = [&broadphase_calls](std::span<combat::TacticalAIEntity>,
                                                            foundation::SimulationTick) {
        broadphase_calls.fetch_add(1U, std::memory_order_relaxed);
        return foundation::Result<void, foundation::Error>::success();
    };
    parallel_config.line_of_sight_stage = [&los_calls](std::span<combat::TacticalAIEntity>,
                                                       foundation::SimulationTick) {
        los_calls.fetch_add(1U, std::memory_order_relaxed);
        return foundation::Result<void, foundation::Error>::success();
    };
    parallel_config.squad_stage = [&squad_calls](std::span<combat::TacticalAIEntity>,
                                                 foundation::SimulationTick) {
        squad_calls.fetch_add(1U, std::memory_order_relaxed);
        return foundation::Result<void, foundation::Error>::success();
    };
    const auto serial = pipeline.evaluate(jobs, serial_model, serial_entities, {7U},
                                          {32U, false});
    assert(serial);
    const auto parallel = pipeline.evaluate(jobs, parallel_model, parallel_entities, {7U},
                                            parallel_config);
    assert(parallel);
    assert(serial.value().size() == parallel.value().size());
    for (std::size_t index = 0U; index < serial.value().size(); ++index) {
        assert(serial.value()[index].self == parallel.value()[index].self);
        assert(serial.value()[index].target == parallel.value()[index].target);
        assert(serial.value()[index].trigger == parallel.value()[index].trigger);
        assert(serial.value()[index].tick == parallel.value()[index].tick);
    }
    assert(pipeline.stats().batch_count == 4U);
    assert(broadphase_calls == 4U && los_calls == 4U && squad_calls == 4U);
    std::atomic_bool canceled{true};
    const auto canceled_result = pipeline.evaluate(jobs, parallel_model, parallel_entities, {8U},
                                                   {32U, true}, &canceled);
    assert(!canceled_result && pipeline.stats().canceled);
    return 0;
}
