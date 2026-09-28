#include <genomes/compute/FieldAtlas.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace genomes::compute {

namespace {

using foundation::Error;
using foundation::ErrorCode;

[[nodiscard]] Error error(ErrorCode code, std::string_view message) noexcept {
    return Error{code, message};
}

[[nodiscard]] bool isFloatFormat(FieldFormat format) noexcept {
    return format == FieldFormat::R32Float || format == FieldFormat::R16Float;
}

[[nodiscard]] bool isKnownFormat(FieldFormat format) noexcept {
    return format == FieldFormat::R32Float || format == FieldFormat::R16Float ||
           format == FieldFormat::R32Uint;
}

[[nodiscard]] bool isKnownAuthority(FieldAuthority authority) noexcept {
    return authority == FieldAuthority::DerivedAdvisory ||
           authority == FieldAuthority::DerivedAuthoritativeWithCpuFallback;
}

[[nodiscard]] bool checkedCellCount(const FieldAtlasDescriptor& descriptor,
                                    std::size_t& count) noexcept {
    const auto width = static_cast<std::size_t>(descriptor.width);
    const auto height = static_cast<std::size_t>(descriptor.height);
    const auto layers = static_cast<std::size_t>(descriptor.layers);
    if (width == 0U || height == 0U || layers == 0U) {
        return false;
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    const auto plane = width * height;
    if (plane > std::numeric_limits<std::size_t>::max() / layers) {
        return false;
    }
    count = plane * layers;
    return true;
}

} // namespace

bool FieldAtlasDescriptor::valid() const noexcept {
    std::size_t count = 0U;
    return id != 0U && isKnownFormat(format) && isKnownAuthority(authority) &&
           checkedCellCount(*this, count) &&
           std::isfinite(world_origin.x) && std::isfinite(world_origin.y) &&
           std::isfinite(cell_size.x) && std::isfinite(cell_size.y) &&
           cell_size.x > 0.0F && cell_size.y > 0.0F &&
           tile_width > 0U && tile_width <= 1024U && tile_height > 0U &&
           tile_height <= 1024U &&
           count <= 64U * 1024U * 1024U;
}

foundation::Result<void, foundation::Error> FieldAtlas::define(
    const FieldAtlasDescriptor& descriptor) {
    if (!descriptor.valid()) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::InvalidArgument, "invalid field atlas descriptor"));
    }
    std::size_t cell_count = 0U;
    (void)checkedCellCount(descriptor, cell_count);

    // Re-defining a field is a recreate operation.  Do not let uploaders
    // apply dirty records belonging to the old allocation after this point.
    dirty_regions_.erase(
        std::remove_if(dirty_regions_.begin(), dirty_regions_.end(),
                       [&descriptor](const FieldDirtyRegion& dirty) {
                           return dirty.id == descriptor.id;
                       }),
        dirty_regions_.end());

    FieldStorage storage{};
    storage.descriptor = descriptor;
    storage.revision = 1U;
    storage.generation = next_generation_++;
    if (isFloatFormat(descriptor.format)) {
        storage.values = std::vector<float>(cell_count, 0.0F);
    } else {
        storage.values = std::vector<std::uint32_t>(cell_count, 0U);
    }
    fields_[descriptor.id] = std::move(storage);
    dirty_regions_.push_back(FieldDirtyRegion{
        descriptor.id,
        FieldRegion{0U, 0U, 0U, descriptor.width, descriptor.height,
                    descriptor.layers},
        1U,
    });
    return foundation::Result<void, Error>::success();
}

bool FieldAtlas::remove(FieldId id) noexcept {
    const bool removed = fields_.erase(id) != 0U;
    if (removed) {
        dirty_regions_.erase(
            std::remove_if(dirty_regions_.begin(), dirty_regions_.end(),
                           [id](const FieldDirtyRegion& dirty) { return dirty.id == id; }),
            dirty_regions_.end());
    }
    return removed;
}

bool FieldAtlas::contains(FieldId id) const noexcept {
    return fields_.find(id) != fields_.end();
}

std::optional<FieldAtlasDescriptor> FieldAtlas::descriptor(FieldId id) const {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return std::nullopt;
    }
    return iterator->second.descriptor;
}

std::uint64_t FieldAtlas::revision(FieldId id) const noexcept {
    const auto iterator = fields_.find(id);
    return iterator == fields_.end() ? 0U : iterator->second.revision;
}

foundation::Result<FieldCell, foundation::Error> FieldAtlas::worldToCell(
    FieldId id, foundation::Vec2 world, std::uint32_t layer) const {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<FieldCell, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    const auto& descriptor = iterator->second.descriptor;
    if (layer >= descriptor.layers) {
        return foundation::Result<FieldCell, Error>::failure(
            error(ErrorCode::OutOfRange, "field layer outside atlas"));
    }
    const auto local_x = (world.x - descriptor.world_origin.x) / descriptor.cell_size.x;
    const auto local_y = (world.y - descriptor.world_origin.y) / descriptor.cell_size.y;
    if (!std::isfinite(local_x) || !std::isfinite(local_y) || local_x < 0.0F ||
        local_y < 0.0F || local_x >= static_cast<float>(descriptor.width) ||
        local_y >= static_cast<float>(descriptor.height)) {
        return foundation::Result<FieldCell, Error>::failure(
            error(ErrorCode::OutOfRange, "world position outside field atlas"));
    }
    return foundation::Result<FieldCell, Error>::success(
        FieldCell{layer, static_cast<std::uint32_t>(std::floor(local_x)),
                  static_cast<std::uint32_t>(std::floor(local_y))});
}

foundation::Result<void, foundation::Error> FieldAtlas::clear(FieldId id, float value) {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (!isFloatFormat(iterator->second.descriptor.format)) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not floating point"));
    }
    auto& values = std::get<std::vector<float>>(iterator->second.values);
    std::fill(values.begin(), values.end(), value);
    ++iterator->second.revision;
    dirty_regions_.push_back(
        FieldDirtyRegion{id,
                         FieldRegion{0U, 0U, 0U, iterator->second.descriptor.width,
                                     iterator->second.descriptor.height,
                                     iterator->second.descriptor.layers},
                         iterator->second.revision});
    return foundation::Result<void, Error>::success();
}

foundation::Result<void, foundation::Error> FieldAtlas::clearUint(FieldId id,
                                                                   std::uint32_t value) {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (iterator->second.descriptor.format != FieldFormat::R32Uint) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not unsigned integer"));
    }
    auto& values = std::get<std::vector<std::uint32_t>>(iterator->second.values);
    std::fill(values.begin(), values.end(), value);
    ++iterator->second.revision;
    dirty_regions_.push_back(
        FieldDirtyRegion{id,
                         FieldRegion{0U, 0U, 0U, iterator->second.descriptor.width,
                                     iterator->second.descriptor.height,
                                     iterator->second.descriptor.layers},
                         iterator->second.revision});
    return foundation::Result<void, Error>::success();
}

foundation::Result<void, foundation::Error> FieldAtlas::validateRegion(
    const FieldAtlasDescriptor& descriptor, FieldRegion region) {
    if (region.width == 0U || region.height == 0U || region.layer_count == 0U ||
        region.layer >= descriptor.layers ||
        region.layer_count > descriptor.layers - region.layer ||
        region.x >= descriptor.width || region.y >= descriptor.height ||
        region.width > descriptor.width - region.x ||
        region.height > descriptor.height - region.y) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::OutOfRange, "field region outside atlas"));
    }
    return foundation::Result<void, Error>::success();
}

std::size_t FieldAtlas::offset(const FieldAtlasDescriptor& descriptor, FieldRegion region,
                               std::uint32_t local_x, std::uint32_t local_y,
                               std::uint32_t local_layer) noexcept {
    const auto width = static_cast<std::size_t>(descriptor.width);
    const auto plane = width * static_cast<std::size_t>(descriptor.height);
    return static_cast<std::size_t>(region.layer + local_layer) * plane +
           static_cast<std::size_t>(region.y + local_y) * width + region.x + local_x;
}

foundation::Result<void, foundation::Error> FieldAtlas::writeFloat(
    FieldId id, FieldRegion region, std::span<const float> values) {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (!isFloatFormat(iterator->second.descriptor.format)) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not floating point"));
    }
    const auto valid = validateRegion(iterator->second.descriptor, region);
    if (!valid) {
        return valid;
    }
    const auto expected = static_cast<std::size_t>(region.width) * region.height *
                          region.layer_count;
    if (values.size() != expected) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::InvalidArgument, "field value count does not match region"));
    }
    auto& target = std::get<std::vector<float>>(iterator->second.values);
    for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
        for (std::uint32_t y = 0U; y < region.height; ++y) {
            const auto source_offset =
                (static_cast<std::size_t>(layer) * region.height + y) * region.width;
            for (std::uint32_t x = 0U; x < region.width; ++x) {
                target[offset(iterator->second.descriptor, region, x, y, layer)] =
                    values[source_offset + x];
            }
        }
    }
    ++iterator->second.revision;
    dirty_regions_.push_back(FieldDirtyRegion{id, region, iterator->second.revision});
    return foundation::Result<void, Error>::success();
}

foundation::Result<void, foundation::Error> FieldAtlas::writeUint(
    FieldId id, FieldRegion region, std::span<const std::uint32_t> values) {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (iterator->second.descriptor.format != FieldFormat::R32Uint) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not unsigned integer"));
    }
    const auto valid = validateRegion(iterator->second.descriptor, region);
    if (!valid) {
        return valid;
    }
    const auto expected = static_cast<std::size_t>(region.width) * region.height *
                          region.layer_count;
    if (values.size() != expected) {
        return foundation::Result<void, Error>::failure(
            error(ErrorCode::InvalidArgument, "field value count does not match region"));
    }
    auto& target = std::get<std::vector<std::uint32_t>>(iterator->second.values);
    for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
        for (std::uint32_t y = 0U; y < region.height; ++y) {
            const auto source_offset =
                (static_cast<std::size_t>(layer) * region.height + y) * region.width;
            for (std::uint32_t x = 0U; x < region.width; ++x) {
                target[offset(iterator->second.descriptor, region, x, y, layer)] =
                    values[source_offset + x];
            }
        }
    }
    ++iterator->second.revision;
    dirty_regions_.push_back(FieldDirtyRegion{id, region, iterator->second.revision});
    return foundation::Result<void, Error>::success();
}

foundation::Result<std::vector<float>, foundation::Error> FieldAtlas::readFloat(
    FieldId id, FieldRegion region) const {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<std::vector<float>, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (!isFloatFormat(iterator->second.descriptor.format)) {
        return foundation::Result<std::vector<float>, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not floating point"));
    }
    const auto valid = validateRegion(iterator->second.descriptor, region);
    if (!valid) {
        return foundation::Result<std::vector<float>, Error>::failure(valid.error());
    }
    std::vector<float> result(static_cast<std::size_t>(region.width) * region.height *
                              region.layer_count);
    const auto& source = std::get<std::vector<float>>(iterator->second.values);
    for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
        for (std::uint32_t y = 0U; y < region.height; ++y) {
            for (std::uint32_t x = 0U; x < region.width; ++x) {
                result[(static_cast<std::size_t>(layer) * region.height + y) * region.width +
                       x] = source[offset(iterator->second.descriptor, region, x, y, layer)];
            }
        }
    }
    return foundation::Result<std::vector<float>, Error>::success(std::move(result));
}

foundation::Result<std::vector<std::uint32_t>, foundation::Error> FieldAtlas::readUint(
    FieldId id, FieldRegion region) const {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<std::vector<std::uint32_t>, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (iterator->second.descriptor.format != FieldFormat::R32Uint) {
        return foundation::Result<std::vector<std::uint32_t>, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not unsigned integer"));
    }
    const auto valid = validateRegion(iterator->second.descriptor, region);
    if (!valid) {
        return foundation::Result<std::vector<std::uint32_t>, Error>::failure(valid.error());
    }
    std::vector<std::uint32_t> result(static_cast<std::size_t>(region.width) * region.height *
                                      region.layer_count);
    const auto& source = std::get<std::vector<std::uint32_t>>(iterator->second.values);
    for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
        for (std::uint32_t y = 0U; y < region.height; ++y) {
            for (std::uint32_t x = 0U; x < region.width; ++x) {
                result[(static_cast<std::size_t>(layer) * region.height + y) * region.width +
                       x] = source[offset(iterator->second.descriptor, region, x, y, layer)];
            }
        }
    }
    return foundation::Result<std::vector<std::uint32_t>, Error>::success(std::move(result));
}

foundation::Result<float, foundation::Error> FieldAtlas::sampleFloat(
    FieldId id, foundation::Vec2 world, std::uint32_t layer) const {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<float, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    if (!isFloatFormat(iterator->second.descriptor.format)) {
        return foundation::Result<float, Error>::failure(
            error(ErrorCode::Unsupported, "field format is not floating point"));
    }
    const auto cell = worldToCell(id, world, layer);
    if (!cell) {
        return foundation::Result<float, Error>::failure(cell.error());
    }
    const auto& descriptor = iterator->second.descriptor;
    const FieldRegion cell_region{cell.value().layer, cell.value().x, cell.value().y, 1U,
                                  1U};
    const auto& values = std::get<std::vector<float>>(iterator->second.values);
    return foundation::Result<float, Error>::success(
        values[offset(descriptor, cell_region, 0U, 0U)]);
}

foundation::Result<FieldReadbackToken, foundation::Error> FieldAtlas::requestReadback(
    FieldId id, FieldRegion region) {
    const auto iterator = fields_.find(id);
    if (iterator == fields_.end()) {
        return foundation::Result<FieldReadbackToken, Error>::failure(
            error(ErrorCode::NotFound, "field atlas id not found"));
    }
    const auto valid = validateRegion(iterator->second.descriptor, region);
    if (!valid) {
        return foundation::Result<FieldReadbackToken, Error>::failure(valid.error());
    }

    const FieldReadbackToken token = next_readback_token_++;
    ReadbackStorage readback{};
    readback.id = id;
    readback.region = region;
    readback.source_revision = iterator->second.revision;
    readback.generation = iterator->second.generation;
    if (isFloatFormat(iterator->second.descriptor.format)) {
        const auto& source = std::get<std::vector<float>>(iterator->second.values);
        std::vector<float> values(static_cast<std::size_t>(region.width) * region.height *
                                  region.layer_count);
        for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
            for (std::uint32_t y = 0U; y < region.height; ++y) {
                for (std::uint32_t x = 0U; x < region.width; ++x) {
                    values[(static_cast<std::size_t>(layer) * region.height + y) * region.width +
                           x] = source[offset(iterator->second.descriptor, region, x, y, layer)];
                }
            }
        }
        readback.values = std::move(values);
    } else {
        const auto& source = std::get<std::vector<std::uint32_t>>(iterator->second.values);
        std::vector<std::uint32_t> values(static_cast<std::size_t>(region.width) * region.height *
                                          region.layer_count);
        for (std::uint32_t layer = 0U; layer < region.layer_count; ++layer) {
            for (std::uint32_t y = 0U; y < region.height; ++y) {
                for (std::uint32_t x = 0U; x < region.width; ++x) {
                    values[(static_cast<std::size_t>(layer) * region.height + y) * region.width +
                           x] = source[offset(iterator->second.descriptor, region, x, y, layer)];
                }
            }
        }
        readback.values = std::move(values);
    }
    readbacks_.emplace(token, std::move(readback));
    return foundation::Result<FieldReadbackToken, Error>::success(token);
}

foundation::Result<FieldReadback, foundation::Error> FieldAtlas::pollReadback(
    FieldReadbackToken token) const {
    const auto iterator = readbacks_.find(token);
    if (iterator == readbacks_.end()) {
        return foundation::Result<FieldReadback, Error>::failure(
            error(ErrorCode::NotFound, "field readback token not found"));
    }

    FieldReadback result{};
    result.token = token;
    result.id = iterator->second.id;
    result.region = iterator->second.region;
    result.source_revision = iterator->second.source_revision;
    result.values = iterator->second.values;
    const auto field = fields_.find(result.id);
    result.state = field == fields_.end() ||
                           field->second.generation != iterator->second.generation ||
                           field->second.revision != result.source_revision
                       ? FieldReadbackState::Stale
                       : FieldReadbackState::Ready;
    return foundation::Result<FieldReadback, Error>::success(std::move(result));
}

bool FieldAtlas::releaseReadback(FieldReadbackToken token) noexcept {
    return readbacks_.erase(token) != 0U;
}

std::vector<FieldDirtyRegion> FieldAtlas::consumeDirtyRegions() {
    std::vector<FieldDirtyRegion> result;
    result.swap(dirty_regions_);
    return result;
}

} // namespace genomes::compute
