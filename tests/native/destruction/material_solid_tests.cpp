#include <genomes/destruction/MaterialAssembly.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <vector>

int main() {
    using namespace genomes::destruction;

    const MaterialCatalog catalog = MaterialCatalog::makeDefault();
    assert(catalog.frozen());
    assert(catalog.size() >= 9);
    assert(catalog.find(MaterialId::fromName("brick")) != nullptr);

    const auto external = MaterialCatalog::load(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "reference/fixtures/destruction/material_catalog_v1.json");
    assert(external);
    const MaterialCatalog& loaded = external.value();
    assert(loaded.frozen());
    assert(loaded.size() == catalog.size());
    assert(loaded.contentSnapshot() != nullptr);
    assert(loaded.contentSnapshot()->package_id == "destruction.materials");
    assert(loaded.fingerprint().value != 0U);
    for (const auto& expected : catalog.definitions()) {
        const auto* actual = loaded.find(expected.id);
        assert(actual != nullptr);
        assert(actual->name == expected.name);
        assert(actual->density_kg_m3 == expected.density_kg_m3);
        assert(actual->strength_pa == expected.strength_pa);
        assert(actual->penetration_work_j_m3 == expected.penetration_work_j_m3);
        assert(actual->toughness_j_m2 == expected.toughness_j_m2);
        assert(actual->ricochet_factor == expected.ricochet_factor);
        assert(actual->spall_threshold_j == expected.spall_threshold_j);
        assert(actual->response == expected.response);
        assert(actual->provenance == expected.provenance);
    }

    const MaterialFrame frame{{1.0F, 2.0F, 3.0F},
                              {0.0F, 1.0F, 0.0F},
                              {1.0F, 0.0F, 0.0F},
                              {0.0F, 0.0F, 1.0F}};
    assert(frame.valid());
    const auto local = frame.worldToMaterial({2.0F, 2.5F, 5.0F});
    assert(std::abs(local.x - 1.0F) < 1.0e-5F);
    assert(std::abs(local.y - 0.5F) < 1.0e-5F);
    assert(std::abs(local.z - 2.0F) < 1.0e-5F);

    const PhysicalSolidId wall = PhysicalSolidId::fromName("wall");
    const PhysicalSolidId plate = PhysicalSolidId::fromName("plate");
    const std::vector<Layer> layers = {
        {MaterialId::fromName("concrete"), wall, 0.0F, 0.30F, LayerKind::Solid},
        {MaterialId::fromName("brick"), wall, 0.30F, 0.60F, LayerKind::Solid},
        {{}, {}, 0.60F, 0.80F, LayerKind::Void},
        {MaterialId::fromName("steel"), plate, 0.80F, 1.00F, LayerKind::Solid},
    };
    const auto created = MaterialAssembly::create(0x1234U, frame, layers, catalog);
    assert(created);
    const MaterialAssembly& assembly = created.value();
    assert(assembly.layerIndexAt(0.45F).value() == 1U);
    const auto void_index = assembly.layerIndexAt(0.70F);
    assert(void_index.has_value());
    assert(assembly.layers()[*void_index].isVoid());

    const auto forward = assembly.traverse(0.0F, 1.0F);
    assert(forward.size() == 4U);
    assert(!forward.front().reversed);
    const auto reverse = assembly.traverse(1.0F, 0.0F);
    assert(reverse.size() == 4U);
    assert(reverse.front().reversed);
    assert(reverse.front().layer_index == 3U);

    const auto groups = assembly.interfaceGroups();
    assert(groups.size() == 2U);
    assert(groups.front().physical_solid == wall);
    assert(groups.front().layer_indices.size() == 2U);
    assert(groups.back().physical_solid == plate);

    const auto sample_a = assembly.sample(frame.materialToWorld({0.0F, 0.45F, 0.0F}));
    const auto sample_b = assembly.sample(frame.materialToWorld({0.0F, 0.45F, 0.0F}));
    assert(!sample_a.void_layer);
    assert(sample_a.material == MaterialId::fromName("brick"));
    assert(sample_a.cell_seed == sample_b.cell_seed);
    assert(sample_a.density_scale == sample_b.density_scale);
    assert(sample_a.strength_scale == sample_b.strength_scale);

    const auto invalid = MaterialAssembly::create(
        0x1234U, frame,
        std::vector<Layer>{{MaterialId::fromName("unknown"), wall, 0.0F, 1.0F,
                            LayerKind::Solid}},
        catalog);
    assert(!invalid);
    return 0;
}
