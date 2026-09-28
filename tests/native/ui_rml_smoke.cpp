#include <RmlUiRuntime.hpp>

#include <cassert>
#include <filesystem>

class TestRouter final : public genomes::ui::IUiActionRouter {
public:
    genomes::ui::UiActionResult dispatch(
        genomes::ui::UiActionId action,
        const genomes::ui::UiActionArguments&) override {
        last_action = action;
        ++calls;
        return genomes::ui::UiActionResult::Handled;
    }

    genomes::ui::UiActionId last_action{0};
    int calls{0};
};

int main() {
    genomes::ui::rml::Runtime runtime{
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods" / "core", 1280, 720};
    assert(runtime.valid());
    TestRouter router;
    runtime.set_action_router(&router);
    assert(runtime.load_document("ui-test.rml"));
    const auto& frame = runtime.update(1.0 / 60.0);
    assert(frame.revision > 0);
    assert(!frame.commands.empty());
    assert(!frame.commands.front().vertices.empty());
    assert(!frame.commands.front().indices.empty());

    genomes::input::InputFrame input{};
    input.events.push_back({genomes::input::EventType::MouseMove, 0, 0, 0,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    input.events.push_back({genomes::input::EventType::MouseButtonUp, 0, 0, 1,
                            100.0F, 190.0F, 0.0F, 0.0F, {}});
    runtime.process_input(input);
    assert(router.calls > 0);
    assert(router.last_action == genomes::foundation::stable_id("scene.start-battlefield"));
    return 0;
}
