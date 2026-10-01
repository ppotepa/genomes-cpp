#pragma once

#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace genomes::ui {

struct UiSceneManifest final {
    std::string id;
    std::string controller;
    std::string document;
    std::vector<std::string> stylesheets;
    std::vector<std::string> overlays;
    std::string action_namespace;
    std::string mod_id;
    std::filesystem::path root;
};

struct UiModManifest final {
    std::string id;
    std::string version;
    int load_priority{0};
    std::vector<std::string> dependencies;
    std::string native_plugin;
    bool trusted_native{false};
    std::vector<UiSceneManifest> scenes;
    std::filesystem::path root;
};

struct UiContentError final {
    std::string message;
    std::filesystem::path path;
    std::string field;
    std::string source{"ui-content"};
};

class UiContentRegistry final {
public:
    using Result = foundation::Result<UiContentRegistry, UiContentError>;

    static Result discover(const std::filesystem::path& mods_root);
    [[nodiscard]] static std::optional<std::filesystem::path> resolve_path(
        const std::filesystem::path& root, const std::filesystem::path& relative) noexcept;

    [[nodiscard]] const std::vector<UiModManifest>& mods() const noexcept { return mods_; }
    [[nodiscard]] const UiSceneManifest* find_scene(const std::string& id) const noexcept;
    [[nodiscard]] const UiSceneManifest* find_scene(foundation::SceneId id) const noexcept;
    [[nodiscard]] bool is_allowed_path(const UiSceneManifest& scene,
                                       const std::filesystem::path& relative) const noexcept;

private:
    std::vector<UiModManifest> mods_;
};

} // namespace genomes::ui
