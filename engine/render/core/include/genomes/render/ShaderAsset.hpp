#pragma once

#include <genomes/foundation/Types.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::render {

enum class ShaderStage : std::uint8_t {
    Vertex,
    Pixel,
    Compute,
};

struct ShaderDefine final {
    std::string name;
    std::string value;
};

struct ShaderIncludeDependency final {
    std::string path;
    std::uint64_t content_hash{0};
};

struct ShaderCompileInput final {
    std::string source;
    std::string entry_point{"main"};
    ShaderStage stage{ShaderStage::Vertex};
    std::string compiler_version;
    std::string backend_target;
    std::vector<ShaderDefine> defines;
    std::vector<ShaderIncludeDependency> includes;
};

struct ShaderAssetKey final {
    std::uint64_t digest{0};

    [[nodiscard]] bool valid() const noexcept { return digest != 0; }

    [[nodiscard]] static ShaderAssetKey make(const ShaderCompileInput& input) noexcept {
        std::uint64_t hash = 14695981039346656037ull;
        const auto append = [&hash](std::string_view value) noexcept {
            for (const unsigned char character : value) {
                hash ^= character;
                hash *= 1099511628211ull;
            }
            hash ^= 0xFFu;
            hash *= 1099511628211ull;
        };
        const auto append_number = [&append](std::uint64_t value) noexcept {
            for (unsigned int shift = 0; shift < 64; shift += 8) {
                const char byte = static_cast<char>((value >> shift) & 0xFFu);
                append(std::string_view{&byte, 1});
            }
        };

        append(input.source);
        append(input.entry_point);
        append_number(static_cast<std::uint64_t>(input.stage));
        append(input.compiler_version);
        append(input.backend_target);

        std::vector<ShaderDefine> defines = input.defines;
        std::sort(defines.begin(), defines.end(), [](const ShaderDefine& left,
                                                     const ShaderDefine& right) {
            return left.name == right.name ? left.value < right.value : left.name < right.name;
        });
        for (const ShaderDefine& define : defines) {
            append(define.name);
            append(define.value);
        }

        std::vector<ShaderIncludeDependency> includes = input.includes;
        std::sort(includes.begin(), includes.end(), [](const ShaderIncludeDependency& left,
                                                       const ShaderIncludeDependency& right) {
            return left.path < right.path;
        });
        for (const ShaderIncludeDependency& include : includes) {
            append(include.path);
            append_number(include.content_hash);
        }
        return {hash == 0 ? 1 : hash};
    }

    friend constexpr auto operator<=>(const ShaderAssetKey&, const ShaderAssetKey&) noexcept =
        default;
};

struct ShaderAsset final {
    ShaderAssetKey key{};
    std::string source_path;
    ShaderCompileInput input;
};

} // namespace genomes::render
