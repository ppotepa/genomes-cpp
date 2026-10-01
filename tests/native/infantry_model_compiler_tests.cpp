#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <cassert>
#include <array>
#include <limits>
#include <thread>

int main() {
    using namespace genomes::infantry;
    InfantryModelCompiler compiler;
    InfantryModelRequest valid{};
    valid.seed = 0x5EED2026U;
    valid.uniform_color = kDefaultUniformColor;
    const auto first = compiler.compile(valid);
    assert(first);
    const auto previous_key = first.value().cache_key;

    const auto find_tag = [&](std::string_view name)
        -> const AppearanceVertexTag* {
        for (const auto& tag : first.value().appearance.body.tags) {
            if (tag.name == name) return &tag;
        }
        return nullptr;
    };
    const auto* upper_left = find_tag("lidUpper.L");
    const auto* lower_left = find_tag("lidLower.L");
    const auto* upper_right = find_tag("lidUpper.R");
    const auto* lower_right = find_tag("lidLower.R");
    assert(upper_left && lower_left && upper_right && lower_right);
    assert(first.value().appearance.morphs[0].name == "eyelidsClose");
    const auto closes_in_direction = [&](const AppearanceVertexTag& tag, bool downward) {
        bool found = false;
        for (const auto index : tag.vertices) {
            assert(index < first.value().appearance.morphs[0].position_deltas.size());
            const float dy = first.value().appearance.morphs[0].position_deltas[index].y;
            found = found || (downward ? dy < -1.0e-8F : dy > 1.0e-8F);
        }
        return found;
    };
    assert(closes_in_direction(*upper_left, true));
    assert(closes_in_direction(*upper_right, true));
    assert(closes_in_direction(*lower_left, false));
    assert(closes_in_direction(*lower_right, false));

    InfantryModelRequest invalid = valid;
    invalid.variation = 4.0;
    const auto failed = compiler.compile(invalid);
    assert(!failed);
    assert(compiler.lastError().has_value());

    InfantryModelRequest invalid_detail = valid;
    invalid_detail.detail_level = static_cast<InfantryDetail>(0U);
    assert(!compiler.compile(invalid_detail));

    InfantryModelRequest invalid_nan = valid;
    invalid_nan.variation = std::numeric_limits<double>::quiet_NaN();
    assert(!compiler.compile(invalid_nan));

    InfantryModelRequest invalid_wear = valid;
    invalid_wear.wear = 1.01;
    assert(!compiler.compile(invalid_wear));

    InfantryModelRequest invalid_side = valid;
    invalid_side.side = static_cast<InfantrySide>(255U);
    assert(!compiler.compile(invalid_side));

    InfantryModelRequest precise_variation = valid;
    precise_variation.variation = 1.0000000001;
    const auto precise = compiler.compile(precise_variation);
    assert(precise);
    assert(precise.value().cache_key != previous_key);

    EquipmentOverrideSet overrides{};
    overrides.slots[equipmentSlotIndex(EquipmentSlot::Head)] =
        EquipmentOverride::nullValue();
    InfantryModelRequest changed = valid;
    changed.equipment_overrides = overrides;
    const auto second = compiler.compile(changed);
    assert(second);
    assert(second.value().cache_key != previous_key);
    assert(compiler.lastError() == std::nullopt);

    InfantryModelRequest phenotype_changed = valid;
    phenotype_changed.genome_overrides.height = 1.90F;
    const auto third = compiler.compile(phenotype_changed);
    assert(third);
    assert(third.value().cache_key != previous_key);
    assert(third.value().phenotype.body.height == 1.90F);

    InfantryModelRequest palette_changed = valid;
    palette_changed.palette.uniform = {0.18F, 0.31F, 0.52F, 1.0F};
    const auto palette_model = compiler.compile(palette_changed);
    assert(palette_model);
    assert(palette_model.value().cache_key != previous_key);
    assert(palette_model.value().appearance.body.materials.front().base_color.r == 0.18F);

    std::array<genomes::foundation::StableId, 4U> parallel_keys{};
    std::array<std::thread, 4U> workers;
    for (std::size_t index = 0U; index < workers.size(); ++index) {
        workers[index] = std::thread([&compiler, &valid, &parallel_keys, index] {
            const auto result = compiler.compile(valid);
            assert(result);
            parallel_keys[index] = result.value().cache_key;
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    for (const auto key : parallel_keys) {
        assert(key == previous_key);
    }
    assert(compiler.cacheHits() >= workers.size());
    assert(compiler.cacheMisses() >= 4U);

    const auto revision = compiler.beginRevision();
    compiler.cancelRevision(revision);
    const auto cancelled = compiler.compile(valid, revision);
    assert(!cancelled);
    assert(cancelled.error().code == genomes::foundation::ErrorCode::InvalidState);

    const auto current_revision = compiler.beginRevision();
    const auto revision_result = compiler.compile(valid, current_revision);
    assert(revision_result);
    return 0;
}
