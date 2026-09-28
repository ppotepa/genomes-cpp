#include <genomes/combat/CombatSystem.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#include <genomes/foundation/StableHash.hpp>

namespace genomes::combat {

CombatApplyResult CombatSystem::apply(DamageBuffer& buffer) noexcept {
    std::stable_sort(buffer.events_.begin(), buffer.events_.end(),
                     [](const DamageEvent& left, const DamageEvent& right) {
                         if (left.target.packed() != right.target.packed()) {
                             return left.target.packed() < right.target.packed();
                         }
                         if (left.tick.value != right.tick.value) {
                             return left.tick.value < right.tick.value;
                         }
                         return left.source.packed() < right.source.packed();
                     });

    CombatApplyResult result{};
    for (const DamageEvent& event : buffer.events_) {
        if (!std::isfinite(event.amount) || event.amount <= 0.0F ||
            !entities_.contains(event.target)) {
            continue;
        }
        float* health = entities_.health(event.target);
        std::uint32_t* flags = entities_.flags(event.target);
        if (health == nullptr || flags == nullptr || (*flags & simulation::EntityAlive) == 0) {
            continue;
        }
        *health = std::max(0.0F, *health - event.amount);
        ++result.accepted_events;
        if (*health <= 0.0F) {
            *flags &= ~simulation::EntityAlive;
            ++result.killed_entities;
        }
    }
    buffer.clear();
    return result;
}

bool FireRequest::valid() const noexcept {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    return source != 0U && weapon_id != 0U && ammunition_id != 0U && shot_sequence > 0U &&
           std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z) &&
           std::isfinite(direction.x) && std::isfinite(direction.y) && std::isfinite(direction.z) &&
           std::isfinite(length) && std::abs(length - 1.0F) < 1.0e-3F;
}

foundation::Result<void, foundation::Error> CombatCommandFlow::submitFire(
    const weapons::FireIntent& intent) noexcept {
    if (!intent.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid combat fire intent"});
    }
    const auto duplicate = [&intent](const auto& entry) {
        return entry.entity == intent.entity && entry.shot_sequence == intent.shot_sequence;
    };
    if (std::any_of(fire_intents_.begin(), fire_intents_.end(), duplicate) ||
        std::any_of(committed_shots_.begin(), committed_shots_.end(),
                    [&intent](const auto& entry) {
        return entry.first == intent.entity && entry.second == intent.shot_sequence;
                    })) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "duplicate combat fire intent"});
    }
    fire_intents_.push_back(intent);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<std::vector<FireRequest>, foundation::Error> CombatCommandFlow::commitFire(
    foundation::StableId match_seed) {
    std::stable_sort(fire_intents_.begin(), fire_intents_.end(),
                     [](const weapons::FireIntent& left, const weapons::FireIntent& right) {
                         if (left.tick.value != right.tick.value) {
                             return left.tick.value < right.tick.value;
                         }
                         if (left.entity != right.entity) {
                             return left.entity < right.entity;
                         }
                         return left.shot_sequence < right.shot_sequence;
                     });
    std::vector<FireRequest> requests;
    requests.reserve(fire_intents_.size());
    for (const weapons::FireIntent& intent : fire_intents_) {
        const foundation::StableId seed = foundation::stableHashCombine(
            foundation::stableHashCombine(foundation::stableHashU64(match_seed), intent.entity),
            foundation::stableHashCombine(foundation::stableHashU64(intent.shot_sequence),
                                          foundation::stableHashU64(intent.tick.value)));
        FireRequest request{intent.entity, intent.weapon_id, intent.ammunition_id,
                            intent.shot_sequence, intent.origin, intent.direction, intent.tick, seed};
        if (!request.valid()) {
            fire_intents_.clear();
            return foundation::Result<std::vector<FireRequest>, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "combat fire request conversion failed"});
        }
        requests.push_back(request);
        committed_shots_.push_back({intent.entity, intent.shot_sequence});
        presentation_.push_back({intent.entity, intent.weapon_id, intent.origin, intent.direction,
                                  intent.tick, intent.shot_sequence});
    }
    fire_intents_.clear();
    return foundation::Result<std::vector<FireRequest>, foundation::Error>::success(
        std::move(requests));
}

foundation::Result<void, foundation::Error> CombatCommandFlow::submitImpact(
    ImpactEvent impact) noexcept {
    const float normal_length = std::sqrt(impact.normal.x * impact.normal.x +
                                          impact.normal.y * impact.normal.y +
                                          impact.normal.z * impact.normal.z);
    if (!impact.source.isValid() || !impact.target.isValid() || !std::isfinite(impact.energy) ||
        impact.energy <= 0.0F || !std::isfinite(normal_length) || normal_length <= 1.0e-6F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid combat impact event"});
    }
    for (const ImpactEvent& existing : impacts_) {
        if (existing.source == impact.source && existing.target == impact.target &&
            existing.sequence == impact.sequence && existing.tick == impact.tick) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "duplicate combat impact event"});
        }
    }
    impacts_.push_back(impact);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<std::vector<DamageCommand>, foundation::Error> CombatCommandFlow::commitDamage() {
    std::stable_sort(impacts_.begin(), impacts_.end(), [](const ImpactEvent& left,
                                                          const ImpactEvent& right) {
        if (left.tick.value != right.tick.value) {
            return left.tick.value < right.tick.value;
        }
        if (left.target != right.target) {
            return left.target.packed() < right.target.packed();
        }
        if (left.source != right.source) {
            return left.source.packed() < right.source.packed();
        }
        return left.sequence < right.sequence;
    });
    std::vector<DamageCommand> damage;
    damage.reserve(impacts_.size());
    for (const ImpactEvent& impact : impacts_) {
        if (impact.target_kind != ImpactTargetKind::Infantry) {
            continue;
        }
        damage.push_back({impact.source, impact.target, impact.energy, DamageType::Kinetic,
                          impact.tick, impact.sequence});
    }
    impacts_.clear();
    return foundation::Result<std::vector<DamageCommand>, foundation::Error>::success(
        std::move(damage));
}

void CombatCommandFlow::clear() noexcept {
    fire_intents_.clear();
    impacts_.clear();
    presentation_.clear();
    committed_shots_.clear();
}

} // namespace genomes::combat
