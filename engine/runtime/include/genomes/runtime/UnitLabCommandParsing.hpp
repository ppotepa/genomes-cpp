#pragma once

#include <genomes/runtime/UnitLabScene.hpp>

#include <charconv>
#include <cmath>
#include <iterator>
#include <string_view>
#include <type_traits>

namespace genomes::runtime {

namespace detail {

template <typename Number>
[[nodiscard]] foundation::Result<Number, foundation::Error> parseUnitLabNumber(
    std::string_view text) {
    if (text.empty()) {
        return foundation::Result<Number, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "unit lab command value is empty"});
    }
    Number value{};
    const auto converted = [&] {
        if constexpr (std::is_floating_point_v<Number>) {
            return std::from_chars(text.data(), text.data() + text.size(), value,
                                   std::chars_format::general);
        }
        return std::from_chars(text.data(), text.data() + text.size(), value);
    }();
    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
        (std::is_floating_point_v<Number> && !std::isfinite(value))) {
        return foundation::Result<Number, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid unit lab command number"});
    }
    return foundation::Result<Number, foundation::Error>::success(value);
}

} // namespace detail

[[nodiscard]] inline foundation::Result<SetVariation, foundation::Error>
parseSetVariation(std::string_view text) {
    const auto value = detail::parseUnitLabNumber<double>(text);
    if (!value || !infantry::isValidVariation(value.value())) {
        return foundation::Result<SetVariation, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "variation is outside 0.0..1.75"});
    }
    return foundation::Result<SetVariation, foundation::Error>::success(
        {static_cast<float>(value.value())});
}

[[nodiscard]] inline foundation::Result<SetCameraMode, foundation::Error>
parseSetCameraMode(std::string_view text) {
    static constexpr std::string_view names[] = {
        "3q", "three-quarter", "three quarter", "Three quarter", "front", "Front",
        "side", "Side", "back", "Back", "face", "Face", "hands", "Hands"};
    if (text == names[0] || text == names[1] || text == names[2] || text == names[3])
        return foundation::Result<SetCameraMode, foundation::Error>::success(
            {UnitLabCameraMode::ThreeQuarter});
    for (std::size_t index = 4U; index < std::size(names); index += 2U) {
        if (text == names[index]) {
            return foundation::Result<SetCameraMode, foundation::Error>::success(
                {static_cast<UnitLabCameraMode>((index - 2U) / 2U)});
        }
        if (text == names[index + 1U]) {
            return foundation::Result<SetCameraMode, foundation::Error>::success(
                {static_cast<UnitLabCameraMode>((index - 2U) / 2U)});
        }
    }
    return foundation::Result<SetCameraMode, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, "unknown unit lab camera mode"});
}

[[nodiscard]] inline foundation::Result<SetLocomotionPreset, foundation::Error>
parseSetLocomotionPreset(std::string_view text) {
    if (text == "crouch-walk") {
        return foundation::Result<SetLocomotionPreset, foundation::Error>::success(
            {infantry::BipedPreset::CrouchWalk});
    }
    static constexpr std::string_view names[] = {
        "idle", "Idle", "walk", "Walk", "run", "Run", "crouch", "Crouch",
        "crouch walk", "Crouch Walk", "crouch-walk"};
    for (std::size_t index = 0U; index < std::size(names) - 1U; index += 2U) {
        if (text == names[index]) {
            return foundation::Result<SetLocomotionPreset, foundation::Error>::success(
                {static_cast<infantry::BipedPreset>(index / 2U)});
        }
        if (text == names[index + 1U]) {
            return foundation::Result<SetLocomotionPreset, foundation::Error>::success(
                {static_cast<infantry::BipedPreset>(index / 2U)});
            }
    }
    return foundation::Result<SetLocomotionPreset, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, "unknown unit lab locomotion preset"});
}

[[nodiscard]] inline foundation::Result<infantry::FaceExpression, foundation::Error>
parseUnitLabExpression(std::string_view text) {
    static constexpr std::string_view names[] = {
        "neutral", "alert", "fear", "anger", "pain", "fatigue", "eyes-closed"};
    for (std::size_t index = 0U; index < std::size(names); ++index) {
        if (text == names[index]) {
            return foundation::Result<infantry::FaceExpression, foundation::Error>::success(
                static_cast<infantry::FaceExpression>(index));
        }
    }
    return foundation::Result<infantry::FaceExpression, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, "unknown unit lab expression"});
}

[[nodiscard]] inline foundation::Result<SetGeneOverride, foundation::Error>
parseSetGeneOverride(std::string_view gene_text, std::string_view value_text) {
    const auto gene = infantry::genomeGeneFromName(gene_text);
    const auto value = detail::parseUnitLabNumber<double>(value_text);
    if (!gene || !value || value.value() < 0.0 || value.value() > 1.0) {
        return foundation::Result<SetGeneOverride, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid unit lab gene override"});
    }
    return foundation::Result<SetGeneOverride, foundation::Error>::success(
        {*gene, value.value()});
}

} // namespace genomes::runtime
