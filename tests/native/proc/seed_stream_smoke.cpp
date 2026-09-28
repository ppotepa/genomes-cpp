#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <cassert>
#include <cstdint>

int main() {
    const genomes::proc::SeedPath root(0x5EED2026ull);
    const auto terrain0 = root.child("terrain", 0);
    const auto terrain1 = root.child("terrain", 1);
    const auto building0 = root.child("building", 0);

    assert(terrain0.seed() == 0xd6ef8f01ce6bc1dfull);
    assert(terrain1.seed() == 0xb7f4c7f8c37c77beull);
    assert(building0.seed() == 0xb2b615768d308c59ull);
    assert(terrain0 != terrain1);
    assert(terrain0 != building0);
    assert(terrain0 == root.child("terrain", 0));
    assert(root.childStableId("terrain", 0) == terrain0);

    genomes::proc::RandomStream stream(terrain0);
    assert(stream.nextU32() == 0xe0e040ccu);
    assert(stream.nextU32() == 0x1405122fu);
    assert(stream.nextU32() == 0xbe72004fu);

    genomes::proc::RandomStream bounded_stream(root.child("bounded", 0));
    for (int index = 0; index < 1000; ++index) {
        const auto value = bounded_stream.bounded(7);
        assert(value < 7);
        const double unit = bounded_stream.uniform01();
        assert(unit >= 0.0 && unit < 1.0);
    }
    assert(genomes::proc::RandomStream(root.child("zero", 0)).bounded(0) == 0);
    return 0;
}
