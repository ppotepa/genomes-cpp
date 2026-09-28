#include <genomes/combat/InfluenceField.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

namespace genomes::combat {

namespace {

constexpr std::size_t MaxInfluenceFieldCells = 16U * 1024U * 1024U;

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec2 value) noexcept {
    return finite(value.x) && finite(value.y);
}

[[nodiscard]] bool validType(InfluenceSourceType type) noexcept {
    switch (type) {
    case InfluenceSourceType::KnownContact:
    case InfluenceSourceType::FriendlyForce:
    case InfluenceSourceType::Order:
        return true;
    }
    return false;
}

[[nodiscard]] bool checkedCellCount(const InfluenceFieldSpec& spec,
                                    std::size_t& count) noexcept {
    const auto width = static_cast<std::size_t>(spec.width);
    const auto height = static_cast<std::size_t>(spec.height);
    if (width == 0U || height == 0U || width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    count = width * height;
    return count <= MaxInfluenceFieldCells;
}

[[nodiscard]] bool sourceLess(const InfluenceSource* left,
                              const InfluenceSource* right) noexcept {
    return std::tuple{left->stable_id, static_cast<std::uint8_t>(left->type),
                       left->observed_tick.value, left->position.x, left->position.y,
                       left->radius_m, left->strength, left->confidence, left->facing.x,
                       left->facing.y, left->anisotropy} <
           std::tuple{right->stable_id, static_cast<std::uint8_t>(right->type),
                      right->observed_tick.value, right->position.x, right->position.y,
                      right->radius_m, right->strength, right->confidence, right->facing.x,
                      right->facing.y, right->anisotropy};
}

[[nodiscard]] float temporalConfidence(const InfluenceSource& source,
                                       const InfluenceFieldSpec& spec) noexcept {
    const std::uint64_t age = spec.evaluation_tick.value - source.observed_tick.value;
    return source.confidence *
           std::pow(spec.confidence_decay_per_tick, static_cast<float>(age));
}

} // namespace

bool InfluenceSource::valid() const noexcept {
    return stable_id != 0U && known_to_faction && finite(position) && finite(facing) && finite(radius_m) &&
           radius_m > 0.0F && finite(strength) && finite(confidence) && confidence >= 0.0F &&
           confidence <= 1.0F && finite(anisotropy) && anisotropy >= 0.0F && anisotropy <= 1.0F &&
           (anisotropy == 0.0F || (facing.x * facing.x + facing.y * facing.y > 1.0e-10F)) &&
           validType(type);
}

bool InfluenceFieldSpec::valid() const noexcept {
    std::size_t count = 0U;
    return checkedCellCount(*this, count) && finite(world_origin) && finite(cell_size) &&
           cell_size.x > 0.0F && cell_size.y > 0.0F && field_revision > 0U &&
           max_source_age_ticks > 0U && finite(confidence_decay_per_tick) &&
           confidence_decay_per_tick > 0.0F && confidence_decay_per_tick <= 1.0F &&
           finite(minimum_confidence) && minimum_confidence >= 0.0F && minimum_confidence <= 1.0F &&
           (!layer_type.has_value() || validType(*layer_type));
}

float influenceSmoothstep01(float value) noexcept {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

float influenceContribution(const InfluenceSource& source, foundation::Vec2 cell_center,
                            const InfluenceFieldSpec& spec) noexcept {
    if (!source.valid() || !spec.valid() || source.observed_tick > spec.evaluation_tick) {
        return 0.0F;
    }
    const std::uint64_t age = spec.evaluation_tick.value - source.observed_tick.value;
    if (age > spec.max_source_age_ticks || temporalConfidence(source, spec) < spec.minimum_confidence) {
        return 0.0F;
    }
    const float dx = cell_center.x - source.position.x;
    const float dy = cell_center.y - source.position.y;
    const float distance_squared = dx * dx + dy * dy;
    const float radius_squared = source.radius_m * source.radius_m;
    if (distance_squared >= radius_squared) {
        return 0.0F;
    }

    const float distance = std::sqrt(distance_squared);
    const float radial = influenceSmoothstep01(1.0F - distance / source.radius_m);
    float directional = 1.0F;
    if (source.anisotropy > 0.0F && distance > 1.0e-5F) {
        const float facing_length = std::sqrt(source.facing.x * source.facing.x +
                                               source.facing.y * source.facing.y);
        const float dot = (dx * source.facing.x + dy * source.facing.y) /
                          (distance * facing_length);
        directional += source.anisotropy * std::max(0.0F, dot);
    }
    return source.strength * temporalConfidence(source, spec) * radial * directional;
}

foundation::Result<InfluenceField, foundation::Error> InfluenceField::compute(
    InfluenceFieldSpec spec, std::span<const InfluenceSource> sources) {
    if (!spec.valid()) {
        return foundation::Result<InfluenceField, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid influence field specification"});
    }

    std::size_t cell_count = 0U;
    (void)checkedCellCount(spec, cell_count);
    InfluenceField field{};
    field.spec_ = spec;
    field.values_.assign(cell_count, 0.0F);
    field.revision_ = {spec.field_revision, spec.world_field_revision, spec.evaluation_tick,
                       foundation::SimulationTick{}, 0U};

    std::vector<const InfluenceSource*> ordered;
    ordered.reserve(sources.size());
    for (const InfluenceSource& source : sources) {
        if (!source.valid() || source.observed_tick > spec.evaluation_tick) {
            return foundation::Result<InfluenceField, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "influence source is not known or has a future observation tick"});
        }
        if (spec.layer_type.has_value() && source.type != *spec.layer_type) {
            continue;
        }
        const std::uint64_t age = spec.evaluation_tick.value - source.observed_tick.value;
        if (age > spec.max_source_age_ticks ||
            temporalConfidence(source, spec) < spec.minimum_confidence) {
            continue;
        }
        ordered.push_back(&source);
        ++field.revision_.accepted_source_count;
        if (field.revision_.accepted_source_count == 1U ||
            source.observed_tick > field.revision_.source_snapshot_tick) {
            field.revision_.source_snapshot_tick = source.observed_tick;
        }
    }

    if (!spec.enabled || ordered.empty()) {
        return foundation::Result<InfluenceField, foundation::Error>::success(std::move(field));
    }

    std::stable_sort(ordered.begin(), ordered.end(), sourceLess);
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            const foundation::Vec2 cell_center{
                spec.world_origin.x + (static_cast<float>(x) + 0.5F) * spec.cell_size.x,
                spec.world_origin.y + (static_cast<float>(y) + 0.5F) * spec.cell_size.y};
            float sum = 0.0F;
            for (const InfluenceSource* source : ordered) {
                sum += influenceContribution(*source, cell_center, spec);
            }
            field.values_[static_cast<std::size_t>(y) * spec.width + x] = sum;
        }
    }
    return foundation::Result<InfluenceField, foundation::Error>::success(std::move(field));
}

foundation::Result<float, foundation::Error> InfluenceField::sample(
    foundation::Vec2 world) const {
    if (!finite(world)) {
        return foundation::Result<float, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "influence sample is not finite"});
    }
    const float local_x = (world.x - spec_.world_origin.x) / spec_.cell_size.x;
    const float local_y = (world.y - spec_.world_origin.y) / spec_.cell_size.y;
    if (!finite(local_x) || !finite(local_y) || local_x < 0.0F || local_y < 0.0F ||
        local_x >= static_cast<float>(spec_.width) || local_y >= static_cast<float>(spec_.height)) {
        return foundation::Result<float, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "influence sample outside field"});
    }
    return foundation::Result<float, foundation::Error>::success(
        at(static_cast<std::uint32_t>(std::floor(local_x)),
           static_cast<std::uint32_t>(std::floor(local_y))));
}

} // namespace genomes::combat
