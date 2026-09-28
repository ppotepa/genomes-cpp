#include <genomes/ballistics/BallisticsWorld.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::ballistics {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(1.0e-8F, length_squared(value)));
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] TraceTerminal terminalFor(const ProjectileState& state,
                                        TerminalReason reason,
                                        std::uint64_t age_tick) noexcept {
    return {state.projectile_id.value(), state.trace_id, reason, state.position, age_tick};
}

} // namespace

bool BallisticsProfile::valid() const noexcept {
    return max_projectiles > 0 && max_fragments > 0 && std::isfinite(fixed_step_seconds) &&
           fixed_step_seconds > 0.0F && std::isfinite(tracking_seconds_without_ground) &&
           tracking_seconds_without_ground > 0.0F &&
           std::isfinite(tracking_seconds_with_ground) && tracking_seconds_with_ground > 0.0F;
}

BallisticsWorld::BallisticsWorld(AmmunitionCatalog catalog,
                                 const world::WorldQuerySnapshot* query,
                                 BallisticsProfile profile,
                                 FlightEnvironment environment,
                                 destruction::MaterialCatalog materials,
                                 ContactCandidateProvider contact_provider,
                                 void* contact_context) noexcept
    : catalog_{std::move(catalog)},
      query_{query},
      profile_{profile},
      environment_{environment},
      materials_{std::move(materials)},
      contact_provider_{contact_provider},
      contact_context_{contact_context} {
    pending_.reserve(profile_.max_projectiles);
    active_.reserve(profile_.max_projectiles);
}

std::uint32_t BallisticsWorld::capacityFor(const FireRequest& request) const noexcept {
    return request.fragment ? profile_.max_fragments : profile_.max_projectiles;
}

std::uint32_t BallisticsWorld::fragmentCount() const noexcept {
    std::uint32_t count = 0;
    for (const ProjectileState& state : active_) {
        count += state.fragment ? 1U : 0U;
    }
    for (const PendingFire& pending : pending_) {
        count += pending.request.fragment ? 1U : 0U;
    }
    return count;
}

FireAdmission BallisticsWorld::queueFire(FireRequest request) {
    if (!profile_.valid() || !request.projectile_id.isValid() || !request.shot_id.isValid() ||
        !request.trace_id.isValid() || !request.ammunition_id.isValid() ||
        !finite(request.position) || !finite(request.direction) ||
        length_squared(request.direction) <= 1.0e-8F || catalog_.find(request.ammunition_id) == nullptr) {
        return {false, catalog_.find(request.ammunition_id) == nullptr ? FireRejectReason::Catalog
                                                                        : FireRejectReason::InvalidRequest};
    }
    std::size_t count = 0;
    for (const ProjectileState& state : active_) {
        count += state.fragment == request.fragment ? 1U : 0U;
    }
    for (const PendingFire& pending : pending_) {
        count += pending.request.fragment == request.fragment ? 1U : 0U;
    }
    if (count >= capacityFor(request)) {
        return {false, FireRejectReason::Capacity};
    }
    pending_.push_back({sequence_++, std::move(request)});
    return {true, FireRejectReason::None};
}

float BallisticsWorld::trackingLimit(bool ground_enabled) const noexcept {
    return ground_enabled ? profile_.tracking_seconds_with_ground
                           : profile_.tracking_seconds_without_ground;
}

BallisticsTickResult BallisticsWorld::advanceFixed(bool ground_enabled,
                                                   ProjectileTraceObserver* observer) {
    BallisticsTickResult result{};
    result.tick = ++tick_;
    std::stable_sort(pending_.begin(), pending_.end(), [](const auto& left, const auto& right) {
        return left.sequence < right.sequence;
    });
    for (const PendingFire& pending : pending_) {
        const auto created = ProjectileState::create(pending.request, catalog_);
        if (!created) {
            ++result.rejected_fires;
            continue;
        }
        active_.push_back(created.value());
    }
    pending_.clear();

    std::vector<ProjectileState> survivors;
    survivors.reserve(active_.size());
    const float limit = trackingLimit(ground_enabled);
    for (ProjectileState state : active_) {
        const AmmunitionStrategy* strategy = catalog_.strategy(state.strategy_id);
        bool terminal = false;
        TerminalReason terminal_reason = TerminalReason::Stopped;
        float remaining = profile_.fixed_step_seconds;
        std::uint32_t substeps = 0;
        while (remaining > 1.0e-7F && !terminal) {
            if (++substeps > 256U) {
                terminal = true;
                terminal_reason = TerminalReason::Removed;
                break;
            }
            if (strategy == nullptr) {
                terminal = true;
                terminal_reason = TerminalReason::Removed;
                break;
            }
            const FlightIntervalInput interval_input{&state,
                                                      strategy,
                                                      &environment_,
                                                      remaining,
                                                      state.diameter_m,
                                                      state.fragment};
            const auto interval = FlightIntegrator::chooseInterval(interval_input);
            if (!interval) {
                terminal = true;
                terminal_reason = TerminalReason::Removed;
                break;
            }
            const auto step = FlightIntegrator::integrate(interval_input, interval.value());
            if (!step) {
                terminal = true;
                terminal_reason = TerminalReason::Removed;
                break;
            }
            const TraceSegment trace_segment{state.projectile_id.value(),
                                             state.trace_id,
                                             state.position,
                                             step.value().position,
                                             state.translationalEnergy(),
                                             0.5F * state.mass_kg *
                                                 (step.value().velocity.x * step.value().velocity.x +
                                                  step.value().velocity.y * step.value().velocity.y +
                                                  step.value().velocity.z * step.value().velocity.z),
                                             state.impact_index};
            if (observer != nullptr) {
                observer->onSegment(trace_segment);
            }

            world::QuerySegmentResult query_result{};
            if (query_ != nullptr) {
                query_result = query_->querySegment(state.position, step.value().position, true);
            }
            if (!query_result.hits.empty()) {
                const world::QuerySegmentHit& hit = query_result.hits.front();
                const float consumed = interval.value() * std::clamp(hit.t, 0.0F, 1.0F);
                state.position = hit.point;
                state.velocity = step.value().velocity;
                (void)state.advanceAge(0U, consumed, step.value().travel_distance_m * hit.t);
                const TerminalReason default_contact_reason =
                    hit.candidate.source == world::QuerySourceKind::Terrain
                        ? TerminalReason::GroundContact
                        : TerminalReason::Stopped;
                const TraceContact contact{state.projectile_id.value(),
                                           state.trace_id,
                                           hit.candidate.id,
                                           hit.candidate.source,
                                           hit.point,
                                           {0.0F, 1.0F, 0.0F},
                                           step.value().travel_distance_m * hit.t,
                                           state.impact_index};
                if (observer != nullptr) {
                    observer->onContact(contact);
                }
                BallisticsContact contact_result{contact};
                bool resolved = false;
                bool detonated = false;
                if (contact_provider_ != nullptr) {
                    ContactCandidate candidate{};
                    if (contact_provider_(contact_context_, hit, candidate)) {
                        const AmmunitionStrategy* ammunition_strategy =
                            catalog_.strategy(state.strategy_id);
                        if (ammunition_strategy != nullptr &&
                            ammunition_strategy->fuze == FuzeMode::ArmedContact &&
                            ammunition_strategy->explosive) {
                            const AmmunitionDefinition* ammunition = catalog_.find(state.ammunition_id);
                            const std::uint32_t occupied = fragmentCount();
                            const std::uint32_t available = occupied >= profile_.max_fragments
                                                                ? 0U
                                                                : profile_.max_fragments - occupied;
                            const auto detonation = ammunition == nullptr
                                                        ? foundation::Result<DetonationEvent,
                                                                             foundation::Error>::failure(
                                                              {foundation::ErrorCode::NotFound,
                                                               "detonation ammunition not found"})
                                                        : Detonation::detonate(
                                                              state,
                                                              *ammunition,
                                                              *ammunition_strategy,
                                                              {candidate.contact_id,
                                                               candidate.point,
                                                               candidate.normal,
                                                               true,
                                                               available});
                            if (detonation) {
                                resolved = true;
                                detonated = true;
                                contact_result.outcome = ContactOutcome::Stopped;
                                contact_result.continue_flight = false;
                                contact_result.impulse_token = candidate.contact_id;
                                result.detonations.push_back(detonation.value());
                                state.energy.explosive_energy += detonation.value().ledger.explosive_energy;
                                state.energy.blast_energy += detonation.value().ledger.blast_energy;
                                state.energy.fragment_energy +=
                                    detonation.value().ledger.represented_fragment_energy;
                                state.energy.unrepresented_energy +=
                                    detonation.value().ledger.unrepresented_energy;
                                state.energy.material_work += detonation.value().ledger.nose_crush_work;
                                for (const FragmentSpawn& fragment : detonation.value().fragments) {
                                    if (!queueFire(Detonation::makeFireRequest(
                                            detonation.value(), fragment, tick_))
                                             .accepted) {
                                        ++result.rejected_fires;
                                    }
                                }
                                terminal = true;
                                terminal_reason = TerminalReason::Stopped;
                            } else {
                                terminal = true;
                                terminal_reason = TerminalReason::Removed;
                            }
                        } else {
                            const auto resolution = ContactResolver::resolve(
                                state, candidate, catalog_, materials_);
                            if (resolution) {
                                resolved = true;
                                contact_result.outcome = resolution.value().outcome;
                                contact_result.continue_flight = resolution.value().continue_flight;
                                contact_result.impulse_token = resolution.value().impulse_token;
                                contact_result.target_impulse = resolution.value().target_impulse;
                                if (resolution.value().continue_flight &&
                                    state.apply(resolution.value().transition) &&
                                    state.setVelocity(resolution.value().outgoing_velocity)) {
                                    const foundation::Vec3 exit = candidate.exit_point;
                                    state.position = add(exit,
                                                          multiply(normalized(state.velocity), 0.001F));
                                    const float consumed_step = std::max(consumed, 1.0e-5F);
                                    remaining -= std::min(remaining, consumed_step);
                                    result.contacts.push_back(contact_result);
                                    continue;
                                }
                                terminal = true;
                                terminal_reason = TerminalReason::Stopped;
                            } else {
                                terminal = true;
                                terminal_reason = TerminalReason::Removed;
                            }
                        }
                    }
                }
                result.contacts.push_back(contact_result);
                if (!resolved && !detonated && !terminal) {
                    terminal = true;
                    terminal_reason = default_contact_reason;
                }
                if (terminal) {
                    break;
                }
            }

            state.position = step.value().position;
            state.velocity = step.value().velocity;
            state.energy.flight_work += step.value().flight_work;
            (void)state.advanceAge(0U, step.value().seconds, step.value().travel_distance_m);
            remaining -= step.value().seconds;
            if (state.age_seconds >= limit) {
                terminal = true;
                terminal_reason = TerminalReason::TrackingLimit;
            }
        }
        (void)state.advanceAge(1U, 0.0F, 0.0F);
        if (terminal) {
            const TraceTerminal trace = terminalFor(state, terminal_reason, result.tick);
            result.terminals.push_back({trace});
            if (observer != nullptr) {
                observer->onTerminal(trace);
            }
        } else {
            survivors.push_back(std::move(state));
        }
    }
    active_ = std::move(survivors);
    return result;
}

} // namespace genomes::ballistics
