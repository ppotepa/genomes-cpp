#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/ReferenceBodySurfaceGenerator.hpp>

#include <cmath>

namespace genomes::infantry {

foundation::Result<InfantryModelArtifact, foundation::Error>
InfantryModelCompiler::compile(const InfantryModelRequest& request) {
    return compile(request, 0U);
}

foundation::Result<InfantryModelArtifact, foundation::Error>
InfantryModelCompiler::compile(const InfantryModelRequest& request,
                               CompileRevision revision) {
    const auto cancelled = [this, revision]() noexcept {
        return revision != 0U && revision_.load(std::memory_order_acquire) != revision;
    };
    const auto cancellation = [this](foundation::Error error)
        -> foundation::Result<InfantryModelArtifact, foundation::Error> {
        std::scoped_lock lock(mutex_);
        last_error_ = error;
        return foundation::Result<InfantryModelArtifact, foundation::Error>::failure(
            std::move(error));
    };
    if (cancelled()) {
        return cancellation({foundation::ErrorCode::InvalidState,
                              "infantry model compilation cancelled"});
    }
    foundation::StableId request_key = foundation::stableHashCombine(
        foundation::stable_id("infantry.model.request"), InfantryGeneratorVersion);
    request_key = foundation::stableHashCombine(request_key, request.seed);
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashDouble(request.variation));
    request_key = foundation::stableHashCombine(
        request_key, static_cast<std::uint32_t>(request.detail_level));
    request_key = foundation::stableHashCombine(request_key, request.genome_overrides.hash());
    request_key = foundation::stableHashCombine(request_key, request.loadout_id);
    request_key = foundation::stableHashCombine(request_key,
                                                static_cast<std::uint64_t>(request.side));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashDouble(request.wear));
    for (const EquipmentOverride& override : request.equipment_overrides.slots) {
        request_key = foundation::stableHashCombine(request_key, override.specified ? 1U : 0U);
        request_key = foundation::stableHashCombine(request_key, override.empty ? 1U : 0U);
        request_key = foundation::stableHashCombine(request_key, override.definition_id);
    }
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.r));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.g));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.b));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.a));
    const foundation::Color palette_colors[]{request.palette.uniform, request.palette.trousers,
                                              request.palette.leather, request.palette.metal};
    for (const auto& color : palette_colors) {
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.r));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.g));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.b));
        request_key = foundation::stableHashCombine(request_key, foundation::stableHashFloat(color.a));
    }
    {
        std::scoped_lock lock(mutex_);
        if (const auto found = cache_.find(request_key); found != cache_.end()) {
            ++cache_hits_;
            last_error_.reset();
            return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
                *found->second);
        }
        ++cache_misses_;
    }
    const auto failure = [this](foundation::Error error)
        -> foundation::Result<InfantryModelArtifact, foundation::Error> {
        std::scoped_lock lock(mutex_);
        last_error_ = error;
        return foundation::Result<InfantryModelArtifact, foundation::Error>::failure(
            std::move(error));
    };
    const auto detail_level = static_cast<std::uint32_t>(request.detail_level);
    const auto same_color = [](foundation::Color left, foundation::Color right) noexcept {
        return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
    };
    // `uniform_color` is retained for source compatibility.  A non-default
    // palette uniform takes precedence; otherwise the legacy field remains a
    // functional override for existing callers.
    const foundation::Color effective_uniform =
        same_color(request.palette.uniform, kDefaultUniformColor)
            ? request.uniform_color
            : request.palette.uniform;
    const auto valid_color = [](foundation::Color color) noexcept {
        return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
               std::isfinite(color.a) && color.r >= 0.0F && color.r <= 1.0F &&
               color.g >= 0.0F && color.g <= 1.0F && color.b >= 0.0F &&
               color.b <= 1.0F && color.a >= 0.0F && color.a <= 1.0F;
    };
    if (!isValidVariation(request.variation) || detail_level < 1U || detail_level > 3U ||
        !std::isfinite(request.wear) || request.wear < 0.0 || request.wear > 1.0 ||
        static_cast<std::uint8_t>(request.side) > static_cast<std::uint8_t>(InfantrySide::Neutral) ||
        !valid_color(request.uniform_color) || !valid_color(request.palette.uniform) ||
        !valid_color(request.palette.trousers) || !valid_color(request.palette.leather) ||
        !valid_color(request.palette.metal)) {
        return failure({foundation::ErrorCode::InvalidArgument,
                        "invalid infantry model request"});
    }

    const foundation::StableId loadout = request.loadout_id != 0
        ? request.loadout_id
        : EquipmentCatalog::loadoutId("RIFLEMAN");

    auto genome = InfantryGenome::generate(request.seed, static_cast<float>(request.variation));
    if (!genome) {
        return failure(genome.error());
    }
    auto phenotype = PhenotypeResolver::resolve(genome.value(), request.genome_overrides);
    if (!phenotype) {
        return failure(phenotype.error());
    }
    auto skeleton = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    if (!skeleton) {
        return failure(skeleton.error());
    }
    auto equipment = EquipmentResolver::resolve(request.seed, loadout,
                                                request.equipment_overrides);
    if (!equipment) {
        return failure(equipment.error());
    }
    auto fit = EquipmentFitter::build(equipment.value(), phenotype.value(),
                                      skeleton.value());
    if (!fit) {
        return failure(fit.error());
    }

    AppearanceOptions appearance_options{};
    appearance_options.seed = request.seed;
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
                                     static_cast<float>(request.wear));
    if (!gear) {
        return failure(gear.error());
    }

    if (cancelled()) {
        return cancellation({foundation::ErrorCode::InvalidState,
                              "infantry model compilation cancelled"});
    }

    InfantryModelArtifact result{};
    result.genome = genome.value();
    result.phenotype = phenotype.value();
    result.skeleton = skeleton.value();
    result.appearance = std::move(appearance);
    result.equipment = equipment.value();
    result.equipment_fit = fit.value();
    result.gear = gear.value();
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("genomes.infantry.model"),
                                      InfantryArtifactVersion), request.seed);
    result.cache_key = foundation::stableHashCombine(result.cache_key, request_key);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.phenotype.cache_key);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.appearance.cache_key);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.gear.cache_key);

    {
        std::scoped_lock lock(mutex_);
        last_error_.reset();
        if (const auto found = cache_.find(request_key); found != cache_.end()) {
            // Another worker may have completed the same expensive request while
            // this candidate was being generated.  Preserve one immutable
            // canonical artifact instead of replacing it with an equivalent
            // duplicate.
            ++cache_hits_;
            return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
                *found->second);
        }
        cache_[request_key] = std::make_shared<const InfantryModelArtifact>(result);
    }
    return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
        std::move(result));
}

std::optional<foundation::Error> InfantryModelCompiler::lastError() const {
    std::scoped_lock lock(mutex_);
    return last_error_;
}

} // namespace genomes::infantry
