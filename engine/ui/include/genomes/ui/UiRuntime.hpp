#pragma once

#include <genomes/ui/UiTypes.hpp>
#include <genomes/input/InputFrame.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace genomes::ui {

class UiRuntime;

using UiValue = std::variant<bool, std::int64_t, double, std::string>;

struct UiRecord final {
    std::unordered_map<std::string, UiValue> fields;
};

class UiDataModel final {
public:
    void set(std::string key, UiValue value) {
        values_[std::move(key)] = std::move(value);
        ++revision_;
    }

    void set_list(std::string key, std::vector<UiRecord> value) {
        lists_[std::move(key)] = std::move(value);
        ++revision_;
    }

    [[nodiscard]] const UiValue* find(std::string_view key) const noexcept {
        const auto it = values_.find(std::string{key});
        return it == values_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] const std::vector<UiRecord>* find_list(std::string_view key) const noexcept {
        const auto it = lists_.find(std::string{key});
        return it == lists_.end() ? nullptr : &it->second;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    void clear() noexcept { values_.clear(); lists_.clear(); ++revision_; }

private:
    std::unordered_map<std::string, UiValue> values_;
    std::unordered_map<std::string, std::vector<UiRecord>> lists_;
    std::uint64_t revision_{0};
};

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

// Renderer-neutral UI owner. The legacy widget ingestion below is deliberately
// kept as a small extraction bridge while screens move to RML controllers. No
// renderer or gameplay code receives a widget list; renderers consume frame().
class UiRuntime final {
public:
    UiRuntime() = default;

    void clear();
    UiWidget& add(UiWidget widget);
    void set_viewport(std::uint32_t width, std::uint32_t height) noexcept;
    void update(double delta_seconds);
    // Returns true only when a registered UI action consumed the event. This
    // is the boundary used by SceneDirector to keep UI input out of gameplay.
    bool process_input(const input::InputFrame& input);

    void set_action_router(IUiActionRouter* router) noexcept { action_router_ = router; }
    using ControllerFactory = std::function<std::unique_ptr<IUiScreenController>()>;
    void register_controller(std::string id, ControllerFactory factory);
    [[nodiscard]] bool activate_controller(std::string_view id);
    [[nodiscard]] IUiScreenController* active_controller() noexcept { return controller_.get(); }
    [[nodiscard]] UiActionResult dispatch(UiActionId action,
                                          const UiActionArguments& arguments = {}) const;

    [[nodiscard]] UiRouteStack& routes() noexcept { return routes_; }
    [[nodiscard]] const UiRouteStack& routes() const noexcept { return routes_; }
    [[nodiscard]] UiDataModel& model() noexcept { return model_; }
    [[nodiscard]] const UiRenderFrame& frame() const noexcept;
    void replace_frame(const UiRenderFrame& frame) { frame_ = frame; frame_dirty_ = false; }

private:
    void rebuild_frame() const;

    std::vector<UiWidget> widgets_;
    UiRouteStack routes_;
    UiDataModel model_;
    IUiActionRouter* action_router_{nullptr};
    mutable UiRenderFrame frame_;
    mutable bool frame_dirty_{true};
    double elapsed_seconds_{0.0};
    std::unordered_map<std::string, ControllerFactory> controller_factories_;
    std::unique_ptr<IUiScreenController> controller_;
    std::string controller_id_;
};

} // namespace genomes::ui
