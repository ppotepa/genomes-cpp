#include <genomes/runtime/UnitLabCommandParsing.hpp>

#include <cassert>
#include <array>
#include <string_view>

int main() {
    using namespace genomes::runtime;
    UnitLabDirtyState dirty;
    assert(dirty.contains(UnitLabDirtyFlag::Geometry));
    dirty.clearAll();
    dirty.mark(UnitLabDirtyFlag::Material);
    assert(!dirty.contains(UnitLabDirtyFlag::Geometry));
    assert(dirty.contains(UnitLabDirtyFlag::Material));
    dirty.mark(UnitLabDirtyFlag::Pose);
    dirty.clear(UnitLabDirtyFlag::Material);
    assert(dirty.contains(UnitLabDirtyFlag::Pose));
    assert(!dirty.contains(UnitLabDirtyFlag::Material));
    dirty.markAll();
    assert(dirty.contains(UnitLabDirtyFlag::Ui));

    assert(parseSetVariation("1.75"));
    assert(!parseSetVariation("1.750 trailing"));
    assert(parseSetVariation("1.750 trailing").error().command == "set-variation");
    assert(parseSetVariation("1.750 trailing").error().field == "variation");
    assert(parseSetVariation("1.750 trailing").error().input == "1.750 trailing");
    assert(!parseSetVariation("nan"));
    assert(!parseSetVariation("2.0"));

    assert(parseSetCameraMode("3q").value().value == UnitLabCameraMode::ThreeQuarter);
    assert(parseSetCameraMode("three-quarter").value().value == UnitLabCameraMode::ThreeQuarter);
    assert(!parseSetCameraMode("front trailing"));
    assert(parseSetLocomotionPreset("crouch-walk").value().value ==
           infantry::BipedPreset::CrouchWalk);
    assert(parseSetLocomotionPreset("Crouch Walk").value().value ==
           infantry::BipedPreset::CrouchWalk);
    assert(!parseSetLocomotionPreset("run "));
    assert(parseUnitLabExpression("eyes-closed"));
    assert(parseUnitLabExpression("Eyes closed"));
    assert(parseSetExpression("anger").value().value == infantry::FaceExpression::Anger);

    const auto equipment = parseSetEquipmentSlot("head", "helmet_light");
    assert(equipment && equipment.value().slot == infantry::EquipmentSlot::Head);
    assert(equipment.value().value.specified && !equipment.value().value.empty);
    assert(!parseSetEquipmentSlot("head", "not-an-item"));
    assert(parseSetEquipmentSlot("head", "not-an-item").error().field == "item");
    assert(!parseSetEquipmentSlot("not-a-slot", "auto"));
    assert(parseSetEquipmentSlot("not-a-slot", "auto").error().field == "slot");

    const auto gene = parseSetGeneOverride("height", "0.82");
    assert(gene && gene.value().gene == infantry::GenomeGene::Height);
    const auto invalid_gene_value = parseSetGeneOverride("height", "inf");
    assert(!invalid_gene_value);
    assert(invalid_gene_value.error().command == "set-gene-override");
    assert(invalid_gene_value.error().field == "gene-value");
    const auto out_of_range_gene_value = parseSetGeneOverride("height", "1.1");
    assert(!out_of_range_gene_value);
    assert(out_of_range_gene_value.error().field == "gene-value");
    const auto invalid_gene = parseSetGeneOverride("not-a-gene", "0.5");
    assert(!invalid_gene);
    assert(invalid_gene.error().field == "gene");
    const auto appearance = parseSetAppearancePreset("inspection-olive");
    assert(appearance && appearance.value().value == kInspectionOliveAppearancePreset);
    assert(!parseSetAppearancePreset("random"));
    assert(parseSetAppearancePreset("random").error().field == "preset");

    const std::array<std::string_view, 1> variation_args{"1.25"};
    const auto typed_variation = parseUnitLabCommand("set-variation", variation_args);
    assert(typed_variation);
    assert(std::holds_alternative<SetVariation>(typed_variation.value()));
    assert(std::get<SetVariation>(typed_variation.value()).value == 1.25F);

    const std::array<std::string_view, 2> equipment_args{"head", "helmet_light"};
    const auto typed_equipment = parseUnitLabCommand("set-equipment-slot", equipment_args);
    assert(typed_equipment);
    assert(std::holds_alternative<SetEquipmentSlot>(typed_equipment.value()));
    const std::array<std::string_view, 1> appearance_args{"inspection-olive"};
    const auto typed_appearance = parseUnitLabCommand(
        "set-appearance-preset", appearance_args);
    assert(typed_appearance);
    assert(std::holds_alternative<SetAppearancePreset>(typed_appearance.value()));
    const std::array<std::string_view, 2> trailing_args{"height", "0.5 trailing"};
    const auto rejected = parseUnitLabCommand("set-gene-override", trailing_args);
    assert(!rejected);
    assert(rejected.error().command == "set-gene-override");
    const std::array<std::string_view, 2> wrong_arity{"1.0", "extra"};
    const auto arity = parseUnitLabCommand("set-variation", wrong_arity);
    assert(!arity);
    assert(arity.error().field == "arguments");
    const std::array<std::string_view, 1> unknown_args{"x"};
    const auto unknown = parseUnitLabCommand("set-unknown", unknown_args);
    assert(!unknown);
    assert(unknown.error().field == "command");
    const std::array<std::string_view, 2> cli_tokens{"set-camera-mode", "front"};
    const auto cli_command = parseUnitLabCommandLine(cli_tokens);
    assert(cli_command && std::holds_alternative<SetCameraMode>(cli_command.value()));
    const std::array<std::string_view, 0> empty_tokens{};
    assert(!parseUnitLabCommandLine(empty_tokens));
    return 0;
}
