#include <genomes/geometry/SolidOps.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <cassert>

int main() {
    using namespace genomes::geometry;
    MeshData empty;
    const auto result=booleanSolid(empty,empty,SolidBoolean::Union);
    assert(!result);
    const auto left=makeBoxResult({{2.0F,2.0F,2.0F}});
    const auto right=makeBoxResult({{1.0F,1.0F,1.0F}});
    assert(left && right);
    const auto union_result=booleanSolid(left.value(),right.value(),SolidBoolean::Union);
    const auto difference_result=booleanSolid(left.value(),right.value(),SolidBoolean::Difference);
    const auto intersection_result=booleanSolid(left.value(),right.value(),SolidBoolean::Intersection);
    assert(union_result && difference_result && intersection_result);
    assert(union_result.value().valid() && difference_result.value().valid() &&
           intersection_result.value().valid());
    return 0;
}
