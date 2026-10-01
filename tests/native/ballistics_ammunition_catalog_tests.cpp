#include <genomes/ballistics/AmmunitionCatalog.hpp>

#include <cassert>
#include <filesystem>
#include <string_view>

int main() {
    using namespace genomes::ballistics;

    const auto path = std::filesystem::path{GENOMES_SOURCE_DIR} / "reference" / "fixtures" /
                      "ballistics" / "ammunition_catalog_profiles.json";
    const auto loaded = loadAmmunitionCatalog(path);
    assert(loaded);
    const AmmunitionCatalog& catalog = loaded.value();
    assert(catalog.frozen());
    assert(catalog.size() == 1U);
    assert(catalog.sourceCommit() == "da885ca68b2ae63154a004574fed00eb9dfeb458");
    assert(catalog.contentSnapshot() != nullptr);
    assert(catalog.fingerprint().value != 0U);

    const auto* ammunition = catalog.find(ammunition_id("battlefield.ammo.556"));
    assert(ammunition != nullptr);
    assert(ammunition->strategy_id_value == strategy_id("battlefield.556.fmj"));
    assert(ammunition->caliber_id_value == caliber_id("battlefield.556"));
    assert(ammunition->variant_id_value == variant_id("battlefield.standard"));
    assert(ammunition->mass_kg == 0.004F);
    assert(ammunition->diameter_m == 0.00556F);
    assert(ammunition->drag_diameter_m == 0.00556F);
    assert(ammunition->muzzle_velocity_mps == 870.0F);
    assert(ammunition->inertia_factor == 0.25F);
    assert(ammunition->provenance == "FAST_VIABILITY_AGENT_PLAN");

    const auto* strategy = catalog.strategy(strategy_id("battlefield.556.fmj"));
    assert(strategy != nullptr);
    assert(strategy->caliber_id == caliber_id("battlefield.556"));
    assert(strategy->variant_id == variant_id("battlefield.standard"));
    assert(strategy->construction == ConstructionKind::FullMetalJacket);
    assert(strategy->drag_coefficient == 0.3F);
    assert(strategy->contact_work_scale == 1.0F);
    assert(strategy->ricochet_threshold == 0.35F);
    assert(!strategy->explosive);
    assert(strategy->fuze == FuzeMode::None);

    const auto second = loadAmmunitionCatalog(path);
    assert(second);
    assert(second.value().fingerprint() == catalog.fingerprint());
    return 0;
}
