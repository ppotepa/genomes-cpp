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
#include <string_view>
#include <unordered_map>
#include <vector>

namespace genomes::infantry {

enum class HairStyle : std::uint8_t {
    Bald,
    Short,
    Long,
    Braids,
    Bun,
    Mohawk,
    Curly,
};

struct AppearanceOptions final {
    std::uint32_t version{1};
    std::uint32_t detail_level{2};
    proc::Seed seed{0};
    HairStyle hair_style{HairStyle::Short};
    foundation::Color skin_color{0.72F, 0.50F, 0.38F, 1.0F};
    foundation::Color cloth_color{0.18F, 0.24F, 0.20F, 1.0F};

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

struct AppearanceMesh final {
    std::vector<AppearanceVertex> vertices;
    std::vector<std::uint32_t> indices;
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{};
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
