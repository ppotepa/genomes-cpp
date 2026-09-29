#include <genomes/geometry/threepp/PrimitiveProvider.hpp>

#include <cassert>

int main() {
    using namespace genomes::geometry::threepp_provider;
    const auto b=box({2.0F,3.0F,4.0F});
    assert(b && b.value().valid());
    assert(b.value().vertices.size() == 24U);
    assert(b.value().indices.size() == 36U);

    const auto s=sphere(1.0F,16U,12U);
    assert(s && s.value().valid());
    const auto c=cylinder(0.5F,0.5F,2.0F,16U);
    assert(c && c.value().valid());
    const auto cap=capsule(0.5F,1.0F,8U,16U);
    assert(cap && cap.value().valid());

    assert(!box({0.0F,1.0F,1.0F}));
    assert(!sphere(-1.0F));
    return 0;
}
