#pragma once

#include <genomes/proc/GeneratorId.hpp>

#include <compare>
#include <cstdint>
#include <string_view>

namespace genomes::proc {

struct GeneratorVersion final {
    std::uint16_t major{0};
    std::uint16_t minor{0};
    std::uint16_t patch{0};

    friend constexpr auto operator<=>(const GeneratorVersion&,
                                      const GeneratorVersion&) noexcept = default;
};

enum class GeneratorExecutionPolicy {
    Cpu,
    GpuCapable
};

enum class GeneratorCachePolicy {
    None,
    Artifact
};

struct GeneratorDescriptor final {
    GeneratorId id{};
    std::string_view name{};
    GeneratorVersion version{};
    foundation::StableId input_type{0};
    foundation::StableId output_type{0};
    bool deterministic{true};
    GeneratorExecutionPolicy execution{GeneratorExecutionPolicy::Cpu};
    GeneratorCachePolicy cache{GeneratorCachePolicy::None};

    [[nodiscard]] bool valid() const noexcept {
        return id.isValid() && !name.empty() && version.major != 0 && input_type != 0 &&
               output_type != 0 && (cache == GeneratorCachePolicy::None || deterministic);
    }
};

} // namespace genomes::proc
