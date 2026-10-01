#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/runtime/UnitLabScene.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
#include <string>
#include <string_view>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

namespace genomes::runtime {

struct UnitLabCommandDiagnostic final {
    foundation::ErrorCode code{foundation::ErrorCode::InvalidArgument};
    std::string command;
    std::string field;
    std::string input;
    std::string message;
};

template <typename T>
using UnitLabCommandResult = foundation::Result<T, UnitLabCommandDiagnostic>;

[[nodiscard]] inline UnitLabCommandDiagnostic unitLabDiagnostic(
    std::string_view command, std::string_view field, std::string_view input,
    std::string_view message,
    foundation::ErrorCode code = foundation::ErrorCode::InvalidArgument) {
    return {code, std::string{command}, std::string{field}, std::string{input},
            std::string{message}};
}

namespace detail {

template <typename Number>
[[nodiscard]] UnitLabCommandResult<Number> parseUnitLabNumber(
    std::string_view text, std::string_view field = "value") {
    if (text.empty()) {
        return UnitLabCommandResult<Number>::failure(unitLabDiagnostic(
            "unit-lab", field, text, "unit lab command value is empty"));
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
        return UnitLabCommandResult<Number>::failure(unitLabDiagnostic(
            "unit-lab", field, text, "invalid unit lab command number"));
    }
    return UnitLabCommandResult<Number>::success(value);
}

} // namespace detail

[[nodiscard]] inline UnitLabCommandResult<SetVariation>
parseSetVariation(std::string_view text) {
    const auto value = detail::parseUnitLabNumber<double>(text, "variation");
    if (!value) {
        auto diagnostic = value.error();
        diagnostic.command = "set-variation";
        return UnitLabCommandResult<SetVariation>::failure(std::move(diagnostic));
    }
    if (!infantry::isValidVariation(value.value())) {
        return UnitLabCommandResult<SetVariation>::failure(unitLabDiagnostic(
            "set-variation", "variation", text, "variation is outside 0.0..1.75"));
    }
    return UnitLabCommandResult<SetVariation>::success(
        {static_cast<float>(value.value())});
}

[[nodiscard]] inline UnitLabCommandResult<SetCameraMode>
parseSetCameraMode(std::string_view text) {
    static constexpr std::string_view names[] = {
        "3q", "three-quarter", "three quarter", "Three quarter", "front", "Front",
        "side", "Side", "back", "Back", "face", "Face", "hands", "Hands"};
    if (text == names[0] || text == names[1] || text == names[2] || text == names[3])
        return UnitLabCommandResult<SetCameraMode>::success(
            {UnitLabCameraMode::ThreeQuarter});
    for (std::size_t index = 4U; index < std::size(names); index += 2U) {
        if (text == names[index]) {
            return UnitLabCommandResult<SetCameraMode>::success(
                {static_cast<UnitLabCameraMode>((index - 2U) / 2U)});
        }
        if (text == names[index + 1U]) {
            return UnitLabCommandResult<SetCameraMode>::success(
                {static_cast<UnitLabCameraMode>((index - 2U) / 2U)});
        }
    }
    return UnitLabCommandResult<SetCameraMode>::failure(unitLabDiagnostic(
        "set-camera-mode", "camera", text, "unknown unit lab camera mode"));
}

[[nodiscard]] inline UnitLabCommandResult<SetLocomotionPreset>
parseSetLocomotionPreset(std::string_view text) {
    if (text == "crouch-walk") {
        return UnitLabCommandResult<SetLocomotionPreset>::success(
            {infantry::BipedPreset::CrouchWalk});
    }
    static constexpr std::string_view names[] = {
        "idle", "Idle", "walk", "Walk", "run", "Run", "crouch", "Crouch",
        "crouch walk", "Crouch Walk", "crouch-walk"};
    for (std::size_t index = 0U; index < std::size(names) - 1U; index += 2U) {
        if (text == names[index]) {
            return UnitLabCommandResult<SetLocomotionPreset>::success(
                {static_cast<infantry::BipedPreset>(index / 2U)});
        }
        if (text == names[index + 1U]) {
            return UnitLabCommandResult<SetLocomotionPreset>::success(
                {static_cast<infantry::BipedPreset>(index / 2U)});
        }
    }
    return UnitLabCommandResult<SetLocomotionPreset>::failure(unitLabDiagnostic(
        "set-locomotion-preset", "locomotion", text,
        "unknown unit lab locomotion preset"));
}

[[nodiscard]] inline UnitLabCommandResult<infantry::FaceExpression>
parseUnitLabExpression(std::string_view text) {
    static constexpr std::string_view names[] = {
        "neutral", "Neutral", "alert", "Alert", "fear", "Fear", "anger", "Anger",
        "pain", "Pain", "fatigue", "Fatigue", "eyes-closed", "Eyes closed"};
    for (std::size_t index = 0U; index < std::size(names); index += 2U) {
        if (text == names[index] || text == names[index + 1U]) {
            return UnitLabCommandResult<infantry::FaceExpression>::success(
                static_cast<infantry::FaceExpression>(index / 2U));
        }
    }
    return UnitLabCommandResult<infantry::FaceExpression>::failure(unitLabDiagnostic(
        "set-expression", "expression", text, "unknown unit lab expression"));
}

[[nodiscard]] inline UnitLabCommandResult<SetExpression>
parseSetExpression(std::string_view text) {
    const auto expression = parseUnitLabExpression(text);
    if (!expression) {
        return UnitLabCommandResult<SetExpression>::failure(expression.error());
    }
    return UnitLabCommandResult<SetExpression>::success({expression.value()});
}

[[nodiscard]] inline UnitLabCommandResult<SetEquipmentSlot>
parseSetEquipmentSlot(std::string_view slot_text, std::string_view item_text) {
    const auto slot = std::find_if(
        infantry::EquipmentCatalog::slots().begin(), infantry::EquipmentCatalog::slots().end(),
        [slot_text](const auto& candidate) { return candidate.identifier == slot_text; });
    if (slot == infantry::EquipmentCatalog::slots().end()) {
        return UnitLabCommandResult<SetEquipmentSlot>::failure(unitLabDiagnostic(
            "set-equipment-slot", "slot", slot_text, "unknown unit lab equipment slot"));
    }
    infantry::EquipmentOverride override{};
    if (item_text == "auto") {
        override = infantry::EquipmentOverride::absent();
    } else if (item_text == "none") {
        override = infantry::EquipmentOverride::nullValue();
    } else {
        const auto* item = infantry::EquipmentCatalog::findItem(item_text);
        if (item == nullptr || !item->allows(slot->slot)) {
            return UnitLabCommandResult<SetEquipmentSlot>::failure(unitLabDiagnostic(
                "set-equipment-slot", "item", item_text,
                "equipment item is missing or incompatible with the slot"));
        }
        override = infantry::EquipmentOverride::item(item->id);
    }
    return UnitLabCommandResult<SetEquipmentSlot>::success({slot->slot, override});
}

[[nodiscard]] inline UnitLabCommandResult<SetGeneOverride>
parseSetGeneOverride(std::string_view gene_text, std::string_view value_text) {
    const auto gene = infantry::genomeGeneFromName(gene_text);
    const auto value = detail::parseUnitLabNumber<double>(value_text, "gene-value");
    if (!gene) {
        return UnitLabCommandResult<SetGeneOverride>::failure(unitLabDiagnostic(
            "set-gene-override", "gene", gene_text,
            "invalid unit lab gene override"));
    }
    if (!value) {
        auto diagnostic = value.error();
        diagnostic.command = "set-gene-override";
        return UnitLabCommandResult<SetGeneOverride>::failure(std::move(diagnostic));
    }
    if (value.value() < 0.0 || value.value() > 1.0) {
        return UnitLabCommandResult<SetGeneOverride>::failure(unitLabDiagnostic(
            "set-gene-override", "gene-value", value_text,
            "gene override is outside 0.0..1.0"));
    }
    return UnitLabCommandResult<SetGeneOverride>::success(
        {*gene, value.value()});
}

[[nodiscard]] inline UnitLabCommandResult<SetAppearancePreset>
parseSetAppearancePreset(std::string_view text) {
    if (text == "inspection-olive") {
        return UnitLabCommandResult<SetAppearancePreset>::success(
            {kInspectionOliveAppearancePreset});
    }
    return UnitLabCommandResult<SetAppearancePreset>::failure(unitLabDiagnostic(
        "set-appearance-preset", "preset", text,
        "unknown unit lab appearance preset"));
}

// Boundary adapters serialize a typed appearance command only at the final
// UI dispatch edge. Keeping this mapping next to the parser prevents a CLI
// adapter from accepting one value and silently dispatching another.
[[nodiscard]] inline std::string_view unitLabAppearancePresetName(
    foundation::StableId preset) noexcept {
    if (preset == kInspectionOliveAppearancePreset) return "inspection-olive";
    return {};
}

[[nodiscard]] inline UnitLabCommandResult<UnitLabCommand> parseUnitLabCommand(
    std::string_view command, std::span<const std::string_view> arguments) {
    const auto invalidArity = [&]() {
        return UnitLabCommandResult<UnitLabCommand>::failure(unitLabDiagnostic(
            command, "arguments", {}, "invalid unit lab command argument count"));
    };
    if (command == "set-variation") {
        if (arguments.size() != 1U) return invalidArity();
        const auto parsed = parseSetVariation(arguments[0]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-camera-mode") {
        if (arguments.size() != 1U) return invalidArity();
        const auto parsed = parseSetCameraMode(arguments[0]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-locomotion-preset") {
        if (arguments.size() != 1U) return invalidArity();
        const auto parsed = parseSetLocomotionPreset(arguments[0]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-expression") {
        if (arguments.size() != 1U) return invalidArity();
        const auto parsed = parseSetExpression(arguments[0]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-equipment-slot") {
        if (arguments.size() != 2U) return invalidArity();
        const auto parsed = parseSetEquipmentSlot(arguments[0], arguments[1]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-gene-override") {
        if (arguments.size() != 2U) return invalidArity();
        const auto parsed = parseSetGeneOverride(arguments[0], arguments[1]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    if (command == "set-appearance-preset") {
        if (arguments.size() != 1U) return invalidArity();
        const auto parsed = parseSetAppearancePreset(arguments[0]);
        if (!parsed) return UnitLabCommandResult<UnitLabCommand>::failure(parsed.error());
        return UnitLabCommandResult<UnitLabCommand>::success(UnitLabCommand{parsed.value()});
    }
    return UnitLabCommandResult<UnitLabCommand>::failure(unitLabDiagnostic(
        command, "command", command, "unknown unit lab command"));
}

// Command-line adapters pass the executable's already-tokenized arguments to
// the same parser. No CLI-specific string-to-domain conversion is permitted
// after this boundary.
[[nodiscard]] inline UnitLabCommandResult<UnitLabCommand> parseUnitLabCommandLine(
    std::span<const std::string_view> tokens) {
    if (tokens.empty()) {
        return UnitLabCommandResult<UnitLabCommand>::failure(unitLabDiagnostic(
            "unit-lab", "command", {}, "unit lab command is missing"));
    }
    return parseUnitLabCommand(tokens.front(), tokens.subspan(1U));
}

} // namespace genomes::runtime
