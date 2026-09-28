#include <genomes/buildings/BuildingModel.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::buildings {

BuildingRuntime::BuildingRuntime(const BuildingPlan& plan) {
    parts_.reserve(plan.parts.size());
    for (const BuildingPart& part : plan.parts) {
        parts_.push_back({part.id, 1.0F, false});
    }
}

bool BuildingRuntime::applyDamage(foundation::StableId part_id, float normalized_damage) noexcept {
    if (!std::isfinite(normalized_damage) || normalized_damage <= 0.0F) {
        return false;
    }
    const auto iterator = std::find_if(parts_.begin(), parts_.end(),
                                       [part_id](const BuildingPartRuntime& part) {
                                           return part.part_id == part_id;
                                       });
    if (iterator == parts_.end() || iterator->destroyed) {
        return false;
    }
    iterator->integrity = std::max(0.0F, iterator->integrity - normalized_damage);
    iterator->destroyed = iterator->integrity <= 0.0F;
    return true;
}

bool BuildingRuntime::isDestroyed(foundation::StableId part_id) const noexcept {
    const auto iterator = std::find_if(parts_.begin(), parts_.end(),
                                       [part_id](const BuildingPartRuntime& part) {
                                           return part.part_id == part_id;
                                       });
    return iterator != parts_.end() && iterator->destroyed;
}

} // namespace genomes::buildings
