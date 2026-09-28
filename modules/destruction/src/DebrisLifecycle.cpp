#include <genomes/destruction/DebrisLifecycle.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool valid_record(const DebrisRecord& record) noexcept {
    if (record.id == 0 || record.source_component_id == 0 || !finite(record.position) ||
        !finite(record.linear_velocity) || !finite(record.angular_velocity) ||
        !std::isfinite(record.importance) || record.importance < 0.0F ||
        record.material_volumes.empty()) {
        return false;
    }
    for (const MaterialVolume& volume : record.material_volumes) {
        if (!volume.material || !std::isfinite(volume.volume) || volume.volume <= 0.0) {
            return false;
        }
    }
    return record.totalVolume() > 0.0;
}

[[nodiscard]] bool lowPriority(const DebrisRecord& left,
                               const DebrisRecord& right) noexcept {
    if (left.importance != right.importance) {
        return left.importance < right.importance;
    }
    if (left.awake != right.awake) {
        return !left.awake;
    }
    if (left.age_ticks != right.age_ticks) {
        return left.age_ticks > right.age_ticks;
    }
    return left.id < right.id;
}

} // namespace

foundation::Result<DebrisLifecycle, foundation::Error> DebrisLifecycle::create(
    DebrisPolicy policy) {
    if (!policy.valid()) {
        return foundation::Result<DebrisLifecycle, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid debris policy"});
    }
    DebrisLifecycle lifecycle;
    lifecycle.policy_ = policy;
    lifecycle.records_.reserve(static_cast<std::size_t>(policy.max_debris) +
                               static_cast<std::size_t>(policy.max_cheap_debris));
    return foundation::Result<DebrisLifecycle, foundation::Error>::success(std::move(lifecycle));
}

foundation::Result<foundation::StableId, foundation::Error> DebrisLifecycle::spawn(
    DebrisRecord record) {
    if (!valid_record(record)) {
        return foundation::Result<foundation::StableId, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid debris record"});
    }
    if (find(record.id) != nullptr) {
        return foundation::Result<foundation::StableId, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "duplicate debris ID"});
    }
    std::size_t active_hero = 0;
    for (const DebrisRecord& item : records_) {
        if (item.representation == DebrisRepresentation::Hero && item.awake) {
            ++active_hero;
        }
    }
    record.representation = record.shape_representable &&
                                   count(DebrisRepresentation::Hero) < policy_.max_debris &&
                                   active_hero < policy_.max_active_debris
                               ? DebrisRepresentation::Hero
                               : DebrisRepresentation::Cheap;
    records_.push_back(std::move(record));
    enforceBudgets();
    return foundation::Result<foundation::StableId, foundation::Error>::success(records_.back().id);
}

bool DebrisLifecycle::wake(foundation::StableId id) noexcept {
    DebrisRecord* record = find(id);
    if (record == nullptr) {
        return false;
    }
    record->awake = true;
    record->age_ticks = 0;
    enforceBudgets();
    return true;
}

bool DebrisLifecycle::sleep(foundation::StableId id) noexcept {
    DebrisRecord* record = find(id);
    if (record == nullptr) {
        return false;
    }
    record->awake = false;
    record->age_ticks = 0;
    return true;
}

void DebrisLifecycle::advance(std::uint64_t ticks) noexcept {
    for (DebrisRecord& record : records_) {
        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        if (ticks > maximum - record.age_ticks) {
            record.age_ticks = maximum;
        } else {
            record.age_ticks += ticks;
        }
    }
}

std::vector<DebrisRecord> DebrisLifecycle::prepareSettlement() const {
    std::vector<DebrisRecord> result;
    for (const DebrisRecord& record : records_) {
        if ((record.representation == DebrisRepresentation::Hero ||
             record.representation == DebrisRepresentation::Cheap) &&
            !record.awake && record.age_ticks >= policy_.settle_grace_ticks) {
            result.push_back(record);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const DebrisRecord& left, const DebrisRecord& right) {
                  return left.id < right.id;
              });
    return result;
}

bool DebrisLifecycle::commitSettlement(
    std::span<const foundation::StableId> ids) noexcept {
    for (const foundation::StableId id : ids) {
        const DebrisRecord* record = find(id);
        if (record == nullptr || record->awake ||
            (record->representation != DebrisRepresentation::Hero &&
             record->representation != DebrisRepresentation::Cheap) ||
            record->age_ticks < policy_.settle_grace_ticks) {
            return false;
        }
    }
    for (const foundation::StableId id : ids) {
        DebrisRecord* record = find(id);
        record->representation = DebrisRepresentation::Baked;
    }
    return true;
}

RepresentationAccounting DebrisLifecycle::accounting() const {
    RepresentationAccounting result{};
    for (const DebrisRecord& record : records_) {
        const double volume = record.totalVolume();
        result.original_source_volume += volume;
        switch (record.representation) {
        case DebrisRepresentation::SourceAttached:
            result.attached_volume += volume;
            break;
        case DebrisRepresentation::Hero:
            result.hero_volume += volume;
            break;
        case DebrisRepresentation::Cheap:
            result.cheap_volume += volume;
            break;
        case DebrisRepresentation::Baked:
            result.baked_volume += volume;
            break;
        case DebrisRepresentation::RemovedByDamage:
            result.removed_by_damage_volume += volume;
            break;
        }
        for (const MaterialVolume& material_volume : record.material_volumes) {
            const auto iterator = std::find_if(
                result.material_totals.begin(), result.material_totals.end(),
                [&material_volume](const MaterialVolume& current) {
                    return current.material == material_volume.material;
                });
            if (iterator == result.material_totals.end()) {
                result.material_totals.push_back(material_volume);
            } else {
                iterator->volume += material_volume.volume;
            }
        }
    }
    std::sort(result.material_totals.begin(), result.material_totals.end(),
              [](const MaterialVolume& left, const MaterialVolume& right) {
                  return left.material.value < right.material.value;
              });
    return result;
}

std::size_t DebrisLifecycle::count(DebrisRepresentation representation) const noexcept {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(), records_.end(), [representation](const DebrisRecord& record) {
            return record.representation == representation;
        }));
}

void DebrisLifecycle::enforceBudgets() noexcept {
    auto candidates = [this](DebrisRepresentation representation) {
        std::vector<DebrisRecord*> result;
        for (DebrisRecord& record : records_) {
            if (record.representation == representation) {
                result.push_back(&record);
            }
        }
        std::sort(result.begin(), result.end(), [](const DebrisRecord* left,
                                                   const DebrisRecord* right) {
            return lowPriority(*left, *right);
        });
        return result;
    };
    auto hero = candidates(DebrisRepresentation::Hero);
    std::size_t active_count = static_cast<std::size_t>(std::count_if(
        hero.begin(), hero.end(), [](const DebrisRecord* record) { return record->awake; }));
    for (DebrisRecord* record : hero) {
        if (active_count <= policy_.max_active_debris) {
            break;
        }
        if (record->awake) {
            record->representation = DebrisRepresentation::Cheap;
            --active_count;
        }
    }
    hero = candidates(DebrisRepresentation::Hero);
    while (hero.size() > policy_.max_debris) {
        hero.front()->representation = DebrisRepresentation::Cheap;
        hero.erase(hero.begin());
    }
    auto cheap = candidates(DebrisRepresentation::Cheap);
    while (cheap.size() > policy_.max_cheap_debris) {
        cheap.front()->representation = DebrisRepresentation::Baked;
        cheap.erase(cheap.begin());
    }
}

DebrisRecord* DebrisLifecycle::find(foundation::StableId id) noexcept {
    const auto iterator = std::find_if(records_.begin(), records_.end(),
                                       [id](const DebrisRecord& record) {
                                           return record.id == id;
                                       });
    return iterator == records_.end() ? nullptr : &*iterator;
}

const DebrisRecord* DebrisLifecycle::find(foundation::StableId id) const noexcept {
    const auto iterator = std::find_if(records_.begin(), records_.end(),
                                       [id](const DebrisRecord& record) {
                                           return record.id == id;
                                       });
    return iterator == records_.end() ? nullptr : &*iterator;
}

} // namespace genomes::destruction
