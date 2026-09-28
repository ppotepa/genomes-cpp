#include <genomes/weapons/WeaponCatalog.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace genomes::weapons {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

} // namespace

foundation::StableId WeaponGeometryGenerator::cacheKey(const WeaponDefinition& definition,
                                                        const WeaponVariant& variant) noexcept {
    foundation::StableId key = foundation::stableHashCombine(
        foundation::stable_id("weapon.artifact.v1"), definition.id);
    key = foundation::stableHashCombine(key, foundation::stableHashU64(variant.seed));
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(variant.scale));
    key = foundation::stableHashCombine(key, foundation::stableHashFloat(variant.wear));
    key = foundation::stableHashCombine(key, variant.material_variant);
    return key;
}

foundation::Result<WeaponArtifact, foundation::Error> WeaponGeometryGenerator::build(
    const WeaponDefinition& definition, const WeaponVariant& variant) {
    if (!definition.valid() || !variant.valid()) {
        return foundation::Result<WeaponArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon geometry input"});
    }
    const float wear_scale = 1.0F - variant.wear * 0.012F;
    const foundation::Vec3 half = multiply(definition.dimensions, 0.5F * variant.scale * wear_scale);
    const foundation::Vec3 minimum{-half.x, -half.y, -half.z};
    const foundation::Vec3 maximum{half.x, half.y, half.z};
    constexpr std::array<foundation::Vec3, 8> corners{{
        {-1.0F, -1.0F, -1.0F}, {1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, -1.0F}, {-1.0F, 1.0F, -1.0F},
        {-1.0F, -1.0F, 1.0F},  {1.0F, -1.0F, 1.0F},  {1.0F, 1.0F, 1.0F},  {-1.0F, 1.0F, 1.0F},
    }};
    WeaponArtifact artifact{};
    artifact.weapon_id = definition.id;
    artifact.cache_key = cacheKey(definition, variant);
    artifact.mesh.minimum = minimum;
    artifact.mesh.maximum = maximum;
    artifact.mesh.vertices.reserve(corners.size());
    for (const foundation::Vec3 corner : corners) {
        artifact.mesh.vertices.push_back({{corner.x * half.x, corner.y * half.y, corner.z * half.z},
                                          {corner.x, corner.y, corner.z}, {},
                                          variant.material_variant % 4U});
    }
    constexpr std::array<std::uint32_t, 36> indices{{
        0U, 1U, 2U, 0U, 2U, 3U, 4U, 6U, 5U, 4U, 7U, 6U,
        0U, 4U, 5U, 0U, 5U, 1U, 3U, 2U, 6U, 3U, 6U, 7U,
        0U, 3U, 7U, 0U, 7U, 4U, 1U, 5U, 6U, 1U, 6U, 2U,
    }};
    artifact.mesh.indices.assign(indices.begin(), indices.end());
    artifact.muzzle = {definition.muzzle, {1.0F, 0.0F, 0.0F}};
    artifact.primary_grip = {definition.primary_grip, {1.0F, 0.0F, 0.0F}};
    artifact.support_grip = {definition.support_grip, {1.0F, 0.0F, 0.0F}};
    artifact.stow_anchor = {definition.stow_anchor, {1.0F, 0.0F, 0.0F}};
    return artifact.valid(definition)
               ? foundation::Result<WeaponArtifact, foundation::Error>::success(std::move(artifact))
               : foundation::Result<WeaponArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "weapon artifact validation failed"});
}

bool WeaponArtifact::valid(const WeaponDefinition& definition) const noexcept {
    if (version != 1U || weapon_id != definition.id || cache_key == 0U || mesh.vertices.empty() ||
        mesh.indices.empty() || !finite(mesh.minimum) || !finite(mesh.maximum)) {
        return false;
    }
    for (const WeaponVertex& vertex : mesh.vertices) {
        if (!finite(vertex.position) || !finite(vertex.normal)) {
            return false;
        }
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    return finite(muzzle.local_position) && finite(primary_grip.local_position) &&
           finite(support_grip.local_position) && finite(stow_anchor.local_position);
}

const WeaponArtifact* WeaponArtifactCache::find(foundation::StableId key) const noexcept {
    for (const WeaponArtifact& artifact : artifacts_) {
        if (artifact.cache_key == key) {
            return &artifact;
        }
    }
    return nullptr;
}

void WeaponArtifactCache::store(WeaponArtifact artifact) {
    for (WeaponArtifact& existing : artifacts_) {
        if (existing.cache_key == artifact.cache_key) {
            existing = std::move(artifact);
            return;
        }
    }
    artifacts_.push_back(std::move(artifact));
}

const WeaponArtifact* WeaponArtifactCache::acquire(const WeaponDefinition& definition,
                                                    const WeaponVariant& variant) {
    const foundation::StableId key = WeaponGeometryGenerator::cacheKey(definition, variant);
    if (const WeaponArtifact* existing = find(key); existing != nullptr) {
        return existing;
    }
    const auto built = WeaponGeometryGenerator::build(definition, variant);
    if (!built) {
        return nullptr;
    }
    store(built.value());
    return find(key);
}

} // namespace genomes::weapons
