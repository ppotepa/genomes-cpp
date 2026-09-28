#include <genomes/navigation/FlowField.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using clock = std::chrono::steady_clock;
    constexpr std::uint32_t size = 256U;
    const auto cells = static_cast<std::size_t>(size) * size;
    std::vector<std::uint8_t> blocked(cells, 0U);
    for (std::uint32_t z = 0U; z < size; ++z) {
        blocked[static_cast<std::size_t>(z) * size + size / 2U] = (z % 11U == 0U) ? 0U : 1U;
    }
    const genomes::navigation::FlowFieldDescriptor descriptor{
        {1U, 7U, 9U, size, size}, {cells - 1U}};
    const auto start = clock::now();
    const auto result = genomes::navigation::FlowFieldBuilder::build(descriptor, blocked);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
    if (!result) {
        return 1;
    }
    std::size_t reachable = 0U;
    for (std::uint32_t z = 0U; z < size; ++z) {
        for (std::uint32_t x = 0U; x < size; ++x) {
            reachable += result.value().sample(x, z).reachable ? 1U : 0U;
        }
    }
    std::cout << "flow_field size=" << size << " ms=" << elapsed.count()
              << " reachable=" << reachable << '\n';
    return 0;
}
