#pragma once

#include <genomes/ballistics/FlightIntegrator.hpp>
#include <genomes/ballistics/ContactResolver.hpp>
#include <genomes/ballistics/Detonation.hpp>
#include <genomes/ballistics/ProjectileTrace.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/world/WorldQuerySnapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::ballistics {

inline constexpr std::uint32_t BallisticsWorldVersion = 1;

struct BallisticsProfile final {
    std::uint32_t max_projectiles{256};
    std::uint32_t max_fragments{2048};
    float fixed_step_seconds{1.0F / 60.0F};
    float tracking_seconds_without_ground{15.0F};
    float tracking_seconds_with_ground{120.0F};

    [[nodiscard]] bool valid() const noexcept;
};

enum class FireRejectReason : std::uint8_t {
    None,
    InvalidRequest,
    Capacity,
    Catalog,
};

struct FireAdmission final {
    bool accepted{false};
    FireRejectReason reason{FireRejectReason::None};
};

struct BallisticsContact final {
    TraceContact trace{};
    ContactOutcome outcome{ContactOutcome::Stopped};
    bool continue_flight{false};
    foundation::StableId impulse_token{0};
    foundation::Vec3 target_impulse{};
};

struct BallisticsTerminal final {
    TraceTerminal trace{};
};

struct BallisticsTickResult final {
    std::uint64_t tick{0};
    std::vector<BallisticsContact> contacts;
    std::vector<BallisticsTerminal> terminals;
    std::vector<DetonationEvent> detonations;
    std::uint32_t rejected_fires{0};
};

using ContactCandidateProvider = bool (*)(void*,
                                          const world::QuerySegmentHit&,
                                          ContactCandidate&) noexcept;

class BallisticsWorld final {
public:
    BallisticsWorld(AmmunitionCatalog catalog,
                    const world::WorldQuerySnapshot* query,
                    BallisticsProfile profile = {},
                    FlightEnvironment environment = {},
                    destruction::MaterialCatalog materials =
                        destruction::MaterialCatalog::makeDefault(),
                    ContactCandidateProvider contact_provider = nullptr,
                    void* contact_context = nullptr) noexcept;

    [[nodiscard]] FireAdmission queueFire(FireRequest request);
    [[nodiscard]] BallisticsTickResult advanceFixed(
        bool ground_enabled, ProjectileTraceObserver* observer = nullptr);

    void setQuerySnapshot(const world::WorldQuerySnapshot* query) noexcept { query_ = query; }
    void setEnvironment(FlightEnvironment environment) noexcept { environment_ = environment; }
    void setContactProvider(ContactCandidateProvider provider, void* context) noexcept {
        contact_provider_ = provider;
        contact_context_ = context;
    }
    [[nodiscard]] const BallisticsProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] const FlightEnvironment& environment() const noexcept { return environment_; }
    [[nodiscard]] std::size_t activeCount() const noexcept { return active_.size(); }
    [[nodiscard]] std::size_t pendingCount() const noexcept { return pending_.size(); }
    [[nodiscard]] const std::vector<ProjectileState>& active() const noexcept { return active_; }

private:
    struct PendingFire final {
        std::uint64_t sequence{0};
        FireRequest request{};
    };

    [[nodiscard]] std::uint32_t capacityFor(const FireRequest&) const noexcept;
    [[nodiscard]] std::uint32_t fragmentCount() const noexcept;
    [[nodiscard]] float trackingLimit(bool ground_enabled) const noexcept;

    AmmunitionCatalog catalog_;
    const world::WorldQuerySnapshot* query_{nullptr};
    BallisticsProfile profile_{};
    FlightEnvironment environment_{};
    destruction::MaterialCatalog materials_;
    ContactCandidateProvider contact_provider_{nullptr};
    void* contact_context_{nullptr};
    std::uint64_t tick_{0};
    std::uint64_t sequence_{0};
    std::vector<PendingFire> pending_;
    std::vector<ProjectileState> active_;
};

} // namespace genomes::ballistics
