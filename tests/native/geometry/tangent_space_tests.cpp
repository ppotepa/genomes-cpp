#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/TangentSpace.hpp>
#include <cassert>

int main() {
    const auto box=genomes::geometry::makeBoxResult({{1,1,1}});
    assert(box);
    const auto tangents=genomes::geometry::generateTangents(box.value());
    assert(tangents && tangents.value().tangents.size()==box.value().vertices.size());
    for(const auto tangent:tangents.value().tangents) assert(tangent.w==1.0F || tangent.w==-1.0F);
    return 0;
}
