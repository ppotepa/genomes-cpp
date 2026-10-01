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
    runtime.routes().pop();
    runtime.update(0.0);
    assert(log == "enter:base;enter:overlay;exit:overlay;");
    return 0;
}
