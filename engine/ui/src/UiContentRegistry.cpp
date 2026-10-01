#include <genomes/ui/UiContentRegistry.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace genomes::ui {

namespace {

using Json = nlohmann::json;

[[nodiscard]] UiContentRegistry::Result fail(std::string message) {
    return UiContentRegistry::Result::failure({std::move(message)});
}

[[nodiscard]] bool required_string(const Json& value, const char* key) {
    return value.contains(key) && value.at(key).is_string() && !value.at(key).get<std::string>().empty();
}

[[nodiscard]] std::vector<std::string> strings(const Json& value, const char* key) {
    std::vector<std::string> result;
    if (!value.contains(key)) {
        return result;
    }
    if (!value.at(key).is_array()) {
        throw std::runtime_error(std::string{"field must be an array: "} + key);
    }
    for (const auto& item : value.at(key)) {
        if (!item.is_string() || item.get<std::string>().empty()) {
            throw std::runtime_error(std::string{"array contains a non-string: "} + key);
        }
        result.push_back(item.get<std::string>());
    }
    return result;
}

[[nodiscard]] bool traversal_free(const std::filesystem::path& path) noexcept {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto& component : path) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

} // namespace

UiContentRegistry::Result UiContentRegistry::discover(const std::filesystem::path& mods_root) {
    if (!std::filesystem::is_directory(mods_root)) {
        return fail("mods root is not a directory");
    }

    UiContentRegistry registry;
    std::set<std::string> mod_ids;
    std::set<std::string> scene_ids;
    try {
        for (const auto& entry : std::filesystem::directory_iterator{mods_root}) {
            if (!entry.is_directory()) {
                continue;
            }
            const auto manifest_path = entry.path() / "mod.json";
            if (!std::filesystem::is_regular_file(manifest_path)) {
                return fail("mod is missing mod.json: " + entry.path().string());
            }
            std::ifstream stream{manifest_path};
            const Json json = Json::parse(stream);
            if (!json.contains("schema_version") || json.at("schema_version") != 1 ||
                !required_string(json, "id") || !required_string(json, "version")) {
                return fail("invalid mod manifest: " + manifest_path.string());
            }

            UiModManifest mod{};
            mod.id = json.at("id").get<std::string>();
            mod.version = json.at("version").get<std::string>();
            mod.load_priority = json.value("load_priority", 0);
            mod.dependencies = strings(json, "dependencies");
            if (json.contains("native_plugin")) {
                if (!json.at("native_plugin").is_string() || !json.contains("trusted_native") ||
                    !json.at("trusted_native").is_boolean()) {
                    return fail("native_plugin must be a string: " + mod.id);
                }
                mod.native_plugin = json.at("native_plugin").get<std::string>();
                mod.trusted_native = json.at("trusted_native").get<bool>();
            }
            mod.root = entry.path();
            if (!mod_ids.insert(mod.id).second) {
                return fail("duplicate mod id: " + mod.id);
            }
            if (json.contains("scenes")) {
                if (!json.at("scenes").is_array()) {
                    return fail("scenes must be an array: " + mod.id);
                }
                for (const auto& scene_ref : json.at("scenes")) {
                    if (!scene_ref.is_string()) {
                        return fail("scene reference must be a string: " + mod.id);
                    }
                    const auto scene_root = mod.root / scene_ref.get<std::string>();
                    const auto scene_manifest_path = scene_root / "scene.json";
                    std::ifstream scene_stream{scene_manifest_path};
                    const Json scene_json = Json::parse(scene_stream);
                    if (!scene_json.contains("schema_version") || scene_json.at("schema_version") != 1 ||
                        !required_string(scene_json, "id") || !required_string(scene_json, "controller") ||
                        !required_string(scene_json, "document")) {
                        return fail("invalid scene manifest: " + scene_manifest_path.string());
                    }
                    UiSceneManifest scene{};
                    scene.id = scene_json.at("id").get<std::string>();
                    scene.controller = scene_json.at("controller").get<std::string>();
                    scene.document = scene_json.at("document").get<std::string>();
                    scene.stylesheets = strings(scene_json, "stylesheets");
                    scene.overlays = strings(scene_json, "overlays");
                    scene.action_namespace = scene_json.value("action_namespace", scene.id);
                    scene.mod_id = mod.id;
                    scene.root = scene_root;
                    if (!scene_ids.insert(scene.id).second) {
                        return fail("duplicate scene id: " + scene.id);
                    }
                    if (!traversal_free(scene_root.lexically_relative(mod.root)) ||
                        !traversal_free(scene.document)) {
                        return fail("scene path escapes mod root: " + scene.id);
                    }
                    for (const auto& stylesheet : scene.stylesheets) {
                        if (!traversal_free(stylesheet) ||
                            std::filesystem::path{stylesheet}.extension() != ".rcss") {
                            return fail("invalid scene stylesheet path: " + scene.id);
                        }
                    }
                    for (const auto& overlay : scene.overlays) {
                        if (!traversal_free(overlay)) return fail("invalid scene overlay path: " + scene.id);
                    }
                    mod.scenes.push_back(std::move(scene));
                }
            }
            registry.mods_.push_back(std::move(mod));
        }

        std::unordered_map<std::string, std::size_t> by_id;
        for (std::size_t i = 0; i < registry.mods_.size(); ++i) {
            by_id.emplace(registry.mods_[i].id, i);
        }
        std::set<std::string> all_scene_ids;
        for (const auto& mod : registry.mods_) {
            for (const auto& scene : mod.scenes) all_scene_ids.insert(scene.id);
        }
        for (const auto& mod : registry.mods_) {
            for (const auto& scene : mod.scenes) {
                for (const auto& overlay : scene.overlays) {
                    if (!all_scene_ids.contains(overlay)) {
                        return fail("scene overlay does not resolve: " + scene.id + " -> " + overlay);
                    }
                }
            }
        }
        for (const auto& mod : registry.mods_) {
            for (const auto& dependency : mod.dependencies) {
                if (!by_id.contains(dependency)) {
                    return fail("missing mod dependency: " + mod.id + " -> " + dependency);
                }
            }
        }
        std::vector<std::uint32_t> indegree(registry.mods_.size(), 0U);
        std::vector<std::vector<std::size_t>> dependents(registry.mods_.size());
        for (std::size_t index = 0; index < registry.mods_.size(); ++index) {
            for (const auto& dependency : registry.mods_[index].dependencies) {
                ++indegree[index];
                dependents[by_id.at(dependency)].push_back(index);
            }
        }
        const auto ready_less = [&registry](std::size_t left, std::size_t right) {
            const auto& left_mod = registry.mods_[left];
            const auto& right_mod = registry.mods_[right];
            return left_mod.load_priority != right_mod.load_priority ?
                       left_mod.load_priority < right_mod.load_priority : left_mod.id < right_mod.id;
        };
        std::vector<std::size_t> ready;
        for (std::size_t index = 0; index < indegree.size(); ++index) {
            if (indegree[index] == 0U) ready.push_back(index);
        }
        std::vector<UiModManifest> ordered;
        ordered.reserve(registry.mods_.size());
        while (!ready.empty()) {
            std::sort(ready.begin(), ready.end(), ready_less);
            const std::size_t index = ready.front();
            ready.erase(ready.begin());
            ordered.push_back(std::move(registry.mods_[index]));
            for (const std::size_t dependent : dependents[index]) {
                if (--indegree[dependent] == 0U) ready.push_back(dependent);
            }
        }
        if (ordered.size() != registry.mods_.size()) return fail("cyclic mod dependency");
        registry.mods_ = std::move(ordered);
    } catch (const std::exception& error) {
        return fail(error.what());
    }
    return UiContentRegistry::Result::success(std::move(registry));
}

const UiSceneManifest* UiContentRegistry::find_scene(const std::string& id) const noexcept {
    for (const auto& mod : mods_) {
        for (const auto& scene : mod.scenes) {
            if (scene.id == id) return &scene;
        }
    }
    return nullptr;
}

const UiSceneManifest* UiContentRegistry::find_scene(foundation::SceneId id) const noexcept {
    for (const auto& mod : mods_) {
        for (const auto& scene : mod.scenes) {
            if (foundation::scene_id(scene.id) == id) return &scene;
        }
    }
    return nullptr;
}

bool UiContentRegistry::is_allowed_path(const UiSceneManifest& scene,
                                        const std::filesystem::path& relative) const noexcept {
    if (!traversal_free(relative)) return false;
    const auto absolute = (scene.root / relative).lexically_normal();
    const auto root = scene.root.lexically_normal();
    const auto relative_to_root = absolute.lexically_relative(root);
    if (!traversal_free(relative_to_root)) return false;
    const auto extension = absolute.extension().string();
    return extension == ".rml" || extension == ".rcss" || extension == ".json" ||
           extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".ttf" || extension == ".otf";
}

} // namespace genomes::ui
