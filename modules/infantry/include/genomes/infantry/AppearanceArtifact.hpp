#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/SkeletonData.hpp>
#include <genomes/proc/Seed.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace genomes::infantry {

// These values are the domain vocabulary used by new genome artifacts.  The
// legacy names remain source-compatible for callers compiled against the
// first native prototype, but are deliberately outside the canonical range
// so serialized/native integer IDs cannot accidentally masquerade as the JS
// reference's style numbering.
enum class HairStyle : std::uint8_t {
    Bald = 0,
    Buzz = 1,
    Crew = 2,
    Crop = 3,
    SidePart = 4,
    Fade = 5,
    Messy = 6,
    LegacyShort = 7,
    LegacyLong = 8,
    LegacyBraids = 9,
    LegacyBun = 10,
    LegacyMohawk = 11,
    LegacyCurly = 12,
    Short = LegacyShort,
    Long = LegacyLong,
    Braids = LegacyBraids,
    Bun = LegacyBun,
    Mohawk = LegacyMohawk,
    Curly = LegacyCurly,
};

inline constexpr foundation::Color kDefaultUniformColor{
    0.0998987282F, 0.135633330F, 0.0595112382F, 1.0F};

[[nodiscard]] constexpr HairStyle canonicalHairStyle(HairStyle style) noexcept {
    switch (style) {
    case HairStyle::LegacyShort:
        return HairStyle::Buzz;
    case HairStyle::LegacyLong:
        return HairStyle::SidePart;
    case HairStyle::LegacyBraids:
        return HairStyle::Messy;
    case HairStyle::LegacyBun:
        return HairStyle::Crew;
    case HairStyle::LegacyMohawk:
        return HairStyle::Crop;
    case HairStyle::LegacyCurly:
        return HairStyle::Messy;
    default:
        return style;
    }
}

[[nodiscard]] constexpr std::string_view hairStyleName(HairStyle style) noexcept {
    switch (canonicalHairStyle(style)) {
    case HairStyle::Bald: return "bald";
    case HairStyle::Buzz: return "buzz";
    case HairStyle::Crew: return "crew";
    case HairStyle::Crop: return "crop";
    case HairStyle::SidePart: return "sidePart";
    case HairStyle::Fade: return "fade";
    case HairStyle::Messy: return "messy";
    default: return "unknown";
    }
}

struct AppearanceOptions final {
    std::uint32_t version{1};
    std::uint32_t detail_level{2};
    proc::Seed seed{0};
    HairStyle hair_style{HairStyle::Buzz};
    foundation::Color skin_color{0.72F, 0.50F, 0.38F, 1.0F};
    foundation::Color cloth_color{0.18F, 0.24F, 0.20F, 1.0F};
    // Equipment fit supplies this presentation mask without changing the
    // semantic HairStyle selected by the genome.  0 is uncovered and 1 is
    // full frontal/crown coverage.
    float hair_coverage{0.0F};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] foundation::StableId hash() const noexcept;
};

struct SkinInfluence final {
    std::uint16_t bone_index{kInvalidBoneIndex};
    float weight{0.0F};
};

struct AppearanceVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{};
    std::array<SkinInfluence, 4U> influences{};
    std::uint8_t influence_count{0};
    std::uint16_t material_region{0};
};

struct AppearanceIndexGroup final {
    std::uint32_t start{0};
    std::uint32_t count{0};
    std::uint16_t material{0};
};

struct AppearanceVertexTag final {
    std::string name;
    std::vector<std::uint32_t> vertices;
};

struct AppearanceMaterialDescriptor final {
    std::string name;
    foundation::Color base_color{1.0F,1.0F,1.0F,1.0F};
    float roughness{1.0F};
    float metalness{0.0F};
    float opacity{1.0F};
    bool transparent{false};
};

struct AppearanceMesh final {
    std::vector<AppearanceVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<AppearanceIndexGroup> groups;
    std::vector<AppearanceVertexTag> tags;
    std::vector<AppearanceMaterialDescriptor> materials;
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{};
    foundation::Vec3 sphere_center{};
    float sphere_radius{0.0F};
};

struct FaceEyeMetadata final {
    foundation::Vec3 center{};
    float radius{0.0F};
    std::vector<std::uint32_t> rim;
    std::vector<std::uint32_t> outer;
    float neutral_open{0.0F};
};

struct FaceSurfaceMetadata final {
    std::array<FaceEyeMetadata,2U> eyes{};
    bool neck_connected{false};
    bool mouth_opening{false};
};

struct MorphTarget final {
    std::string_view name{};
    std::vector<foundation::Vec3> position_deltas;
    std::vector<foundation::Vec3> normal_deltas;
};

struct AppearanceArtifact final {
    std::uint32_t version{1};
    foundation::StableId cache_key{0};
    AppearanceMesh body;
    AppearanceMesh hair;
    std::array<MorphTarget, 4U> morphs{};
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{};
    bool has_eye_openings{false};
    bool has_mouth_opening{false};
    FaceSurfaceMetadata face_metadata{};

    [[nodiscard]] bool valid(const SkeletonData&) const noexcept;
};

class AppearanceCompiler final {
public:
    [[nodiscard]] static foundation::StableId cacheKey(const PhenotypeArtifact&,
                                                       const SkeletonData&,
                                                       const AppearanceOptions&) noexcept;
    [[nodiscard]] static foundation::Result<AppearanceArtifact, foundation::Error> build(
        const PhenotypeArtifact&, const SkeletonData&, const AppearanceOptions& = {});
};

class AppearanceCache final {
public:
    using Artifact = std::shared_ptr<const AppearanceArtifact>;

    [[nodiscard]] Artifact find(foundation::StableId key) const;
    [[nodiscard]] bool insert(Artifact artifact);
    void clear();
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t hits() const noexcept;
    [[nodiscard]] std::size_t misses() const noexcept;

    [[nodiscard]] Artifact acquire(const PhenotypeArtifact&, const SkeletonData&,
                                   const AppearanceOptions& = {});

private:
    mutable std::mutex mutex_;
    std::unordered_map<foundation::StableId, Artifact> entries_;
    mutable std::size_t hits_{0};
    mutable std::size_t misses_{0};
};

} // namespace genomes::infantry
