#include <genomes/destruction/DamageField.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                         foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(1.0e-12F, length_squared(value)));
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] bool valid_coefficients(const DamageCoefficients& coefficients) noexcept {
    return std::isfinite(coefficients.crush) && coefficients.crush >= 0.0F &&
           std::isfinite(coefficients.crack) && coefficients.crack >= 0.0F &&
           std::isfinite(coefficients.rear) && coefficients.rear >= 0.0F &&
           std::isfinite(coefficients.weakness) && coefficients.weakness >= 0.0F;
}

} // namespace

bool DamageFieldSpec::valid() const noexcept {
    const foundation::Vec3 extent = subtract(maximum, minimum);
    const auto valid_axis = [](float value) {
        return std::isfinite(value) && value > 0.0F;
    };
    const std::uint64_t cell_count = static_cast<std::uint64_t>(cells_x) * cells_y * cells_z;
    return seed != 0 && finite(minimum) && finite(maximum) && finite(cell_size) &&
           valid_axis(extent.x) && valid_axis(extent.y) && valid_axis(extent.z) &&
           valid_axis(cell_size.x) && valid_axis(cell_size.y) && valid_axis(cell_size.z) &&
           cells_x > 0 && cells_y > 0 && cells_z > 0 && cells_x <= 256 && cells_y <= 256 &&
           cells_z <= 256 && cell_count <= 1'000'000U && max_holes <= 4096U;
}

foundation::Result<DamageField, foundation::Error> DamageField::create(DamageFieldSpec spec) {
    if (!spec.valid()) {
        return foundation::Result<DamageField, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid damage field specification"});
    }
    DamageField field;
    field.spec_ = spec;
    field.cells_.resize(static_cast<std::size_t>(spec.cells_x) * spec.cells_y * spec.cells_z);
    for (std::uint32_t z = 0; z < spec.cells_z; ++z) {
        for (std::uint32_t y = 0; y < spec.cells_y; ++y) {
            for (std::uint32_t x = 0; x < spec.cells_x; ++x) {
                field.cells_[field.index(x, y, z)].center = {
                    spec.minimum.x + (static_cast<float>(x) + 0.5F) * spec.cell_size.x,
                    spec.minimum.y + (static_cast<float>(y) + 0.5F) * spec.cell_size.y,
                    spec.minimum.z + (static_cast<float>(z) + 0.5F) * spec.cell_size.z};
            }
        }
    }
    field.holes_.reserve(spec.max_holes);
    return foundation::Result<DamageField, foundation::Error>::success(std::move(field));
}

DamageApplyResult DamageField::apply(const ImpactDamageCommand& command) noexcept {
    DamageApplyResult result{};
    if (command.event_id == 0 || !finite(command.local_point) || !finite(command.direction) ||
        !std::isfinite(command.radius) || command.radius <= 0.0F ||
        !std::isfinite(command.depth) || command.depth < 0.0F ||
        !std::isfinite(command.energy) || command.energy < 0.0F ||
        !valid_coefficients(command.coefficients) || length_squared(command.direction) <= 1.0e-12F) {
        result.clipped = true;
        return result;
    }
    const foundation::Vec3 extent = subtract(spec_.maximum, spec_.minimum);
    const auto to_index = [](float value, float minimum, float cell_size,
                             std::uint32_t count) {
        const auto raw = static_cast<std::int64_t>(std::floor((value - minimum) / cell_size));
        return std::clamp<std::int64_t>(raw, 0, static_cast<std::int64_t>(count) - 1);
    };
    const auto inside = [&](foundation::Vec3 point) {
        return point.x >= spec_.minimum.x && point.x <= spec_.maximum.x &&
               point.y >= spec_.minimum.y && point.y <= spec_.maximum.y &&
               point.z >= spec_.minimum.z && point.z <= spec_.maximum.z;
    };
    if (!inside(command.local_point)) {
        result.clipped = true;
    }
    const std::int64_t min_x = to_index(command.local_point.x - command.radius,
                                        spec_.minimum.x, spec_.cell_size.x, spec_.cells_x);
    const std::int64_t max_x = to_index(command.local_point.x + command.radius,
                                        spec_.minimum.x, spec_.cell_size.x, spec_.cells_x);
    const std::int64_t min_y = to_index(command.local_point.y - command.radius,
                                        spec_.minimum.y, spec_.cell_size.y, spec_.cells_y);
    const std::int64_t max_y = to_index(command.local_point.y + command.radius,
                                        spec_.minimum.y, spec_.cell_size.y, spec_.cells_y);
    const std::int64_t min_z = to_index(command.local_point.z - command.radius,
                                        spec_.minimum.z, spec_.cell_size.z, spec_.cells_z);
    const std::int64_t max_z = to_index(command.local_point.z + command.radius,
                                        spec_.minimum.z, spec_.cell_size.z, spec_.cells_z);
    const float radius_squared = command.radius * command.radius;
    for (std::int64_t z = min_z; z <= max_z; ++z) {
        for (std::int64_t y = min_y; y <= max_y; ++y) {
            for (std::int64_t x = min_x; x <= max_x; ++x) {
                DamageCell& cell = cells_[index(static_cast<std::uint32_t>(x),
                                                static_cast<std::uint32_t>(y),
                                                static_cast<std::uint32_t>(z))];
                const foundation::Vec3 delta = subtract(cell.center, command.local_point);
                const float distance_squared = length_squared(delta);
                if (distance_squared > radius_squared) {
                    continue;
                }
                const float distance = std::sqrt(std::max(0.0F, distance_squared));
                const float falloff = 1.0F - distance / command.radius;
                const float weight = falloff * falloff;
                const float before = std::max({cell.crush, cell.crack, cell.rear, cell.weakness});
                cell.crush += command.energy * weight * command.coefficients.crush;
                cell.crack += command.energy * weight * command.coefficients.crack;
                if (command.rear_surface) {
                    cell.rear += command.energy * weight * command.coefficients.rear;
                }
                cell.weakness = std::clamp(
                    cell.weakness + command.energy * weight * command.coefficients.weakness,
                    0.0F, 1.0F);
                const float after = std::max({cell.crush, cell.crack, cell.rear, cell.weakness});
                result.aggregate_damage_delta += std::max(0.0F, after - before);
                ++result.affected_cells;
            }
        }
    }
    aggregate_damage_ += result.aggregate_damage_delta;
    if (command.through_channel && command.depth > 0.0F && spec_.max_holes > holes_.size()) {
        const auto duplicate = std::find_if(holes_.begin(), holes_.end(),
                                            [&command](const HoleRecord& hole) {
                                                return hole.id == command.event_id;
                                            });
        if (duplicate == holes_.end()) {
            const float volume = 3.14159265358979323846F * command.radius * command.radius *
                                 command.depth;
            holes_.push_back({command.event_id, command.local_point,
                              normalized(command.direction), command.radius, command.depth, volume});
            removed_volume_ += volume;
            result.removed_volume_delta = volume;
            result.hole_added = true;
        }
    } else if (command.through_channel) {
        result.clipped = true;
    }
    (void)extent;
    return result;
}

const DamageCell* DamageField::cell(std::uint32_t x,
                                    std::uint32_t y,
                                    std::uint32_t z) const noexcept {
    if (x >= spec_.cells_x || y >= spec_.cells_y || z >= spec_.cells_z) {
        return nullptr;
    }
    return &cells_[index(x, y, z)];
}

std::size_t DamageField::index(std::uint32_t x,
                               std::uint32_t y,
                               std::uint32_t z) const noexcept {
    return (static_cast<std::size_t>(z) * spec_.cells_y + y) * spec_.cells_x + x;
}

} // namespace genomes::destruction
