#include "infantry_fixture_reader.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <type_traits>

namespace genomes::test::infantry_fixture {
namespace {

template <class T>
T readLittle(std::istream& input) {
    static_assert(std::is_integral_v<T>);
    std::array<std::byte, sizeof(T)> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input) throw std::runtime_error("truncated GNIF integer");
    T value{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        value |= static_cast<T>(std::to_integer<unsigned char>(bytes[index])) << (index * 8U);
    }
    return value;
}

std::size_t scalarSize(ScalarType type) {
    switch (type) {
    case ScalarType::Float32: return 4U;
    case ScalarType::Float64: return 8U;
    case ScalarType::Uint16: return 2U;
    case ScalarType::Uint32: return 4U;
    }
    throw std::runtime_error("unknown GNIF scalar type");
}

double valueAt(const Stream& stream, std::size_t index) {
    const std::byte* source = stream.bytes.data() + index * scalarSize(stream.type);
    const auto bitsAt = [source]<class T>() {
        T value{};
        for (std::size_t byte = 0; byte < sizeof(T); ++byte) {
            value |= static_cast<T>(std::to_integer<unsigned char>(source[byte])) << (byte * 8U);
        }
        return value;
    };
    switch (stream.type) {
    case ScalarType::Float32: {
        return std::bit_cast<float>(bitsAt.template operator()<std::uint32_t>());
    }
    case ScalarType::Float64: {
        return std::bit_cast<double>(bitsAt.template operator()<std::uint64_t>());
    }
    case ScalarType::Uint16: return bitsAt.template operator()<std::uint16_t>();
    case ScalarType::Uint32: return bitsAt.template operator()<std::uint32_t>();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

double streamTolerance(std::string_view name, ScalarType type) {
    if (type != ScalarType::Float32) return 1.0;
    if (name.find(".normals") != std::string_view::npos) return 2.0e-5;
    if (name.find("rotations") != std::string_view::npos) return 2.0e-4;
    return 2.0e-6;
}

std::string quantizedHash(const Stream& stream) {
    constexpr std::uint64_t offset = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t prime = 0x100000001b3ULL;
    std::uint64_t hash = offset;
    const double tolerance = streamTolerance(stream.name, stream.type);
    for (std::size_t index = 0; index < stream.element_count; ++index) {
        std::uint64_t value = 0;
        if (stream.type == ScalarType::Uint16 || stream.type == ScalarType::Uint32) {
            value = static_cast<std::uint64_t>(valueAt(stream, index));
        } else {
            const double scaled = valueAt(stream, index) / tolerance;
            const auto rounded = static_cast<std::int64_t>(std::floor(scaled + 0.5));
            value = static_cast<std::uint64_t>(rounded);
        }
        for (unsigned byte = 0; byte < 8U; ++byte) {
            hash ^= (value >> (byte * 8U)) & 0xffU;
            hash *= prime;
        }
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

} // namespace

const Stream* Fixture::find(std::string_view name) const noexcept {
    const auto found = std::find_if(streams.begin(), streams.end(),
                                    [name](const Stream& stream) { return stream.name == name; });
    return found == streams.end() ? nullptr : &*found;
}

Fixture read(const std::filesystem::path& binary, const std::filesystem::path& manifest) {
    Fixture result{};
    std::ifstream json_input(manifest);
    if (!json_input || !(json_input >> result.manifest)) {
        throw std::runtime_error("invalid GNIF manifest");
    }
    std::ifstream input(binary, std::ios::binary);
    char magic[4]{};
    input.read(magic, sizeof(magic));
    if (!input || std::string_view(magic, 4U) != "GNIF") throw std::runtime_error("invalid GNIF magic");
    if (readLittle<std::uint32_t>(input) != 1U) throw std::runtime_error("unsupported GNIF version");
    const auto count = readLittle<std::uint32_t>(input);
    result.streams.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        Stream stream{};
        const auto name_size = readLittle<std::uint16_t>(input);
        stream.name.resize(name_size);
        input.read(stream.name.data(), name_size);
        stream.type = static_cast<ScalarType>(readLittle<std::uint8_t>(input));
        (void)readLittle<std::uint8_t>(input);
        (void)readLittle<std::uint8_t>(input);
        (void)readLittle<std::uint8_t>(input);
        stream.element_count = readLittle<std::uint64_t>(input);
        const auto byte_count = stream.element_count * scalarSize(stream.type);
        if (byte_count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
            throw std::runtime_error("GNIF stream too large");
        stream.bytes.resize(static_cast<std::size_t>(byte_count));
        input.read(reinterpret_cast<char*>(stream.bytes.data()),
                   static_cast<std::streamsize>(stream.bytes.size()));
        if (!input) throw std::runtime_error("truncated GNIF stream");
        result.streams.push_back(std::move(stream));
    }
    if (input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing GNIF data");
    if (result.manifest.contains("streams")) {
        const auto& descriptions = result.manifest.at("streams");
        if (!descriptions.is_array() || descriptions.size() != result.streams.size())
            throw std::runtime_error("GNIF manifest stream count differs");
        for (std::size_t index = 0; index < result.streams.size(); ++index) {
            const auto& description = descriptions.at(index);
            const auto& stream = result.streams[index];
            if (description.at("name").get<std::string>() != stream.name ||
                description.at("type").get<std::uint8_t>() !=
                    static_cast<std::uint8_t>(stream.type) ||
                description.at("elements").get<std::uint64_t>() != stream.element_count ||
                description.at("bytes").get<std::uint64_t>() != stream.bytes.size())
                throw std::runtime_error("GNIF manifest stream descriptor differs");
            const auto hash = description.value("quantizedFNV1a64", std::string{});
            if (hash.size() != 16U)
                throw std::runtime_error("GNIF stream is missing quantized FNV-1a64 hash");
            if (hash != quantizedHash(stream))
                throw std::runtime_error("GNIF stream quantized FNV-1a64 hash mismatch");
        }
    }
    return result;
}

ComparisonReport compare(const Stream& expected, const Stream& actual, double tolerance) {
    ComparisonReport report{};
    if (expected.type != actual.type || expected.element_count != actual.element_count) {
        report.equal = false;
        report.message = "stream type or element count differs";
        return report;
    }
    bool first = true;
    for (std::size_t index = 0; index < expected.element_count; ++index) {
        const double expected_value = valueAt(expected, index);
        const double actual_value = valueAt(actual, index);
        const double error = std::abs(expected_value - actual_value);
        report.maximum_error = std::max(report.maximum_error, error);
        if ((!std::isfinite(error) || error > tolerance) && first) {
            first = false;
            report.equal = false;
            report.first_difference = index;
            report.first_expected = expected_value;
            report.first_actual = actual_value;
            report.message = "stream value exceeds tolerance";
        }
    }
    return report;
}

} // namespace genomes::test::infantry_fixture
