#pragma once

#include <cstdint>
#include <string_view>

namespace genomes::foundation {

enum class ErrorCode : std::uint32_t {
    None,
    InvalidArgument,
    InvalidState,
    NotFound,
    OutOfRange,
    Unsupported,
    Internal
};

struct Error final {
    ErrorCode code{ErrorCode::None};
    std::string_view message{};
};

} // namespace genomes::foundation
