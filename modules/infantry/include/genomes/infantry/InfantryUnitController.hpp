#ifndef GENOMES_INFANTRY_INFANTRYUNITCONTROLLER_HPP
#define GENOMES_INFANTRY_INFANTRYUNITCONTROLLER_HPP

#include <genomes/simulation/EntityController.hpp>

#include <array>

namespace genomes::infantry {

[[nodiscard]] inline foundation::StableId infantryUnitActionId(
    std::uint8_t variant) noexcept {
    constexpr std::array<foundation::StableId, 8U> actions{
        foundation::stable_id("infantry.action.idle"),
        foundation::stable_id("infantry.action.walk"),
        foundation::stable_id("infantry.action.run"),
        foundation::stable_id("infantry.action.crouch"),
        foundation::stable_id("infantry.action.crouch-walk"),
        foundation::stable_id("infantry.action.prone"),
        foundation::stable_id("infantry.action.prone-move"),
        foundation::stable_id("infantry.action.weapon-ready"),
    };
    return actions[variant % actions.size()];
}

// Infantry-specific implementation of the generic batched entity controller.
// It resolves movement orders and keeps turning/acceleration continuous; pose,
// weapon and contact systems consume the action/order alongside its output.
class InfantryUnitController final : public simulation::EntityController {
public:
    [[nodiscard]] foundation::StableId entityType() const noexcept override;

    [[nodiscard]] foundation::Result<void, foundation::Error> updateBatch(
        std::span<const simulation::EntityControlRequest> requests,
        std::span<simulation::EntityControlCommand> commands,
        const simulation::TickContext& context) const noexcept override;
};

} // namespace genomes::infantry

#endif // GENOMES_INFANTRY_INFANTRYUNITCONTROLLER_HPP
