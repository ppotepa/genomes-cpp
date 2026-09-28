#pragma once

#include <string_view>

namespace genomes::foundation {

struct BuildInfo final {
    std::string_view projectName;
    std::string_view nativeBootstrapVersion;
};

[[nodiscard]] BuildInfo buildInfo() noexcept;

} // namespace genomes::foundation
