#include <genomes/ui/UiRuntime.hpp>

#include <algorithm>

namespace genomes::ui {

void UiRouteStack::replace(UiRoute route) {
    routes_.clear();
    routes_.push_back(std::move(route));
    ++revision_;
}

void UiRouteStack::push(UiRoute route) {
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
    widgets_.clear();
    frame_dirty_ = true;
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

UiWidget& UiRuntime::add(UiWidget widget) {
    widgets_.push_back(std::move(widget));
    frame_dirty_ = true;
    return widgets_.back();
}

void UiRuntime::set_viewport(std::uint32_t width, std::uint32_t height) noexcept {
    if (frame_.viewport_width != width || frame_.viewport_height != height) {
        frame_.viewport_width = width;
        frame_.viewport_height = height;
        frame_dirty_ = true;
    }
}

void UiRuntime::update(double delta_seconds) {
    elapsed_seconds_ += std::max(0.0, delta_seconds);
    if (controller_) {
        UiContext context{this, &model_};
        controller_->update(context, std::max(0.0, delta_seconds));
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
    if (action_router_ == nullptr) {
        return UiActionResult::Unknown;
    }
    return action_router_->dispatch(action, arguments);
}

const UiRenderFrame& UiRuntime::frame() const noexcept {
    if (frame_dirty_) {
        rebuild_frame();
    }
    return frame_;
}

void UiRuntime::rebuild_frame() const {
    frame_.commands.clear();
    frame_.widgets = widgets_;
    float row_y = 164.0F;
    for (const UiWidget& widget : widgets_) {
        UiDrawCommand command{};
        command.id = widget.id;
        command.text = widget.text;
        command.enabled = widget.enabled;
        command.selected = widget.selected;
        command.rect = {78.0F, row_y, widget.width, widget.height};
        switch (widget.type) {
        case UiWidgetType::Panel:
            command.primitive = UiDrawPrimitive::Quad;
            command.rect = {48.0F, 48.0F, widget.width, widget.height};
            command.color = {0.025F, 0.035F, 0.06F, 0.97F};
            break;
        case UiWidgetType::Button:
            command.primitive = UiDrawPrimitive::Quad;
            command.color = widget.selected ? foundation::Color{0.10F, 0.38F, 0.67F, 1.0F}
                                             : foundation::Color{0.07F, 0.11F, 0.18F, 1.0F};
            row_y += widget.height + 12.0F;
            break;
        case UiWidgetType::Separator:
            command.primitive = UiDrawPrimitive::Separator;
            command.rect.height = 1.0F;
            row_y += 24.0F;
            break;
        case UiWidgetType::Label:
            command.primitive = UiDrawPrimitive::Text;
            command.color = {0.62F, 0.68F, 0.76F, 1.0F};
            row_y += 34.0F;
            break;
        }
        frame_.commands.push_back(std::move(command));
    }
    ++frame_.revision;
    frame_dirty_ = false;
}

} // namespace genomes::ui
