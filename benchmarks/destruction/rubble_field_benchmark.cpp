#include <genomes/destruction/RubbleField.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>

int main() {
    using namespace genomes::destruction;

    const auto created = RubbleField::create();
    if (!created) {
        return 1;
    }
    RubbleField field = std::move(created.value());
    const MaterialId brick = MaterialId::fromName("brick");

    constexpr std::uint32_t count = 100'000U;
    std::uint32_t deposited = 0;
    const auto deposit_begin = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < count; ++index) {
        const float x = static_cast<float>(index % 128U) * 0.25F;
        const float z = static_cast<float>((index / 128U) % 128U) * 0.25F;
        const auto result = field.deposit({static_cast<genomes::foundation::StableId>(index + 1U),
                                           {x, 0.0F, z},
                                           0.35F,
                                           {{brick, 0.05}}});
        deposited += result ? 1U : 0U;
    }
    const auto deposit_end = std::chrono::steady_clock::now();

    double query_checksum = 0.0;
    const auto query_begin = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < count; ++index) {
        const float x = static_cast<float>(index % 128U) * 0.25F;
        const float z = static_cast<float>((index / 128U) % 128U) * 0.25F;
        query_checksum += field.surfaceHeightAt(x, z);
        query_checksum += field.movementCostAt(x, z);
    }
    const auto query_end = std::chrono::steady_clock::now();

    const auto relax_begin = std::chrono::steady_clock::now();
    const RubbleRelaxResult relaxed = field.relax();
    const auto relax_end = std::chrono::steady_clock::now();

    const auto deposit_us = std::chrono::duration_cast<std::chrono::microseconds>(
        deposit_end - deposit_begin);
    const auto query_us = std::chrono::duration_cast<std::chrono::microseconds>(
        query_end - query_begin);
    const auto relax_us = std::chrono::duration_cast<std::chrono::microseconds>(
        relax_end - relax_begin);
    std::cout << "rubble_field deposits=" << deposited << " deposit_us=" << deposit_us.count()
              << " queries=" << count << " query_us=" << query_us.count()
              << " transfers=" << relaxed.transfers << " relax_us=" << relax_us.count()
              << " checksum=" << query_checksum << " volume=" << field.totalVolume() << '\n';
    return deposited == count ? 0 : 1;
}
