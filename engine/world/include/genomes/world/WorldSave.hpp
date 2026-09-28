#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Handle.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>
#include <genomes/world/WorldPosition.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace genomes::world {

inline constexpr std::uint32_t WorldSaveMagic = 0x31565347U; // GSV1
inline constexpr std::uint32_t WorldSaveSchemaVersion = 1U;
inline constexpr std::size_t WorldSaveHeaderBytes = 64U;
inline constexpr std::size_t WorldSaveMaximumBytes = 64U * 1024U * 1024U;

struct WorldSaveHeader final {
    std::uint32_t magic{WorldSaveMagic};
    std::uint32_t schema_version{WorldSaveSchemaVersion};
    std::uint32_t generator_version{0U};
    proc::Seed seed{0U};
    foundation::SimulationTick tick{};
    std::uint64_t content_hash{0U};
    std::uint64_t catalog_hash{0U};
    std::uint32_t region_count{0U};
    std::uint32_t entity_count{0U};
    std::uint32_t payload_bytes{0U};
    std::uint64_t payload_checksum{0U};

    [[nodiscard]] bool valid() const noexcept;
};

struct WorldSaveRegion final {
    RegionId id{};
    std::uint64_t content_hash{0U};
    std::vector<foundation::StableId> destroyed_objects;
};

struct WorldSaveEntity final {
    foundation::Handle<struct WorldSaveEntityTag> id{};
    foundation::Vec3 position{};
    std::uint32_t flags{0U};
    foundation::StableId equipment_id{0U};
};

struct WorldSaveModel final {
    WorldSaveHeader header{};
    std::vector<WorldSaveRegion> regions;
    std::vector<WorldSaveEntity> entities;

    [[nodiscard]] bool valid() const noexcept;
};

class WorldSaveCodec final {
public:
    [[nodiscard]] static foundation::Result<std::vector<std::byte>, foundation::Error> serialize(
        const WorldSaveModel& model);
    [[nodiscard]] static foundation::Result<WorldSaveModel, foundation::Error> deserialize(
        std::span<const std::byte> bytes);
    [[nodiscard]] static foundation::Result<void, foundation::Error> saveFile(
        const std::filesystem::path& path, const WorldSaveModel& model);
    [[nodiscard]] static foundation::Result<WorldSaveModel, foundation::Error> loadFile(
        const std::filesystem::path& path);
};

} // namespace genomes::world
