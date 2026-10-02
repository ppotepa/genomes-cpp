#include <genomes/ui/UiRuntime.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <type_traits>

namespace genomes::ui {

namespace {
[[nodiscard]] std::optional<UiScalar> plain_scalar_value(
    const UiScalar& current, const UiScalar& incoming) {
    return std::visit([&incoming](const auto& value) -> std::optional<UiScalar> {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, bool>) {
            if (const auto* boolean = std::get_if<bool>(&incoming)) return *boolean;
            if (const auto* text = std::get_if<std::string>(&incoming)) {
                if (*text == "true" || *text == "1") return true;
                if (*text == "false" || *text == "0") return false;
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
            if (const auto* integer = std::get_if<std::int64_t>(&incoming)) return *integer;
            if (const auto* number = std::get_if<double>(&incoming)) {
                if (!std::isfinite(*number) || *number <
                    static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
                    *number > static_cast<double>(std::numeric_limits<std::int64_t>::max()))
                    return std::nullopt;
                return static_cast<std::int64_t>(*number);
            }
            if (const auto* text = std::get_if<std::string>(&incoming); text != nullptr) {
                std::int64_t parsed = 0;
                const auto result = std::from_chars(text->data(), text->data() + text->size(), parsed);
                if (result.ec == std::errc{} && result.ptr == text->data() + text->size())
                    return parsed;
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<Value, double>) {
            if (const auto* number = std::get_if<double>(&incoming))
                return std::isfinite(*number) ? std::optional<UiScalar>{*number} : std::nullopt;
            if (const auto* integer = std::get_if<std::int64_t>(&incoming))
                return static_cast<double>(*integer);
            if (const auto* text = std::get_if<std::string>(&incoming); text != nullptr) {
                double parsed = 0.0;
                const auto result = std::from_chars(text->data(), text->data() + text->size(), parsed,
                                                    std::chars_format::general);
                if (result.ec == std::errc{} && result.ptr == text->data() + text->size() &&
                    std::isfinite(parsed)) return parsed;
            }
            return std::nullopt;
        } else {
            if (const auto* text = std::get_if<std::string>(&incoming)) return *text;
            return std::nullopt;
        }
    }, current);
}

class SettingsController final : public IUiScreenController {
public:
    void bind(UiDataModel& model) override {
        UiFieldState scale{};
        if (const auto* existing = model.find("ui_scale"); existing != nullptr)
            scale.value = *existing;
        else scale.value = 1.0;
        scale.commit_policy = UiCommitPolicy::Live;
        scale.minimum = 0.75;
        scale.maximum = 1.50;
        scale.step = 0.05;
        (void)model.set_field("ui_scale", std::move(scale));
        UiFieldState diagnostics{};
        if (const auto* existing = model.find("show_diagnostics"); existing != nullptr)
            diagnostics.value = *existing;
        else diagnostics.value = true;
        diagnostics.commit_policy = UiCommitPolicy::Explicit;
        (void)model.set_field("show_diagnostics", std::move(diagnostics));
    }
};

class OverlayController final : public IUiScreenController {};
}

UiRuntime::UiRuntime() {
    register_controller("builtin.settings", [] { return std::make_unique<SettingsController>(); });
    register_controller("builtin.pause", [] { return std::make_unique<OverlayController>(); });
    register_controller("builtin.world-lab", [] { return std::make_unique<OverlayController>(); });
    // Scene-domain ViewModels are owned by SceneDirector, but every manifest
    // route still receives a concrete lifecycle object. This keeps the route
    // stack uniform and gives future scene adapters a stable registration
    // point without coupling the runtime to their domain types.
    for (const auto* id : {"builtin.main-menu", "builtin.world-config", "builtin.battlefield",
                           "builtin.unit-lab", "builtin.building-lab"})
        register_controller(id, [] { return std::make_unique<OverlayController>(); });
    reset_model();
}

void UiRouteStack::replace(UiRoute route) {
    routes_.clear();
    route.revision = revision_ + 1U;
    routes_.push_back(std::move(route));
    ++revision_;
}

void UiRouteStack::push(UiRoute route) {
    route.revision = revision_ + 1U;
    routes_.push_back(std::move(route));
    ++revision_;
}

bool UiRouteStack::pop() {
    if (routes_.empty()) {
        return false;
    }
    routes_.pop_back();
    ++revision_;
    return true;
}

const UiRoute* UiRouteStack::top() const noexcept {
    return routes_.empty() ? nullptr : &routes_.back();
}

void UiRuntime::clear() {
    frame_dirty_ = true;
}

void UiRuntime::sync_route_lifecycle() {
    if (synchronized_route_revision_ == routes_.revision()) return;
    std::vector<UiRoute> resolved_routes;
    resolved_routes.reserve(routes_.routes().size());
    for (const auto& source : routes_.routes()) {
        UiRoute resolved = source;
        if (route_resolver_) route_resolver_(resolved);
        resolved_routes.push_back(std::move(resolved));
    }
    std::size_t common = 0;
    while (common < route_controllers_.size() && common < resolved_routes.size()) {
        const auto& old = route_controllers_[common].route;
        const auto& next = resolved_routes[common];
        if (old.scene != next.scene || old.document != next.document ||
            old.controller != next.controller || old.overlay != next.overlay) break;
        ++common;
    }
    while (route_controllers_.size() > common) {
        auto& state = route_controllers_.back();
        if (state.controller) {
            UiContext context{this, &state.model};
            state.controller->on_exit(context);
        }
        route_controllers_.pop_back();
    }
    for (std::size_t index = common; index < resolved_routes.size(); ++index) {
        const auto& resolved = resolved_routes[index];
        RouteControllerState state{};
        state.route = resolved;
        const auto factory = controller_factories_.find(resolved.controller);
        if (factory != controller_factories_.end()) {
            state.controller = factory->second();
            if (state.controller) {
                model_.for_each_field([&state](std::string_view key, const UiFieldState& field) {
                    if (state.model.find_field(key) == nullptr) (void)state.model.set_field(std::string{key}, field);
                });
                state.controller->bind(state.model);
                UiContext context{this, &state.model};
                state.controller->on_enter(context);
            }
        }
        route_controllers_.push_back(std::move(state));
    }
    synchronized_route_revision_ = routes_.revision();
}

void UiRuntime::register_controller(std::string id, ControllerFactory factory) {
    if (!id.empty() && factory) controller_factories_[std::move(id)] = std::move(factory);
}

bool UiRuntime::activate_controller(std::string_view id) {
    if (id == controller_id_) return true;
    const auto it = controller_factories_.find(std::string{id});
    if (it == controller_factories_.end()) return false;
    UiContext context{this, &model_};
    if (controller_) controller_->on_exit(context);
    model_.clear();
    controller_ = it->second();
    controller_id_ = std::string{id};
    if (!controller_) {
        controller_id_.clear();
        return false;
    }
    controller_->bind(model_);
    controller_->on_enter(context);
    return true;
}

void UiRuntime::set_viewport(std::uint32_t width, std::uint32_t height) noexcept {
    if (frame_.viewport_width != width || frame_.viewport_height != height) {
        frame_.viewport_width = width;
        frame_.viewport_height = height;
        frame_dirty_ = true;
    }
}

void UiRuntime::update(double delta_seconds) {
    sync_route_lifecycle();
    elapsed_seconds_ += std::max(0.0, delta_seconds);
    if (controller_) {
        UiContext context{this, &model_};
        controller_->update(context, std::max(0.0, delta_seconds));
    }
    for (auto& state : route_controllers_) {
        if (state.controller) {
            UiContext context{this, &state.model};
            state.controller->update(context, std::max(0.0, delta_seconds));
        }
    }
}

bool UiRuntime::process_input(const input::InputFrame& input) {
    if (action_router_ == nullptr || !input.mouse_left_pressed) {
        return false;
    }
    const auto& rendered = frame();
    for (const auto& command : rendered.commands) {
        if (command.primitive != UiDrawPrimitive::Quad || !command.enabled) {
            continue;
        }
        const bool inside = input.mouse_x >= command.rect.x &&
                            input.mouse_x <= command.rect.x + command.rect.width &&
                            input.mouse_y >= command.rect.y &&
                            input.mouse_y <= command.rect.y + command.rect.height;
        if (inside) {
            return dispatch(command.id) == UiActionResult::Handled;
        }
    }
    return false;
}

UiActionResult UiRuntime::dispatch(UiActionId action,
                                   const UiActionArguments& arguments) const {
    if (controller_) {
        const auto result = controller_->handle_action(action, arguments);
        if (result != UiActionResult::Unknown) return result;
    }
    if (!route_controllers_.empty()) {
        auto& top = route_controllers_.back();
        if (top.controller) {
            const auto result = top.controller->handle_action(action, arguments);
            if (result != UiActionResult::Unknown) return result;
        }
    }
    if (action_router_ == nullptr) {
        return UiActionResult::Unknown;
    }
    return action_router_->dispatch(action, arguments);
}

UiEventApplyResult UiRuntime::apply_event(const UiEvent& event) {
    UiEventApplyResult result{};
    UiDataModel* target = &model_;
    if (event.route_id != 0U) {
        const auto route = std::find_if(route_controllers_.begin(), route_controllers_.end(),
            [&event](const RouteControllerState& state) {
                return state.route.scene == event.route_id &&
                       (event.route_revision == 0U || state.route.revision == event.route_revision);
            });
        if (route == route_controllers_.end()) return result;
        // Scene routes publish their authoritative fields through the shared
        // scene model. Overlay routes own an independent draft, which must
        // remain isolated until Apply commits it.
        target = route->route.overlay ? &route->model : &model_;
    }
    if (event.field.empty()) return result;
    auto* field = target->find_field(event.field);
    if (field == nullptr) {
        // Scene view-models also expose command-backed controls as plain
        // scalars. Keep those values in the typed event boundary instead of
        // requiring every existing scene to duplicate a UiFieldState solely
        // to forward an action. Plain scalars use the same OnChange policy;
        // click/submit events are commits for button-like controls.
        const auto* current = target->find(event.field);
        if (current == nullptr) return result;
        result.consumed = true;
        const auto converted = plain_scalar_value(*current, event.value);
        if (!converted.has_value()) return result;
        result.changed = target->set(event.field, *converted);
        result.value = *target->find(event.field);
        result.publish = event.phase == UiEventPhase::Change ||
                         event.phase == UiEventPhase::Click ||
                         event.phase == UiEventPhase::Submit;
        return result;
    }
    result.consumed = true;
    if (!field->enabled || !field->visible || field->read_only) return result;

    const UiScalar previous = field->value;
    bool accepted = false;
    if (std::holds_alternative<double>(previous) || std::holds_alternative<std::int64_t>(previous)) {
        const std::string text = std::visit([](const auto& value) -> std::string {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, double> || std::is_same_v<Value, std::int64_t>)
                return std::to_string(value);
            if constexpr (std::is_same_v<Value, std::string>) return value;
            return {};
        }, event.value);
        (void)target->apply_number(event.field, text);
        field = target->find_field(event.field);
        accepted = field != nullptr && field->error.empty();
    } else if (!field->options.empty()) {
        const auto* value = std::get_if<std::string>(&event.value);
        if (value != nullptr) {
            (void)target->apply_option(event.field, *value);
            field = target->find_field(event.field);
            accepted = field != nullptr && field->error.empty();
        }
    } else if (std::holds_alternative<bool>(previous) && std::holds_alternative<bool>(event.value)) {
        (void)target->set(event.field, event.value);
        accepted = true;
    } else if (std::holds_alternative<std::string>(previous) &&
               std::holds_alternative<std::string>(event.value)) {
        (void)target->set(event.field, event.value);
        accepted = true;
    }
    if (!accepted) return result;
    field = target->find_field(event.field);
    result.value = field->value;
    result.changed = previous != field->value;
    switch (field->commit_policy) {
    case UiCommitPolicy::Live:
        result.publish = result.changed;
        break;
    case UiCommitPolicy::OnChange:
        result.publish = event.phase == UiEventPhase::Change ||
                         event.phase == UiEventPhase::Click ||
                         event.phase == UiEventPhase::Submit;
        break;
    case UiCommitPolicy::Explicit:
        result.publish = false;
        break;
    }
    return result;
}

const UiRenderFrame& UiRuntime::frame() const noexcept {
    if (frame_dirty_) {
        rebuild_frame();
    }
    return frame_;
}

void UiRuntime::rebuild_frame() const {
    frame_.commands.clear();
    ++frame_.revision;
    frame_dirty_ = false;
}

} // namespace genomes::ui
