#include <genomes/infantry/InfantryModule.hpp>

#include <cassert>

int main() {
    using namespace genomes::infantry;
    const InfantryModuleRegistration registration = InfantryModule::registration();
    assert(registration.valid());
    InfantryIdentity identity{};
    identity.entity = {7U, 1U};
    identity.genome_artifact = 11U;
    identity.phenotype_artifact = 12U;
    const auto bound = InfantryModule::bindIdentity(identity);
    assert(bound);
    assert(bound.value().valid());
    return 0;
}
