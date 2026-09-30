#include <genomes/geometry/SolidOps.hpp>
#include <cassert>

int main() {
    using namespace genomes::geometry;
    MeshData empty;
    const auto result=booleanSolid(empty,empty,SolidBoolean::Union);
    assert(!result);
    return 0;
}
