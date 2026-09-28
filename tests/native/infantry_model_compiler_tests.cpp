#include <genomes/infantry/InfantryModelCompiler.hpp>

#include <cassert>
#include <array>
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

    InfantryModelRequest invalid = valid;
    invalid.variation = 4.0F;
    const auto failed = compiler.compile(invalid);
    assert(!failed);
    assert(compiler.lastSuccessful().has_value());
    assert(compiler.lastSuccessful()->cache_key == previous_key);
    assert(compiler.lastError().has_value());

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
    return 0;
}
