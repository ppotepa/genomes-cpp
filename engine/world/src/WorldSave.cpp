#include <genomes/world/WorldSave.hpp>
#include <genomes/io/AtomicFile.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace genomes::world {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] std::uint64_t checksum(std::span<const std::byte> bytes) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint8_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

void appendU32(std::vector<std::byte>& output, std::uint32_t value) {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        output.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void appendU64(std::vector<std::byte>& output, std::uint64_t value) {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        output.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void appendF32(std::vector<std::byte>& output, float value) {
    appendU32(output, std::bit_cast<std::uint32_t>(value));
}

class Cursor final {
public:
    explicit Cursor(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool readU32(std::uint32_t& result) noexcept {
        if (remaining() < 4U) {
            return false;
        }
        result = static_cast<std::uint32_t>(bytes_[offset_]) |
                 (static_cast<std::uint32_t>(bytes_[offset_ + 1U]) << 8U) |
                 (static_cast<std::uint32_t>(bytes_[offset_ + 2U]) << 16U) |
                 (static_cast<std::uint32_t>(bytes_[offset_ + 3U]) << 24U);
        offset_ += 4U;
        return true;
    }

    [[nodiscard]] bool readU64(std::uint64_t& result) noexcept {
        if (remaining() < 8U) {
            return false;
        }
        result = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            result |= static_cast<std::uint64_t>(bytes_[offset_ + shift / 8U]) << shift;
        }
        offset_ += 8U;
        return true;
    }

    [[nodiscard]] bool readF32(float& result) noexcept {
        std::uint32_t bits = 0U;
        if (!readU32(bits)) {
            return false;
        }
        result = std::bit_cast<float>(bits);
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }
    [[nodiscard]] bool empty() const noexcept { return offset_ == bytes_.size(); }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_{0U};
};

[[nodiscard]] foundation::Error corrupt() noexcept {
    return {foundation::ErrorCode::InvalidArgument, "corrupt world save package"};
}

} // namespace

bool WorldSaveHeader::valid() const noexcept {
    return magic == WorldSaveMagic && schema_version == WorldSaveSchemaVersion &&
           generator_version != 0U && seed != 0U && content_hash != 0U &&
           payload_bytes <= WorldSaveMaximumBytes && region_count <= 1'000'000U &&
           entity_count <= 1'000'000U;
}

bool WorldSaveModel::valid() const noexcept {
    if (!header.valid() || header.region_count != regions.size() ||
        header.entity_count != entities.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < regions.size(); ++index) {
        const WorldSaveRegion& region = regions[index];
        if (!region.id.isValid() ||
            (index > 0U && regions[index - 1U].id.value() >= region.id.value())) {
            return false;
        }
        for (std::size_t destroyed = 0U; destroyed < region.destroyed_objects.size(); ++destroyed) {
            if (region.destroyed_objects[destroyed] == 0U ||
                (destroyed > 0U && region.destroyed_objects[destroyed - 1U] >=
                                      region.destroyed_objects[destroyed])) {
                return false;
            }
        }
    }
    for (std::size_t index = 0U; index < entities.size(); ++index) {
        if (!entities[index].id.isValid() || !finite(entities[index].position) ||
            (index > 0U && entities[index - 1U].id.packed() >= entities[index].id.packed())) {
            return false;
        }
    }
    return true;
}

foundation::Result<std::vector<std::byte>, foundation::Error> WorldSaveCodec::serialize(
    const WorldSaveModel& model) {
    if (!model.header.valid() || model.header.region_count != 0U ||
        model.header.entity_count != 0U) {
        return foundation::Result<std::vector<std::byte>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world save metadata"});
    }
    std::vector<WorldSaveRegion> regions = model.regions;
    std::vector<WorldSaveEntity> entities = model.entities;
    std::sort(regions.begin(), regions.end(), [](const auto& left, const auto& right) {
        return left.id.value() < right.id.value();
    });
    std::sort(entities.begin(), entities.end(), [](const auto& left, const auto& right) {
        return left.id.packed() < right.id.packed();
    });
    for (WorldSaveRegion& region : regions) {
        std::sort(region.destroyed_objects.begin(), region.destroyed_objects.end());
    }
    WorldSaveModel canonical = model;
    canonical.regions = std::move(regions);
    canonical.entities = std::move(entities);
    canonical.header.region_count = static_cast<std::uint32_t>(canonical.regions.size());
    canonical.header.entity_count = static_cast<std::uint32_t>(canonical.entities.size());
    if (!canonical.valid()) {
        return foundation::Result<std::vector<std::byte>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world save state"});
    }

    std::vector<std::byte> payload;
    payload.reserve(64U + canonical.regions.size() * 32U + canonical.entities.size() * 32U);
    for (const WorldSaveRegion& region : canonical.regions) {
        appendU64(payload, region.id.value());
        appendU64(payload, region.content_hash);
        appendU32(payload, static_cast<std::uint32_t>(region.destroyed_objects.size()));
        for (const foundation::StableId object : region.destroyed_objects) {
            appendU64(payload, object);
        }
    }
    for (const WorldSaveEntity& entity : canonical.entities) {
        appendU32(payload, entity.id.index);
        appendU32(payload, entity.id.generation);
        appendF32(payload, entity.position.x);
        appendF32(payload, entity.position.y);
        appendF32(payload, entity.position.z);
        appendU32(payload, entity.flags);
        appendU64(payload, entity.equipment_id);
    }
    if (payload.size() > WorldSaveMaximumBytes || payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return foundation::Result<std::vector<std::byte>, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "world save payload is too large"});
    }
    canonical.header.payload_bytes = static_cast<std::uint32_t>(payload.size());
    canonical.header.payload_checksum = checksum(payload);

    std::vector<std::byte> output;
    output.reserve(WorldSaveHeaderBytes + payload.size());
    appendU32(output, canonical.header.magic);
    appendU32(output, canonical.header.schema_version);
    appendU32(output, canonical.header.generator_version);
    appendU64(output, canonical.header.seed);
    appendU64(output, canonical.header.tick.value);
    appendU64(output, canonical.header.content_hash);
    appendU64(output, canonical.header.catalog_hash);
    appendU32(output, canonical.header.region_count);
    appendU32(output, canonical.header.entity_count);
    appendU32(output, canonical.header.payload_bytes);
    appendU64(output, canonical.header.payload_checksum);
    output.insert(output.end(), payload.begin(), payload.end());
    return foundation::Result<std::vector<std::byte>, foundation::Error>::success(std::move(output));
}

foundation::Result<WorldSaveModel, foundation::Error> WorldSaveCodec::deserialize(
    std::span<const std::byte> bytes) {
    if (bytes.size() < WorldSaveHeaderBytes || bytes.size() > WorldSaveMaximumBytes + WorldSaveHeaderBytes) {
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
    }
    Cursor header_cursor(bytes.first(WorldSaveHeaderBytes));
    WorldSaveModel model{};
    std::uint64_t seed = 0U;
    if (!header_cursor.readU32(model.header.magic) || !header_cursor.readU32(model.header.schema_version) ||
        !header_cursor.readU32(model.header.generator_version) || !header_cursor.readU64(seed) ||
        !header_cursor.readU64(model.header.tick.value) ||
        !header_cursor.readU64(model.header.content_hash) ||
        !header_cursor.readU64(model.header.catalog_hash) ||
        !header_cursor.readU32(model.header.region_count) ||
        !header_cursor.readU32(model.header.entity_count) ||
        !header_cursor.readU32(model.header.payload_bytes) ||
        !header_cursor.readU64(model.header.payload_checksum) || !header_cursor.empty()) {
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
    }
    model.header.seed = seed;
    if (!model.header.valid() || bytes.size() != WorldSaveHeaderBytes + model.header.payload_bytes ||
        checksum(bytes.subspan(WorldSaveHeaderBytes)) != model.header.payload_checksum) {
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
    }
    Cursor cursor(bytes.subspan(WorldSaveHeaderBytes));
    model.regions.reserve(model.header.region_count);
    model.entities.reserve(model.header.entity_count);
    for (std::uint32_t index = 0U; index < model.header.region_count; ++index) {
        std::uint64_t id = 0U;
        std::uint64_t content_hash = 0U;
        std::uint32_t destroyed_count = 0U;
        if (!cursor.readU64(id) || !cursor.readU64(content_hash) || !cursor.readU32(destroyed_count) ||
            destroyed_count > cursor.remaining() / sizeof(std::uint64_t)) {
            return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
        }
        WorldSaveRegion region{RegionId(id), content_hash, {}};
        region.destroyed_objects.reserve(destroyed_count);
        for (std::uint32_t destroyed = 0U; destroyed < destroyed_count; ++destroyed) {
            std::uint64_t object = 0U;
            if (!cursor.readU64(object)) {
                return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
            }
            region.destroyed_objects.push_back(object);
        }
        model.regions.push_back(std::move(region));
    }
    constexpr std::size_t entity_bytes = 32U;
    if (model.header.entity_count > cursor.remaining() / entity_bytes) {
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
    }
    for (std::uint32_t index = 0U; index < model.header.entity_count; ++index) {
        WorldSaveEntity entity{};
        std::uint32_t generation = 0U;
        if (!cursor.readU32(entity.id.index) || !cursor.readU32(generation) ||
            !cursor.readF32(entity.position.x) || !cursor.readF32(entity.position.y) ||
            !cursor.readF32(entity.position.z) || !cursor.readU32(entity.flags) ||
            !cursor.readU64(entity.equipment_id)) {
            return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
        }
        entity.id.generation = generation;
        model.entities.push_back(entity);
    }
    if (!cursor.empty() || !model.valid()) {
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
    }
    return foundation::Result<WorldSaveModel, foundation::Error>::success(std::move(model));
}

foundation::Result<void, foundation::Error> WorldSaveCodec::saveFile(
    const std::filesystem::path& path, const WorldSaveModel& model) {
    const auto serialized = serialize(model);
    if (!serialized) {
        return foundation::Result<void, foundation::Error>::failure(serialized.error());
    }
    const auto& bytes = serialized.value();
    return io::AtomicFile::write(
        path, bytes, io::AtomicFileConfig{WorldSaveMaximumBytes + WorldSaveHeaderBytes});
}

foundation::Result<WorldSaveModel, foundation::Error> WorldSaveCodec::loadFile(
    const std::filesystem::path& path) {
    const auto bytes = io::AtomicFile::read(
        path, io::AtomicFileConfig{WorldSaveMaximumBytes + WorldSaveHeaderBytes});
    if (!bytes) {
        if (bytes.error().code == foundation::ErrorCode::OutOfRange ||
            bytes.error().code == foundation::ErrorCode::Internal) {
            return foundation::Result<WorldSaveModel, foundation::Error>::failure(corrupt());
        }
        return foundation::Result<WorldSaveModel, foundation::Error>::failure(bytes.error());
    }
    return deserialize(bytes.value());
}

} // namespace genomes::world
