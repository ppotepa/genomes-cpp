#include <genomes/ui/UiDataModel.hpp>

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
    return 0;
}
