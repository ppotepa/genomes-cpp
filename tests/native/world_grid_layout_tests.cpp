#include <genomes/world/GridLayout.hpp>

#include <cassert>

int main() {
    using genomes::world::GridLayout;
    const auto small = GridLayout::forMap(128U);
    assert(small.valid() && small.cell_count == 16U && small.sample_count == 17U);
    const auto standard = GridLayout::forMap(600U);
    assert(standard.valid() && standard.cell_count == 75U && standard.extent_m == 600.0F);
    const auto large = GridLayout::forMap(4096U);
    assert(large.valid() && large.cell_count == 512U && large.sample_count == 513U);
    assert(!GridLayout::forMap(129U).valid());
    assert(!GridLayout::forMap(601U).valid());
    assert(standard.origin.x == -300.0F && standard.origin.z == -300.0F);
    return 0;
}
