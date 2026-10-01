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

struct WorldSaveMetadata final {
    std::uint32_t generator_version{0U};
    proc::Seed seed{0U};
    foundation::SimulationTick tick{};
    std::uint64_t content_hash{0U};
    std::uint64_t catalog_hash{0U};

    [[nodiscard]] bool valid() const noexcept;
};

struct WorldSaveLimits final {
    std::size_t max_file_bytes{WorldSaveMaximumBytes + WorldSaveHeaderBytes};
    std::uint32_t max_regions{1'000'000U};
    std::uint32_t max_entities{1'000'000U};
    std::uint32_t max_destroyed_objects{1'000'000U};
    std::size_t max_working_bytes{WorldSaveMaximumBytes * 2U};

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
    WorldSaveMetadata metadata{};
    std::vector<WorldSaveRegion> regions;
    std::vector<WorldSaveEntity> entities;

    [[nodiscard]] bool valid() const noexcept;
};

class WorldSaveCodec final {
public:
    [[nodiscard]] static foundation::Result<std::vector<std::byte>, foundation::Error> serialize(
        const WorldSaveModel& model, WorldSaveLimits limits = {});
    [[nodiscard]] static foundation::Result<WorldSaveModel, foundation::Error> deserialize(
        std::span<const std::byte> bytes, WorldSaveLimits limits = {});
    [[nodiscard]] static foundation::Result<void, foundation::Error> saveFile(
        const std::filesystem::path& path, const WorldSaveModel& model,
        WorldSaveLimits limits = {});
    [[nodiscard]] static foundation::Result<WorldSaveModel, foundation::Error> loadFile(
        const std::filesystem::path& path, WorldSaveLimits limits = {});
};

} // namespace genomes::world
