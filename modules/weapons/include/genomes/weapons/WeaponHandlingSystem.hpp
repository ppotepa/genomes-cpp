#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponPoseTasks.hpp>

#include <optional>

namespace genomes::weapons {

struct WeaponLocomotionView final {
    bool sprinting{false};
    bool prone{false};
    bool resting{false};
    float speed_mps{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

struct WeaponHandlingInput final {
    foundation::StableId entity{0};
    const WeaponDefinition* definition{nullptr};
    const WeaponArtifact* artifact{nullptr};
    foundation::Vec3 root_position{};
    std::optional<foundation::Vec3> world_aim_target;
    WeaponLocomotionView locomotion{};
    bool request_fire{false};
};

class WeaponHandlingSystem final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> select(
        WeaponRuntimeState&, WeaponId) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> requestReadiness(
        WeaponRuntimeState&, float) const noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> step(
        WeaponRuntimeState&, const WeaponHandlingInput&, foundation::SimulationTick,
        float fixed_dt_seconds, WeaponStepOutput&) const noexcept;

private:
    static constexpr float draw_seconds_ = 0.22F;
    static constexpr float holster_seconds_ = 0.18F;
};

} // namespace genomes::weapons
