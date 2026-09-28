#pragma once

#include <genomes/ballistics/BallisticsWorld.hpp>
#include <genomes/ballistics/FlightIntegrator.hpp>
#include <genomes/ballistics/ProjectileStorage.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/world/WorldQuery.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::ballistics {

enum class BatchFlightStatus : std::uint8_t {
    Integrated,
    InvalidState,
    MissingStrategy,
    InvalidInterval,
};

struct BatchFlightOutput final {
    std::vector<FlightStep> steps;
    std::vector<BatchFlightStatus> status;

    [[nodiscard]] bool valid() const noexcept { return steps.size() == status.size(); }
};

struct BallisticsBatchEventKey final {
    std::uint64_t tick{0};
    foundation::StableId projectile_id{0};
    std::uint32_t local_sequence{0};

    friend constexpr bool operator<(const BallisticsBatchEventKey& left,
                                    const BallisticsBatchEventKey& right) noexcept {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        if (left.projectile_id != right.projectile_id) {
            return left.projectile_id < right.projectile_id;
        }
        return left.local_sequence < right.local_sequence;
    }
};

struct BallisticsBatchEvent final {
    BallisticsBatchEventKey key{};
    std::uint8_t kind{0};
    foundation::StableId semantic_id{0};
};

class BallisticsBatch final {
public:
    [[nodiscard]] static foundation::Result<BatchFlightOutput, foundation::Error> integrate(
        std::span<const ProjectileState>,
        const AmmunitionCatalog&,
        const FlightEnvironment&,
        float seconds,
        jobs::JobSystem* jobs = nullptr,
        std::size_t grain_size = 0);

    [[nodiscard]] static foundation::Result<BatchFlightOutput, foundation::Error> integrate(
        const ProjectileStorage&,
        const AmmunitionCatalog&,
        const FlightEnvironment&,
        float seconds,
        jobs::JobSystem* jobs = nullptr,
        std::size_t grain_size = 0);

    [[nodiscard]] static std::vector<world::QuerySegmentResult> querySegments(
        const world::WorldQuerySnapshot&,
        std::span<const world::QuerySegmentRequest>,
        bool stable_order = true);

    static void stableSortEvents(std::vector<BallisticsBatchEvent>& events) noexcept;
};

} // namespace genomes::ballistics
