#include <genomes/ui/UiRuntime.hpp>

#include <cassert>
#include <memory>
#include <string>

namespace {
struct Probe final : genomes::ui::IUiScreenController {
    explicit Probe(std::string name, std::string& log) : name_(std::move(name)), log_(log) {}
    void bind(genomes::ui::UiDataModel& model) override { (void)model.set("bound", name_); }
    void on_enter(genomes::ui::UiContext&) override { log_ += "enter:" + name_ + ";"; }
    void on_exit(genomes::ui::UiContext&) override { log_ += "exit:" + name_ + ";"; }
    std::string name_;
    std::string& log_;
};
}

int main() {
    genomes::ui::UiRuntime runtime;
    std::string log;
    runtime.register_controller("base", [&] { return std::make_unique<Probe>("base", log); });
    runtime.register_controller("overlay", [&] { return std::make_unique<Probe>("overlay", log); });
    runtime.routes().replace({genomes::foundation::scene_id("scene.base"), {}, "base", {}, false});
    runtime.update(0.0);
    assert(log == "enter:base;");
    runtime.routes().push({genomes::foundation::scene_id("scene.overlay"), {}, "overlay", {}, true});
    runtime.update(0.0);
    assert(log == "enter:base;enter:overlay;");
    assert(runtime.route_controllers().size() == 2U);
    auto& base_model = runtime.route_controllers()[0].model;
    auto& overlay_model = runtime.route_controllers()[1].model;
    genomes::ui::UiFieldState base_field{};
    base_field.value = 1.0;
    base_field.minimum = 0.0;
    base_field.maximum = 2.0;
    base_field.commit_policy = genomes::ui::UiCommitPolicy::Live;
    (void)runtime.model().set_field("base_field", base_field);
    genomes::ui::UiEvent base_event{};
    base_event.route_id = genomes::foundation::scene_id("scene.base");
    base_event.route_revision = runtime.route_controllers()[0].route.revision;
    base_event.field = "base_field";
    base_event.phase = genomes::ui::UiEventPhase::Input;
    base_event.value = 1.5;
    const auto base_result = runtime.apply_event(base_event);
    assert(base_result.consumed && base_result.publish);
    assert(std::get<double>(*runtime.model().find("base_field")) == 1.5);
    genomes::ui::UiFieldState overlay_field{};
    overlay_field.value = 1.0;
    overlay_field.minimum = 0.0;
    overlay_field.maximum = 2.0;
    overlay_field.commit_policy = genomes::ui::UiCommitPolicy::Live;
    (void)overlay_model.set_field("overlay_field", overlay_field);
    genomes::ui::UiEvent overlay_event{};
    overlay_event.route_id = genomes::foundation::scene_id("scene.overlay");
    overlay_event.route_revision = runtime.route_controllers()[1].route.revision;
    overlay_event.field = "overlay_field";
    overlay_event.phase = genomes::ui::UiEventPhase::Input;
    overlay_event.value = 1.75;
    const auto overlay_result = runtime.apply_event(overlay_event);
    assert(overlay_result.consumed && overlay_result.publish);
    assert(std::get<double>(*overlay_model.find("overlay_field")) == 1.75);
    assert(base_model.set("scale", 1.0));
    assert(overlay_model.set("scale", 1.25));
    assert(std::get<double>(*base_model.find("scale")) == 1.0);
    assert(std::get<double>(*overlay_model.find("scale")) == 1.25);
    runtime.routes().pop();
    runtime.update(0.0);
    assert(log == "enter:base;enter:overlay;exit:overlay;");
    return 0;
}
