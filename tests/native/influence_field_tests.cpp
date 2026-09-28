#include <genomes/combat/InfluenceField.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace {

using genomes::combat::InfluenceField;
using genomes::combat::InfluenceFieldSpec;
using genomes::combat::InfluenceSource;
using genomes::combat::InfluenceSourceType;
using genomes::foundation::SimulationTick;
using genomes::foundation::Vec2;

InfluenceSource source(std::uint64_t id, Vec2 position, float radius, float strength,
                       float confidence, std::uint64_t observed_tick,
                       InfluenceSourceType type = InfluenceSourceType::KnownContact) {
    return {id, position, {1.0F, 0.0F}, radius, strength, confidence,
            SimulationTick{observed_tick}, 0.0F, type, true};
}

void radialKernelAndDeterministicOrdering() {
    InfluenceFieldSpec spec{};
    spec.width = 5U;
    spec.height = 1U;
    spec.world_origin = {-2.5F, -0.5F};
    spec.evaluation_tick = SimulationTick{10U};

    const InfluenceSource left = source(2U, {-1.0F, 0.0F}, 1.0F, 0.5F, 1.0F, 10U);
    const InfluenceSource center = source(1U, {0.0F, 0.0F}, 2.0F, 1.0F, 1.0F, 10U);
    const std::vector<InfluenceSource> ordered{left, center};
    const std::vector<InfluenceSource> reversed{center, left};

    const auto first = InfluenceField::compute(spec, ordered);
    const auto second = InfluenceField::compute(spec, reversed);
    assert(first && second);
    assert(first.value().values().size() == 5U);
    assert(std::equal(first.value().values().begin(), first.value().values().end(),
                      second.value().values().begin(), second.value().values().end()));
    assert(std::abs(first.value().at(2U, 0U) - 1.0F) < 1.0e-6F);
    assert(std::abs(first.value().at(1U, 0U) - 1.0F) < 1.0e-6F);
    assert(std::abs(first.value().at(3U, 0U) - 0.5F) < 1.0e-6F);
    assert(first.value().at(0U, 0U) == 0.0F);
    assert(first.value().revision().accepted_source_count == 2U);
    assert(first.value().revision().source_snapshot_tick.value == 10U);
}

void confidenceDecayAndStalePolicy() {
    InfluenceFieldSpec spec{};
    spec.width = 1U;
    spec.height = 1U;
    spec.world_origin = {-0.5F, -0.5F};
    spec.evaluation_tick = SimulationTick{15U};
    spec.confidence_decay_per_tick = 0.9F;
    spec.max_source_age_ticks = 20U;
    const InfluenceSource contact = source(7U, {0.0F, 0.0F}, 10.0F, 2.0F, 1.0F, 5U);

    const auto decayed = InfluenceField::compute(spec, std::vector<InfluenceSource>{contact});
    assert(decayed);
    assert(std::abs(decayed.value().at(0U, 0U) - 2.0F * std::pow(0.9F, 10.0F)) < 1.0e-6F);

    spec.max_source_age_ticks = 9U;
    const auto stale = InfluenceField::compute(spec, std::vector<InfluenceSource>{contact});
    assert(stale);
    assert(stale.value().at(0U, 0U) == 0.0F);
    assert(stale.value().revision().accepted_source_count == 0U);
}

void knowledgeBoundaryAndLayerFilter() {
    InfluenceFieldSpec spec{};
    spec.width = 1U;
    spec.height = 1U;
    spec.world_origin = {-0.5F, -0.5F};
    spec.evaluation_tick = SimulationTick{4U};

    InfluenceSource unknown = source(11U, {0.0F, 0.0F}, 3.0F, 1.0F, 1.0F, 4U);
    unknown.known_to_faction = false;
    const auto rejected = InfluenceField::compute(spec, std::vector<InfluenceSource>{unknown});
    assert(!rejected);

    const InfluenceSource contact = source(12U, {0.0F, 0.0F}, 3.0F, 1.0F, 1.0F, 4U);
    const InfluenceSource order = source(13U, {0.0F, 0.0F}, 3.0F, 5.0F, 1.0F, 4U,
                                         InfluenceSourceType::Order);
    spec.layer_type = InfluenceSourceType::KnownContact;
    const auto layer = InfluenceField::compute(
        spec, std::vector<InfluenceSource>{order, contact});
    assert(layer);
    assert(layer.value().revision().accepted_source_count == 1U);
    assert(std::abs(layer.value().at(0U, 0U) - 1.0F) < 1.0e-6F);
}

void disabledAndSamplingContract() {
    InfluenceFieldSpec spec{};
    spec.width = 2U;
    spec.height = 2U;
    spec.enabled = false;
    spec.evaluation_tick = SimulationTick{2U};
    const InfluenceSource known = source(1U, {0.0F, 0.0F}, 3.0F, 1.0F, 1.0F, 2U);
    const auto disabled = InfluenceField::compute(spec, std::vector<InfluenceSource>{known});
    assert(disabled);
    for (const float value : disabled.value().values()) {
        assert(value == 0.0F);
    }
    assert(disabled.value().sample({0.25F, 0.25F}));
    assert(!disabled.value().sample({3.0F, 0.25F}));
}

} // namespace

int main() {
    radialKernelAndDeterministicOrdering();
    confidenceDecayAndStalePolicy();
    knowledgeBoundaryAndLayerFilter();
    disabledAndSamplingContract();
    return 0;
}
