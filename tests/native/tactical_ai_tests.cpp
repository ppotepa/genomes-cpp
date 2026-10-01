#include <genomes/combat/TacticalAI.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    using namespace genomes;
    const auto loaded_profile = combat::loadTacticalAIProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/tactical-ai.json");
    assert(loaded_profile);
    assert(loaded_profile.value().id == "tactical-ai-default");
    assert(loaded_profile.value().profile.observation_period_ticks == 12U);
    assert(loaded_profile.value().profile.memory_ticks == 150U);
    assert(loaded_profile.value().content.sources.size() == 1U);
    assert(loaded_profile.value().content.fingerprint != 0U);
    assert(loaded_profile.value().fingerprint.value != 0U);
    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    assert(rifle != nullptr);
    combat::TacticalAIProfile profile = loaded_profile.value().profile;
    combat::TacticalAISystem ai(profile);
    assert(ai.registerDefaults());

    simulation::EntityId self{1U, 1U};
    simulation::EntityId enemy{2U, 1U};
    std::vector<combat::VisibleTarget> visible{{enemy, {0.0F, 1.0F, 10.0F}, 100.0F, true}};
    combat::AIState state{};
    combat::TacticalAIEntity entity{self, {0.0F, 1.0F, 0.0F}, 0.0F, rifle->id, visible, &state};
    std::vector<combat::TacticalAIEntity> entities{entity};

    std::vector<combat::AIIntent> intents;
    for (std::uint64_t tick = 0U; tick < 24U; ++tick) {
        const auto result = ai.evaluate(entities, {tick});
        assert(result);
        for (const auto& intent : result.value()) {
            intents.push_back(intent);
        }
    }
    assert(intents.size() == 2U);
    assert(intents.front().target.has_value() && intents.front().trigger);
    assert(state.memory.valid && state.memory.target == enemy);

    visible.clear();
    entities.front().visible_targets = {};
    assert(entities.front().visible_targets.empty());
    for (std::uint64_t tick = 24U; tick <= 175U; ++tick) {
        assert(ai.evaluate(entities, {tick}));
    }
    assert(!state.memory.valid);
    assert(ai.evaluatedCount() == 15U);

    combat::AIState passive_state{};
    passive_state.model_id = 99U;
    std::vector<combat::TacticalAIEntity> passive_entities{
        {self, {0.0F, 1.0F, 0.0F}, 0.0F, rifle->id, {}, &passive_state}};
    const auto passive = ai.evaluate(passive_entities, {199U});
    assert(passive && passive.value().size() == 1U && passive.value().front().passive);
    return 0;
}
