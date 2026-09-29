#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace genomes::test::infantry_fixture {

enum class ScalarType : std::uint8_t { Float32 = 1, Float64 = 2, Uint16 = 3, Uint32 = 4 };

struct Stream final {
    std::string name;
    ScalarType type{};
    std::uint64_t element_count{0};
    std::vector<std::byte> bytes;
};

struct Fixture final {
    nlohmann::json manifest;
    std::vector<Stream> streams;

    [[nodiscard]] const Stream* find(std::string_view name) const noexcept;
};

struct ComparisonReport final {
    bool equal{true};
    std::size_t first_difference{0};
    double first_expected{0.0};
    double first_actual{0.0};
    double maximum_error{0.0};
    std::string message;
};

[[nodiscard]] Fixture read(const std::filesystem::path& binary,
                           const std::filesystem::path& manifest);
[[nodiscard]] ComparisonReport compare(const Stream& expected, const Stream& actual,
                                       double tolerance);

} // namespace genomes::test::infantry_fixture
