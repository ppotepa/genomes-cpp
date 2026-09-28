#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace genomes::infantry {

enum class FaceExpression : std::uint8_t {
    Neutral,
    Alert,
    Fear,
    Anger,
    Pain,
    Fatigue,
    EyesClosed,
};

enum class FaceChannel : std::uint8_t {
    EyeOpen,
    EyeSquint,
    BrowInnerUp,
    BrowOuterUp,
    BrowDown,
    JawOpen,
    MouthOpen,
    MouthStretch,
    MouthCornerUp,
    MouthCornerDown,
    LipPress,
    CheekRaise,
};

inline constexpr std::size_t kFaceExpressionCount = 7U;
inline constexpr std::size_t kFaceChannelCount = 12U;

struct ExpressionProfile final {
    std::array<float, kFaceChannelCount> channels{};

    [[nodiscard]] static ExpressionProfile forExpression(FaceExpression) noexcept;
};

} // namespace genomes::infantry
