#pragma once

#include <genomes/ui/ModApi.h>
#include <genomes/ui/UiContentRegistry.hpp>

#include <string>
#include <memory>
#include <vector>

namespace genomes::ui {

struct UiPluginError final { std::string message; };

class UiNativePluginManager final {
public:
    UiNativePluginManager() = default;
    ~UiNativePluginManager();
    UiNativePluginManager(const UiNativePluginManager&) = delete;
    UiNativePluginManager& operator=(const UiNativePluginManager&) = delete;

    bool load(const UiContentRegistry& registry, bool allow_native_plugins = false,
              UiPluginError* error = nullptr);
    void unload() noexcept;
    [[nodiscard]] const std::vector<std::string>& registered_scene_controllers() const noexcept { return scenes_; }
    [[nodiscard]] const std::vector<std::string>& registered_ui_actions() const noexcept { return actions_; }
    [[nodiscard]] const std::vector<std::string>& callback_trace() const noexcept { return callback_trace_; }

private:
    struct LoadedPlugin final {
        void* handle{nullptr};
        void (*unload)(){nullptr};
        std::unique_ptr<GenomesModHostApi> host;
    };
    std::vector<LoadedPlugin> plugins_;
    std::vector<std::string> scenes_;
    std::vector<std::string> actions_;
    std::vector<std::string> callback_trace_;
};

} // namespace genomes::ui
