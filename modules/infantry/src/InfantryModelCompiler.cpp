#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/ReferenceBodySurfaceGenerator.hpp>

#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool sameColor(foundation::Color left, foundation::Color right) noexcept {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

[[nodiscard]] InfantryModelRequest canonicalRequest(InfantryModelRequest request) noexcept {
    // `uniform_color` was the legacy material input.  Normalize it into the
    // palette before hashing so equivalent legacy and palette requests share
    // one immutable artifact.
    if (sameColor(request.palette.uniform, kDefaultUniformColor)) {
        request.palette.uniform = request.uniform_color;
    }
    request.uniform_color = kDefaultUniformColor;
    return request;
}

} // namespace

foundation::StableId InfantryModelCompiler::canonicalRequestKey(
    const InfantryModelRequest& request) noexcept {
    const InfantryModelRequest canonical = canonicalRequest(request);
    foundation::StableId request_key = foundation::stableHashCombine(
        foundation::stable_id("infantry.model.request"), InfantryGeneratorVersion);
    request_key = foundation::stableHashCombine(request_key, canonical.seed);
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashDouble(canonical.variation));
    request_key = foundation::stableHashCombine(request_key,
        static_cast<std::uint32_t>(canonical.detail_level));
    request_key = foundation::stableHashCombine(request_key, canonical.genome_overrides.hash());
    request_key = foundation::stableHashCombine(request_key, canonical.loadout_id);
    request_key = foundation::stableHashCombine(request_key,
                                                static_cast<std::uint64_t>(canonical.side));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashDouble(canonical.wear));
    for (const EquipmentOverride& override : canonical.equipment_overrides.slots) {
        request_key = foundation::stableHashCombine(request_key, override.specified ? 1U : 0U);
        request_key = foundation::stableHashCombine(request_key, override.empty ? 1U : 0U);
        request_key = foundation::stableHashCombine(request_key, override.definition_id);
    }
    const foundation::Color colors[]{canonical.palette.uniform, canonical.palette.trousers,
                                      canonical.palette.leather, canonical.palette.metal};
    for (const auto& color : colors) {
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.r));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.g));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.b));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.a));
    }
    return request_key;
}

proc::ArtifactKey InfantryModelCompiler::artifactKey(const InfantryModelRequest& request) noexcept {
    const InfantryModelRequest canonical = canonicalRequest(request);
    return {.namespace_id = foundation::stable_id("genomes.infantry.model"),
            .generator_version = InfantryGeneratorVersion,
            .seed = canonical.seed,
            .input_hash = canonicalRequestKey(canonical),
            .schema_version = InfantryArtifactVersion,
            .dependency_hash = foundation::stable_id("infantry.equipment.catalog.v1")};
}

foundation::Result<InfantryModelCompileResult, foundation::Error>
InfantryModelCompiler::compile(const InfantryModelRequest& request) {
    const InfantryModelRequest canonical = canonicalRequest(request);
    const proc::ArtifactKey artifact_key = artifactKey(canonical);
    {
        std::scoped_lock lock(mutex_);
        if (const auto found = cache_.find(artifact_key); found != cache_.end()) {
            ++cache_hits_;
            return foundation::Result<InfantryModelCompileResult, foundation::Error>::success(
                {found->second, artifact_key});
        }
        ++cache_misses_;
    }
    const auto failure = [](foundation::Error error)
        -> foundation::Result<InfantryModelCompileResult, foundation::Error> {
        return foundation::Result<InfantryModelCompileResult, foundation::Error>::failure(
            std::move(error));
    };
    const auto detail_level = static_cast<std::uint32_t>(canonical.detail_level);
    const foundation::Color effective_uniform = canonical.palette.uniform;
    const auto valid_color = [](foundation::Color color) noexcept {
        return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
               std::isfinite(color.a) && color.r >= 0.0F && color.r <= 1.0F &&
               color.g >= 0.0F && color.g <= 1.0F && color.b >= 0.0F &&
               color.b <= 1.0F && color.a >= 0.0F && color.a <= 1.0F;
    };
    if (!isValidVariation(canonical.variation) || detail_level < 1U || detail_level > 3U ||
        !std::isfinite(canonical.wear) || canonical.wear < 0.0 || canonical.wear > 1.0 ||
        static_cast<std::uint8_t>(canonical.side) > static_cast<std::uint8_t>(InfantrySide::Neutral) ||
        !valid_color(canonical.palette.uniform) || !valid_color(canonical.palette.trousers) ||
        !valid_color(canonical.palette.leather) || !valid_color(canonical.palette.metal)) {
        return failure({foundation::ErrorCode::InvalidArgument,
                        "invalid infantry model request"});
    }

    const foundation::StableId loadout = canonical.loadout_id != 0
        ? canonical.loadout_id
        : EquipmentCatalog::loadoutId("RIFLEMAN");

    auto genome = InfantryGenome::generate(canonical.seed, static_cast<float>(canonical.variation));
    if (!genome) {
        return failure(genome.error());
    }
    auto phenotype = PhenotypeResolver::resolve(genome.value(), canonical.genome_overrides);
    if (!phenotype) {
        return failure(phenotype.error());
    }
    auto skeleton = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    if (!skeleton) {
        return failure(skeleton.error());
    }
    auto equipment = EquipmentResolver::resolve(canonical.seed, loadout,
                                                canonical.equipment_overrides);
    if (!equipment) {
        return failure(equipment.error());
    }
    auto fit = EquipmentFitter::build(equipment.value(), phenotype.value(),
                                      skeleton.value());
    if (!fit) {
        return failure(fit.error());
    }

    AppearanceOptions appearance_options{};
    appearance_options.seed = canonical.seed;
    appearance_options.detail_level = detail_level;
    appearance_options.skin_color = phenotype.value().body.skin_color;
    appearance_options.cloth_color = effective_uniform;
    appearance_options.hair_style = canonicalHairStyle(
        static_cast<HairStyle>(phenotype.value().face.hair_style));
    if (const EquipmentItem* head_item = equipment.value().item(EquipmentSlot::Head)) {
        if (const auto* definition = EquipmentCatalog::findItem(head_item->definition_id)) {
            appearance_options.hair_coverage = definition->kind == EquipmentKind::Helmet
                ? 0.92F
                : definition->kind == EquipmentKind::Cap ? 0.34F : 0.0F;
        }
    }
    auto reference_surface = ReferenceBodySurfaceGenerator::build(
        fit.value(), skeleton.value(), effective_uniform, detail_level);
    if (!reference_surface) return failure(reference_surface.error());
    AppearanceArtifact appearance{};
    appearance.cache_key = AppearanceCompiler::cacheKey(
        phenotype.value(), skeleton.value(), appearance_options);
    appearance.body = std::move(reference_surface.value().mesh);
    appearance.morphs = std::move(reference_surface.value().morphs);
    appearance.minimum = appearance.body.minimum;
    appearance.maximum = appearance.body.maximum;
    appearance.has_eye_openings = true;
    appearance.has_mouth_opening = true;
    appearance.face_metadata.neck_connected = true;
    appearance.face_metadata.mouth_opening = true;
    auto gear = GearGenerator::build(equipment.value(), fit.value(), skeleton.value(),
                                     effective_uniform, detail_level,
                                     static_cast<float>(canonical.wear));
    if (!gear) {
        return failure(gear.error());
    }

    auto result = std::make_shared<InfantryModelArtifact>();
    result->genome = genome.value();
    result->phenotype = phenotype.value();
    result->skeleton = skeleton.value();
    result->appearance = std::move(appearance);
    result->equipment = equipment.value();
    result->equipment_fit = fit.value();
    result->gear = gear.value();
    result->cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("genomes.infantry.model"),
                                      InfantryArtifactVersion), canonical.seed);
    result->cache_key = foundation::stableHashCombine(result->cache_key, artifact_key.input_hash);
    result->cache_key = foundation::stableHashCombine(result->cache_key,
                                                      result->phenotype.cache_key);
    result->cache_key = foundation::stableHashCombine(result->cache_key,
                                                      result->appearance.cache_key);
    result->cache_key = foundation::stableHashCombine(result->cache_key,
                                                      result->gear.cache_key);

    {
        std::scoped_lock lock(mutex_);
        if (const auto found = cache_.find(artifact_key); found != cache_.end()) {
            // Another worker may have completed the same expensive request while
            // this candidate was being generated.  Preserve one immutable
            // canonical artifact instead of replacing it with an equivalent
            // duplicate.
            ++cache_hits_;
            return foundation::Result<InfantryModelCompileResult, foundation::Error>::success(
                {found->second, artifact_key});
        }
        cache_[artifact_key] = result;
    }
    return foundation::Result<InfantryModelCompileResult, foundation::Error>::success(
        {std::move(result), artifact_key});
}

} // namespace genomes::infantry
