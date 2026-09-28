#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>

#include <algorithm>

namespace genomes::infantry {

foundation::Result<InfantryModelArtifact, foundation::Error>
InfantryModelCompiler::compile(const InfantryModelRequest& request) {
    foundation::StableId request_key = foundation::stableHashCombine(
        foundation::stable_id("infantry.model.request"), InfantryGeneratorVersion);
    request_key = foundation::stableHashCombine(request_key, request.seed);
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.variation));
    request_key = foundation::stableHashCombine(request_key, request.detail_level);
    request_key = foundation::stableHashCombine(request_key, request.loadout_id);
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.r));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.g));
    request_key = foundation::stableHashCombine(request_key,
                                                foundation::stableHashFloat(request.uniform_color.b));
    {
        std::scoped_lock lock(mutex_);
        if (const auto found = cache_.find(request_key); found != cache_.end()) {
            ++cache_hits_;
            last_successful_ = *found->second;
            return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
                *found->second);
        }
        ++cache_misses_;
    }
    const auto failure = [this](foundation::Error error)
        -> foundation::Result<InfantryModelArtifact, foundation::Error> {
        std::scoped_lock lock(mutex_);
        if (last_successful_) {
            return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
                *last_successful_);
        }
        return foundation::Result<InfantryModelArtifact, foundation::Error>::failure(
            std::move(error));
    };
    if (request.variation < 0.0F || request.variation > 2.0F ||
        request.detail_level > 3U) {
        return failure({foundation::ErrorCode::InvalidArgument,
                        "invalid infantry model request"});
    }

    const foundation::StableId loadout = request.loadout_id != 0
        ? request.loadout_id
        : EquipmentCatalog::loadoutId("RIFLEMAN");

    auto genome = InfantryGenome::generate(request.seed, request.variation);
    if (!genome) {
        return failure(genome.error());
    }
    auto phenotype = PhenotypeResolver::resolve(genome.value());
    if (!phenotype) {
        return failure(phenotype.error());
    }
    auto skeleton = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    if (!skeleton) {
        return failure(skeleton.error());
    }
    AppearanceOptions appearance_options{};
    appearance_options.seed = request.seed;
    appearance_options.detail_level = request.detail_level;
    appearance_options.skin_color = phenotype.value().body.skin_color;
    appearance_options.cloth_color = request.uniform_color;
    appearance_options.hair_style = static_cast<HairStyle>(phenotype.value().face.hair_style);
    auto appearance = AppearanceCompiler::build(phenotype.value(), skeleton.value(),
                                                 appearance_options);
    if (!appearance) {
        return failure(appearance.error());
    }
    auto equipment = EquipmentResolver::resolve(request.seed, loadout,
                                                request.equipment_overrides);
    if (!equipment) {
        return failure(equipment.error());
    }
    auto fit = EquipmentFitter::build(equipment.value(), phenotype.value().body,
                                      skeleton.value());
    if (!fit) {
        return failure(fit.error());
    }
    auto gear = GearGenerator::build(equipment.value(), fit.value(), skeleton.value(),
                                     request.uniform_color);
    if (!gear) {
        return failure(gear.error());
    }

    InfantryModelArtifact result{};
    result.genome = genome.value();
    result.phenotype = phenotype.value();
    result.skeleton = skeleton.value();
    result.appearance = appearance.value();
    result.equipment = equipment.value();
    result.equipment_fit = fit.value();
    result.gear = gear.value();
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("genomes.infantry.model"),
                                      InfantryArtifactVersion), request.seed);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.phenotype.cache_key);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.appearance.cache_key);
    result.cache_key = foundation::stableHashCombine(result.cache_key,
                                                     result.gear.cache_key);

    {
        std::scoped_lock lock(mutex_);
        last_successful_ = result;
        cache_[request_key] = std::make_shared<const InfantryModelArtifact>(result);
    }
    return foundation::Result<InfantryModelArtifact, foundation::Error>::success(
        std::move(result));
}

std::optional<InfantryModelArtifact>
InfantryModelCompiler::lastSuccessful() const {
    std::scoped_lock lock(mutex_);
    return last_successful_;
}

} // namespace genomes::infantry
