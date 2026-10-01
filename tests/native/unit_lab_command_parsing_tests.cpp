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
    assert(unitLabAppearancePresetName(appearance.value().value) == "inspection-olive");
    assert(unitLabAppearancePresetName(0).empty());
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
    assert(std::get<SetCameraMode>(cli_command.value()).value == UnitLabCameraMode::Front);
    const std::array<std::string_view, 2> cli_locomotion_tokens{
        "set-locomotion-preset", "run"};
    const auto cli_locomotion = parseUnitLabCommandLine(cli_locomotion_tokens);
    assert(cli_locomotion &&
           std::holds_alternative<SetLocomotionPreset>(cli_locomotion.value()));
    assert(std::get<SetLocomotionPreset>(cli_locomotion.value()).value ==
           infantry::BipedPreset::Run);
    const std::array<std::string_view, 2> cli_expression_tokens{
        "set-expression", "anger"};
    const auto cli_expression = parseUnitLabCommandLine(cli_expression_tokens);
    assert(cli_expression &&
           std::holds_alternative<SetExpression>(cli_expression.value()));
    assert(std::get<SetExpression>(cli_expression.value()).value ==
           infantry::FaceExpression::Anger);
    const std::array<std::string_view, 3> cli_equipment_tokens{
        "set-equipment-slot", "head", "helmet_light"};
    const auto cli_equipment = parseUnitLabCommandLine(cli_equipment_tokens);
    assert(cli_equipment &&
           std::holds_alternative<SetEquipmentSlot>(cli_equipment.value()));
    assert(std::get<SetEquipmentSlot>(cli_equipment.value()).slot ==
           infantry::EquipmentSlot::Head);
    const std::array<std::string_view, 3> cli_gene_tokens{
        "set-gene-override", "height", "0.82"};
    const auto cli_gene = parseUnitLabCommandLine(cli_gene_tokens);
    assert(cli_gene && std::holds_alternative<SetGeneOverride>(cli_gene.value()));
    assert(std::get<SetGeneOverride>(cli_gene.value()).gene == infantry::GenomeGene::Height);
    assert(std::get<SetGeneOverride>(cli_gene.value()).value == 0.82);
    const std::array<std::string_view, 2> cli_appearance_tokens{
        "set-appearance-preset", "inspection-olive"};
    const auto cli_appearance = parseUnitLabCommandLine(cli_appearance_tokens);
    assert(cli_appearance &&
           std::holds_alternative<SetAppearancePreset>(cli_appearance.value()));
    assert(std::get<SetAppearancePreset>(cli_appearance.value()).value ==
           appearance.value().value);
    const std::array<std::string_view, 2> cli_variation_tokens{
        "set-variation", "1.75"};
    const auto cli_variation = parseUnitLabCommandLine(cli_variation_tokens);
    assert(cli_variation && std::holds_alternative<SetVariation>(cli_variation.value()));
    assert(std::get<SetVariation>(cli_variation.value()).value == 1.75F);
    const std::array<std::string_view, 2> cli_variation_trailing{
        "set-variation", "1.75 trailing"};
    const auto cli_variation_trailing_result =
        parseUnitLabCommandLine(cli_variation_trailing);
    assert(!cli_variation_trailing_result);
    assert(cli_variation_trailing_result.error().command == "set-variation");
    assert(cli_variation_trailing_result.error().field == "variation");
    const std::array<std::string_view, 2> cli_variation_nan{
        "set-variation", "nan"};
    assert(!parseUnitLabCommandLine(cli_variation_nan));
    const std::array<std::string_view, 2> cli_variation_inf{
        "set-variation", "inf"};
    assert(!parseUnitLabCommandLine(cli_variation_inf));
    const std::array<std::string_view, 3> cli_gene_trailing{
        "set-gene-override", "height", "0.82 trailing"};
    const auto cli_gene_trailing_result = parseUnitLabCommandLine(cli_gene_trailing);
    assert(!cli_gene_trailing_result);
    assert(cli_gene_trailing_result.error().command == "set-gene-override");
    assert(cli_gene_trailing_result.error().field == "gene-value");
    const std::array<std::string_view, 3> cli_gene_nan{
        "set-gene-override", "height", "NaN"};
    assert(!parseUnitLabCommandLine(cli_gene_nan));
    const std::array<std::string_view, 3> cli_gene_inf{
        "set-gene-override", "height", "Inf"};
    assert(!parseUnitLabCommandLine(cli_gene_inf));
    const std::array<std::string_view, 2> cli_appearance_trailing{
        "set-appearance-preset", "inspection-olive trailing"};
    assert(!parseUnitLabCommandLine(cli_appearance_trailing));
    const std::array<std::string_view, 0> empty_tokens{};
    assert(!parseUnitLabCommandLine(empty_tokens));
    return 0;
}
