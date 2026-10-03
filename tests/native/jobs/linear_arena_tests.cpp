#include <genomes/memory/LinearArena.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <vector>

int main() {
    genomes::memory::LinearArena arena(256);
    auto* first = static_cast<std::byte*>(arena.allocate(32, 32));
    auto* second = static_cast<std::byte*>(arena.allocate(512, 64));
    assert(reinterpret_cast<std::uintptr_t>(first) % 32U == 0);
    assert(reinterpret_cast<std::uintptr_t>(second) % 64U == 0);
    first[0] = std::byte{0x2A};
    second[0] = std::byte{0x17};
    assert(first[0] == std::byte{0x2A});
    assert(second[0] == std::byte{0x17});
    assert(arena.capacity() >= 768);
    assert(arena.bytesUsed() >= 544);

    arena.reset();
    assert(arena.bytesUsed() == 0);
    assert(arena.allocate(32, 32) == first);

    genomes::memory::LinearArenaResource resource(arena);
    std::pmr::vector<std::uint32_t> values(&resource);
    values.resize(64, 7U);
    assert(values.front() == 7U);
    assert(values.back() == 7U);
    return 0;
}
