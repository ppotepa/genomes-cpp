#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/AppearanceMorphBuilder.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>
#include <genomes/infantry/BodySurfaceGenerator.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/FaceSurfaceGenerator.hpp>
#include <genomes/infantry/HairGenerator.hpp>

#include <genomes/geometry/GeometryConstants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace genomes::infantry {

namespace {

using foundation::Vec3;

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool finite(foundation::Color value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           std::isfinite(value.a);
}

using MeshBuilder = AppearanceMeshBuilder;

[[nodiscard]] std::size_t detailSegments(const AppearanceOptions& options) noexcept {
    return options.detail_level >= 3U ? 24U : options.detail_level == 1U ? 10U : 16U;
}

} // namespace

bool AppearanceOptions::valid() const noexcept {
    return version != 0U && detail_level >= 1U && detail_level <= 3U && finite(skin_color) &&
           finite(cloth_color) && std::isfinite(hair_coverage) && hair_coverage >= 0.0F &&
           hair_coverage <= 1.0F && skin_color.a > 0.0F && cloth_color.a > 0.0F;
}

foundation::StableId AppearanceOptions::hash() const noexcept {
    foundation::StableId result = foundation::stable_id("infantry.appearance.options.v1");
    result = foundation::stableHashCombine(result, version);
    result = foundation::stableHashCombine(result, detail_level);
    result = foundation::stableHashCombine(result, seed);
    result = foundation::stableHashCombine(
        result, static_cast<std::uint64_t>(canonicalHairStyle(hair_style)));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.r));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.g));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.b));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.r));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.g));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.b));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.a));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.a));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(hair_coverage));
    return result;
}

bool AppearanceArtifact::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || cache_key == 0 || !has_eye_openings || !has_mouth_opening ||
        body.vertices.empty() || body.indices.empty() || !finite(minimum) || !finite(maximum)) {
        return false;
    }
    const auto validMesh = [&skeleton](const AppearanceMesh& mesh) {
        for (const AppearanceVertex& vertex : mesh.vertices) {
            if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.color) ||
                vertex.influence_count == 0U || vertex.influence_count > 4U) {
                return false;
            }
            const float normal_length2 = dot(vertex.normal, vertex.normal);
            if (!std::isfinite(normal_length2) ||
                normal_length2 <= geometry::kNormalLengthEpsilon *
                                     geometry::kNormalLengthEpsilon) {
                return false;
            }
            float sum = 0.0F;
            for (std::size_t index = 0U; index < vertex.influence_count; ++index) {
                const SkinInfluence influence = vertex.influences[index];
                if (influence.bone_index >= skeleton.bones().size() ||
                    !std::isfinite(influence.weight) || influence.weight <= 0.0F) {
                    return false;
                }
                sum += influence.weight;
            }
            if (std::abs(sum - 1.0F) > geometry::kWeightEpsilon) {
                return false;
            }
        }
        if (mesh.indices.size() % 3U != 0U) {
            return false;
        }
        for (const std::uint32_t index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                return false;
            }
        }
        for (std::size_t offset = 0U; offset < mesh.indices.size(); offset += 3U) {
            const std::uint32_t i0 = mesh.indices[offset];
            const std::uint32_t i1 = mesh.indices[offset + 1U];
            const std::uint32_t i2 = mesh.indices[offset + 2U];
            const Vec3 face_normal = cross(
                subtract(mesh.vertices[i1].position, mesh.vertices[i0].position),
                subtract(mesh.vertices[i2].position, mesh.vertices[i0].position));
            if (dot(face_normal, face_normal) <=
                geometry::kTriangleAreaEpsilon * geometry::kTriangleAreaEpsilon) {
                return false;
            }
            const Vec3 average_normal{
                (mesh.vertices[i0].normal.x + mesh.vertices[i1].normal.x +
                 mesh.vertices[i2].normal.x) / 3.0F,
                (mesh.vertices[i0].normal.y + mesh.vertices[i1].normal.y +
                 mesh.vertices[i2].normal.y) / 3.0F,
                (mesh.vertices[i0].normal.z + mesh.vertices[i1].normal.z +
                 mesh.vertices[i2].normal.z) / 3.0F};
            if (dot(face_normal, average_normal) < -1.0e-6F) {
                return false;
            }
        }
        return true;
    };
    if (!validMesh(body) || (!hair.vertices.empty() && !validMesh(hair))) {
        return false;
    }
    for (const MorphTarget& morph : morphs) {
        if (morph.name.empty() || morph.position_deltas.size() != body.vertices.size() ||
            morph.normal_deltas.size() != body.vertices.size()) {
            return false;
        }
        for (std::size_t index = 0U; index < body.vertices.size(); ++index) {
            if (!finite(morph.position_deltas[index]) || !finite(morph.normal_deltas[index])) {
                return false;
            }
        }
    }
    return minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z;
}

foundation::StableId AppearanceCompiler::cacheKey(const PhenotypeArtifact& phenotype,
                                                  const SkeletonData& skeleton,
                                                  const AppearanceOptions& options) noexcept {
    foundation::StableId result = foundation::stable_id("infantry.appearance.v1");
    result = foundation::stableHashCombine(result, phenotype.cache_key);
    result = foundation::stableHashCombine(result, skeleton.cacheKey());
    result = foundation::stableHashCombine(result, options.hash());
    return result;
}

foundation::Result<AppearanceArtifact, foundation::Error> AppearanceCompiler::build(
    const PhenotypeArtifact& phenotype,
    const SkeletonData& skeleton,
    const AppearanceOptions& options) {
    if (!phenotype.valid() || !skeleton.valid() || !options.valid()) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry appearance input"});
    }
    const auto anatomy_result = FaceAnatomyEvaluator::resolve(phenotype);
    if (!anatomy_result) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            anatomy_result.error());
    }
    const ResolvedAnatomy& anatomy = anatomy_result.value();

    const BodyPhenotype& body = phenotype.body;
    const FacePhenotype& face = phenotype.face;
    const std::size_t segments = detailSegments(options);
    const foundation::Color cloth = options.cloth_color;
    const foundation::Color skin = options.skin_color;
    MeshBuilder builder{};
    geometry::Ring neck_ring;
    if (!BodySurfaceGenerator::buildTorso(builder, body, skeleton, segments, cloth,
                                          neck_ring)) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid torso anatomy"});
    }

    if (!BodySurfaceGenerator::buildLimbs(builder, body, skeleton, segments, cloth, skin)) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid articulated body anatomy"});
    }

    const auto face_build = FaceSurfaceGenerator::build(
        builder, anatomy, face, skeleton, options, neck_ring);
    if (!face_build) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            face_build.error());
    }
    AppearanceArtifact result{};
    result.version = options.version;
    result.cache_key = cacheKey(phenotype, skeleton, options);
    result.body = std::move(builder).finalize();
    result.has_eye_openings = true;
    result.has_mouth_opening = true;

    const HairStyle hair_style = canonicalHairStyle(options.hair_style);

    // Hair owns its scalp sampling and semantic style families. Keep the
    // compiler responsible only for orchestration and artifact assembly.
    if (hair_style != HairStyle::Bald) {
        const auto hair = HairGenerator::build(anatomy, face, skeleton, options);
        if (!hair) {
            return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                hair.error());
        }
        result.hair = hair.value();
    }

    AppearanceMorphBuilder::initialize(result);
    AppearanceMorphBuilder::build(result, face);
    result.minimum = result.body.minimum;
    result.maximum = result.body.maximum;
    if (!result.hair.vertices.empty()) {
        result.minimum.x = std::min(result.minimum.x, result.hair.minimum.x);
        result.minimum.y = std::min(result.minimum.y, result.hair.minimum.y);
        result.minimum.z = std::min(result.minimum.z, result.hair.minimum.z);
        result.maximum.x = std::max(result.maximum.x, result.hair.maximum.x);
        result.maximum.y = std::max(result.maximum.y, result.hair.maximum.y);
        result.maximum.z = std::max(result.maximum.z, result.hair.maximum.z);
    }
    return result.valid(skeleton)
               ? foundation::Result<AppearanceArtifact, foundation::Error>::success(std::move(result))
               : foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState,
                      "infantry appearance mesh or skin contract is invalid"});
}

AppearanceCache::Artifact AppearanceCache::find(foundation::StableId key) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(key);
    if (iterator == entries_.end()) {
        ++misses_;
        return {};
    }
    ++hits_;
    return iterator->second;
}

bool AppearanceCache::insert(Artifact artifact) {
    if (!artifact || artifact->cache_key == 0) {
        return false;
    }
    std::scoped_lock lock(mutex_);
    return entries_.emplace(artifact->cache_key, std::move(artifact)).second;
}

void AppearanceCache::clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

std::size_t AppearanceCache::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::size_t AppearanceCache::hits() const noexcept {
    std::scoped_lock lock(mutex_);
    return hits_;
}

std::size_t AppearanceCache::misses() const noexcept {
    std::scoped_lock lock(mutex_);
    return misses_;
}

AppearanceCache::Artifact AppearanceCache::acquire(const PhenotypeArtifact& phenotype,
                                                   const SkeletonData& skeleton,
                                                   const AppearanceOptions& options) {
    const foundation::StableId key = AppearanceCompiler::cacheKey(phenotype, skeleton, options);
    if (Artifact cached = find(key)) {
        return cached;
    }
    const auto built = AppearanceCompiler::build(phenotype, skeleton, options);
    if (!built) {
        return {};
    }
    Artifact artifact = std::make_shared<AppearanceArtifact>(built.value());
    if (insert(artifact)) {
        return artifact;
    }
    return find(key);
}

} // namespace genomes::infantry








