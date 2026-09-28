#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <variant>
#include <vector>

namespace genomes::compute {

using FieldId = foundation::StableId;
using FieldReadbackToken = std::uint64_t;

enum class FieldAuthority : std::uint8_t {
    DerivedAdvisory,
    DerivedAuthoritativeWithCpuFallback,
};

enum class FieldFormat : std::uint8_t {
    R32Float,
    R16Float,
    R32Uint,
};

struct FieldAtlasDescriptor final {
    FieldId id{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::uint32_t layers{1U};
    foundation::Vec2 world_origin{};
    foundation::Vec2 cell_size{1.0F, 1.0F};
    FieldFormat format{FieldFormat::R32Float};
    FieldAuthority authority{FieldAuthority::DerivedAdvisory};
    // Tile dimensions are logical atlas metadata.  A backend may choose a
    // different physical tile shape, but uploaders can use these dimensions
    // to expand a dirty rectangle without depending on a texture API.
    std::uint32_t tile_width{32U};
    std::uint32_t tile_height{32U};

    [[nodiscard]] bool valid() const noexcept;
};

struct FieldRegion final {
    std::uint32_t layer{0U};
    std::uint32_t x{0U};
    std::uint32_t y{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::uint32_t layer_count{1U};
};

struct FieldDirtyRegion final {
    FieldId id{0U};
    FieldRegion region{};
    std::uint64_t revision{0U};
};

struct FieldCell final {
    std::uint32_t layer{0U};
    std::uint32_t x{0U};
    std::uint32_t y{0U};
};

enum class FieldReadbackState : std::uint8_t {
    Pending,
    Ready,
    Stale,
};

using FieldReadbackData =
    std::variant<std::vector<float>, std::vector<std::uint32_t>>;

// A readback is deliberately returned with the revision captured at request
// time.  GPU implementations may leave it Pending; the CPU reference backend
// is already Ready because its storage is the canonical fallback.
struct FieldReadback final {
    FieldReadbackToken token{0U};
    FieldId id{0U};
    FieldRegion region{};
    std::uint64_t source_revision{0U};
    FieldReadbackState state{FieldReadbackState::Pending};
    FieldReadbackData values{};
};

class FieldAtlas final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> define(
        const FieldAtlasDescriptor& descriptor);

    [[nodiscard]] bool remove(FieldId id) noexcept;
    [[nodiscard]] bool contains(FieldId id) const noexcept;
    [[nodiscard]] std::optional<FieldAtlasDescriptor> descriptor(FieldId id) const;
    [[nodiscard]] std::uint64_t revision(FieldId id) const noexcept;

    [[nodiscard]] foundation::Result<FieldCell, foundation::Error> worldToCell(
        FieldId id, foundation::Vec2 world, std::uint32_t layer = 0U) const;

    [[nodiscard]] foundation::Result<void, foundation::Error> clear(
        FieldId id, float value = 0.0F);
    [[nodiscard]] foundation::Result<void, foundation::Error> clearUint(
        FieldId id, std::uint32_t value = 0U);

    [[nodiscard]] foundation::Result<void, foundation::Error> writeFloat(
        FieldId id, FieldRegion region, std::span<const float> values);
    [[nodiscard]] foundation::Result<void, foundation::Error> writeUint(
        FieldId id, FieldRegion region, std::span<const std::uint32_t> values);

    [[nodiscard]] foundation::Result<std::vector<float>, foundation::Error> readFloat(
        FieldId id, FieldRegion region) const;
    [[nodiscard]] foundation::Result<std::vector<std::uint32_t>, foundation::Error> readUint(
        FieldId id, FieldRegion region) const;

    [[nodiscard]] foundation::Result<float, foundation::Error> sampleFloat(
        FieldId id, foundation::Vec2 world, std::uint32_t layer = 0U) const;

    // The token is an explicit staging/fence boundary for uploaders.  The CPU
    // backend snapshots immediately, while a GPU backend can preserve the
    // same API and report Pending until its fence is signalled.
    [[nodiscard]] foundation::Result<FieldReadbackToken, foundation::Error> requestReadback(
        FieldId id, FieldRegion region);
    [[nodiscard]] foundation::Result<FieldReadback, foundation::Error> pollReadback(
        FieldReadbackToken token) const;
    [[nodiscard]] bool releaseReadback(FieldReadbackToken token) noexcept;

    // Uploaders consume these records to update a GPU atlas. The CPU atlas is
    // still the reference/fallback storage and remains valid without a GPU.
    [[nodiscard]] std::vector<FieldDirtyRegion> consumeDirtyRegions();

private:
    struct FieldStorage final {
        FieldAtlasDescriptor descriptor{};
        std::uint64_t revision{0U};
        std::uint64_t generation{0U};
        FieldReadbackData values{};
    };

    struct ReadbackStorage final {
        FieldId id{0U};
        FieldRegion region{};
        std::uint64_t source_revision{0U};
        std::uint64_t generation{0U};
        FieldReadbackData values{};
    };

    [[nodiscard]] static foundation::Result<void, foundation::Error> validateRegion(
        const FieldAtlasDescriptor&, FieldRegion);
    [[nodiscard]] static std::size_t offset(const FieldAtlasDescriptor&, FieldRegion,
                                             std::uint32_t local_x,
                                             std::uint32_t local_y,
                                             std::uint32_t local_layer = 0U) noexcept;

    std::unordered_map<FieldId, FieldStorage> fields_;
    std::vector<FieldDirtyRegion> dirty_regions_;
    std::unordered_map<FieldReadbackToken, ReadbackStorage> readbacks_;
    FieldReadbackToken next_readback_token_{1U};
    std::uint64_t next_generation_{1U};
};

} // namespace genomes::compute
