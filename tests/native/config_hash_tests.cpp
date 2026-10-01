#include <genomes/foundation/ConfigHash.hpp>

#include <array>
#include <cassert>

int main() {
    using namespace genomes::foundation;
    constexpr std::array fields_a{
        CanonicalConfigField{"detail", 3U},
        CanonicalConfigField{"seed", 0x5EED2026U},
        CanonicalConfigField{"workers", 4U},
    };
    constexpr std::array fields_b{
        CanonicalConfigField{"workers", 4U},
        CanonicalConfigField{"seed", 0x5EED2026U},
        CanonicalConfigField{"detail", 3U},
    };
    const auto simulation_a = makeSimConfigHash("test.profile", fields_a);
    const auto simulation_b = makeSimConfigHash("test.profile", fields_b);
    assert(simulation_a == simulation_b);
    assert(simulation_a.value != 0U);

    const auto presentation = makePresentationConfigHash("test.profile", fields_a);
    const auto execution = makeExecutionProfileHash("test.profile", fields_a);
    assert(presentation.value != simulation_a.value);
    assert(execution.value != simulation_a.value);
    assert(execution.value != presentation.value);
    return 0;
}
