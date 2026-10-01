#pragma once

#include <genomes/ui/UiDataModel.hpp>
#include <genomes/ui/UiTypes.hpp>
#include <genomes/input/InputFrame.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::ui {

class UiRuntime;

using UiValue = UiScalar;
struct UiRecord final { UiTableRow fields; };

using UiActionId = foundation::StableId;
using UiActionArguments = std::vector<std::pair<std::string, std::string>>;

enum class UiActionResult : std::uint8_t {
    Handled,
    Rejected,
    Unknown
};

class IUiActionRouter {
public:
    virtual ~IUiActionRouter() = default;
    [[nodiscard]] virtual UiActionResult dispatch(
        UiActionId action, const UiActionArguments& arguments) = 0;
};

struct UiContext final {
    UiRuntime* runtime{nullptr};
    UiDataModel* model{nullptr};
};

class IUiScreenController {
public:
    virtual ~IUiScreenController() = default;
    virtual void bind(UiDataModel&) {}
    virtual void on_enter(UiContext&) {}
    virtual void on_exit(UiContext&) {}
    virtual void update(UiContext&, double) {}
    virtual UiActionResult handle_action(UiActionId, const UiActionArguments&) {
        return UiActionResult::Unknown;
    }
};

struct UiRoute final {
    foundation::SceneId scene{0};
    std::string document;
    std::string controller;
    std::string action_namespace;
    bool overlay{false};
    std::uint64_t revision{0};
};

class UiRouteStack final {
public:
    void replace(UiRoute route);
    void push(UiRoute route);
    bool pop();
    void clear() noexcept { routes_.clear(); ++revision_; }

    [[nodiscard]] const std::vector<UiRoute>& routes() const noexcept { return routes_; }
    [[nodiscard]] const UiRoute* top() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

private:
    std::vector<UiRoute> routes_;
    std::uint64_t revision_{0};
};

// Renderer-neutral UI owner. Rendering consumes only the immutable RmlUi frame;
// scene code publishes values and commands through the data model.
class UiRuntime final {
public:
    UiRuntime();

    void clear();
    void reset_model() {
        model_.clear();
        (void)model_.set("ui_scale", 1.0);
        (void)model_.set("show_diagnostics", true);
    }
    void set_viewport(std::uint32_t width, std::uint32_t height) noexcept;
    void update(double delta_seconds);
    // Returns true only when a registered UI action consumed the event. This
    // is the boundary used by SceneDirector to keep UI input out of gameplay.
    bool process_input(const input::InputFrame& input);

    void set_action_router(IUiActionRouter* router) noexcept { action_router_ = router; }
    using ControllerFactory = std::function<std::unique_ptr<IUiScreenController>()>;
    void register_controller(std::string id, ControllerFactory factory);
    [[nodiscard]] bool activate_controller(std::string_view id);
    [[nodiscard]] IUiScreenController* active_controller() noexcept {
        return route_controllers_.empty() ? controller_.get() : route_controllers_.back().controller.get();
    }
    [[nodiscard]] UiActionResult dispatch(UiActionId action,
                                          const UiActionArguments& arguments = {}) const;

    [[nodiscard]] UiRouteStack& routes() noexcept { return routes_; }
    [[nodiscard]] const UiRouteStack& routes() const noexcept { return routes_; }
    [[nodiscard]] UiDataModel& model() noexcept { return model_; }
    [[nodiscard]] const UiDataModel& model() const noexcept { return model_; }
    [[nodiscard]] const UiRenderFrame& frame() const noexcept;
    void replace_frame(const UiRenderFrame& frame) { frame_ = frame; frame_dirty_ = false; }

    // Route controllers are synchronized at the frame boundary. The callback
    // fills manifest-owned document/controller/action fields before a route is
    // mounted, keeping SceneDirector independent of mod layout.
    using RouteResolver = std::function<void(UiRoute&)>;
    void set_route_resolver(RouteResolver resolver) { route_resolver_ = std::move(resolver); }
    void sync_route_lifecycle();
    struct RouteControllerState final {
        UiRoute route;
        UiDataModel model;
        std::unique_ptr<IUiScreenController> controller;
    };
    [[nodiscard]] const std::vector<RouteControllerState>& route_controllers() const noexcept {
        return route_controllers_;
    }

private:
    void rebuild_frame() const;

    UiRouteStack routes_;
    UiDataModel model_;
    IUiActionRouter* action_router_{nullptr};
    mutable UiRenderFrame frame_;
    mutable bool frame_dirty_{true};
    double elapsed_seconds_{0.0};
    std::unordered_map<std::string, ControllerFactory> controller_factories_;
    std::unique_ptr<IUiScreenController> controller_;
    std::string controller_id_;
    RouteResolver route_resolver_{};
    std::uint64_t synchronized_route_revision_{0};
    std::vector<RouteControllerState> route_controllers_;
};

} // namespace genomes::ui
