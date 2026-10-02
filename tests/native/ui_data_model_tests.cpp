#include <genomes/ui/UiRuntime.hpp>

#include <cassert>

int main() {
    using namespace genomes::ui;
    UiDataModel model;
    assert(model.set("seed", std::int64_t{7}));
    const auto first_revision = model.revision();
    assert(!model.set("seed", std::int64_t{7}));
    assert(model.revision() == first_revision);

    UiFieldState amount{};
    amount.value = 0.0;
    amount.minimum = 0.0;
    amount.maximum = 10.0;
    amount.step = 0.5;
    amount.commit_policy = UiCommitPolicy::Live;
    assert(model.set_field("amount", amount));
    assert(model.apply_number("amount", "12.26"));
    assert(std::get<double>(*model.find("amount")) == 10.0);
    assert(!model.apply_number("amount", "NaN"));
    assert(std::get<double>(*model.find("amount")) == 10.0);
    assert(!model.validate());
    auto* amount_field = model.find_field("amount");
    assert(amount_field != nullptr);
    amount_field->error.clear();
    amount_field->enabled = false;
    const auto disabled_revision = model.revision();
    assert(!model.apply_number("amount", "4.0"));
    assert(model.revision() > disabled_revision);
    amount_field->enabled = true;
    amount_field->visible = false;
    assert(!model.apply_number("amount", "4.0"));
    amount_field->visible = true;

    UiFieldState integer{};
    integer.value = std::int64_t{2};
    integer.minimum = 0.0;
    integer.maximum = 10.0;
    integer.step = 1.0;
    assert(model.set_field("integer", integer));
    assert(model.apply_number("integer", "3.6"));
    assert(std::get<std::int64_t>(*model.find("integer")) == 4);

    UiFieldState choice{};
    choice.value = std::string{"a"};
    choice.options = {{"a", "A", true}, {"b", "B", true}, {"disabled", "Disabled", false}};
    choice.commit_policy = UiCommitPolicy::Explicit;
    assert(model.set_field("choice", choice));
    assert(!model.apply_option("choice", "missing"));
    assert(!model.apply_option("choice", "disabled"));
    assert(std::get<std::string>(*model.find("choice")) == "a");
    assert(model.apply_option("choice", "b"));
    model.acknowledge_changes();
    assert(model.changed_keys().empty());

    UiRuntime runtime;
    UiFieldState live = amount;
    live.value = 1.0;
    assert(runtime.model().set_field("live", live));
    UiEvent event{};
    event.field = "live";
    event.phase = UiEventPhase::Input;
    event.value = 12.26;
    const auto live_result = runtime.apply_event(event);
    assert(live_result.consumed && live_result.changed && live_result.publish);
    assert(std::get<double>(live_result.value) == 10.0);

    UiFieldState on_change = live;
    on_change.value = 1.0;
    on_change.commit_policy = UiCommitPolicy::OnChange;
    assert(runtime.model().set_field("on_change", on_change));
    event.field = "on_change";
    event.value = 2.24;
    const auto input_result = runtime.apply_event(event);
    assert(input_result.consumed && input_result.changed && !input_result.publish);
    event.phase = UiEventPhase::Change;
    const auto change_result = runtime.apply_event(event);
    assert(change_result.consumed && !change_result.changed && change_result.publish);
    assert(std::get<double>(change_result.value) == 2.0);

    UiFieldState explicit_choice = choice;
    explicit_choice.value = std::string{"a"};
    assert(runtime.model().set_field("explicit_choice", explicit_choice));
    event.field = "explicit_choice";
    event.value = std::string{"b"};
    const auto draft_result = runtime.apply_event(event);
    assert(draft_result.consumed && draft_result.changed && !draft_result.publish);
    assert(std::get<std::string>(*runtime.model().find("explicit_choice")) == "b");

    // Command-backed scene controls may intentionally be plain scalars. They
    // still cross the same typed event boundary and publish on their commit
    // event instead of being rejected before the scene handler sees them.
    assert(runtime.model().set("seed_text", std::string{"7"}));
    event.field = "seed_text";
    event.phase = UiEventPhase::Input;
    event.value = std::string{"8"};
    const auto plain_input = runtime.apply_event(event);
    assert(plain_input.consumed && plain_input.changed && !plain_input.publish);
    event.phase = UiEventPhase::Change;
    const auto plain_change = runtime.apply_event(event);
    assert(plain_change.consumed && plain_change.publish);
    assert(std::get<std::string>(*runtime.model().find("seed_text")) == "8");

    assert(runtime.model().set("plain_toggle", false));
    event.field = "plain_toggle";
    event.phase = UiEventPhase::Change;
    event.value = true;
    const auto plain_bool = runtime.apply_event(event);
    assert(plain_bool.consumed && plain_bool.changed && plain_bool.publish);
    assert(std::get<bool>(*runtime.model().find("plain_toggle")));

    UiFieldState click_choice = choice;
    click_choice.value = std::string{"a"};
    click_choice.commit_policy = UiCommitPolicy::OnChange;
    assert(runtime.model().set_field("click_choice", click_choice));
    event.field = "click_choice";
    event.phase = UiEventPhase::Click;
    event.value = std::string{"b"};
    const auto click_result = runtime.apply_event(event);
    assert(click_result.consumed && click_result.changed && click_result.publish);

    auto* unavailable = runtime.model().find_field("live");
    unavailable->read_only = true;
    event.field = "live";
    event.value = 3.0;
    const auto unavailable_result = runtime.apply_event(event);
    assert(unavailable_result.consumed && !unavailable_result.changed &&
           !unavailable_result.publish);
    return 0;
}
