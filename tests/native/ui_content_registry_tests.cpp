#include <genomes/ui/UiContentRegistry.hpp>
#include <genomes/ui/UiNativePluginManager.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output);
    output << text;
    assert(output.good());
}

void writeMod(const std::filesystem::path& root, const std::string& id, int priority,
              const std::vector<std::string>& dependencies) {
    const auto mod_root = root / id;
    const auto scene_root = mod_root / "scenes" / id;
    std::filesystem::create_directories(scene_root);
    std::string dependency_json = "[";
    for (std::size_t index = 0U; index < dependencies.size(); ++index) {
        if (index != 0U) dependency_json += ",";
        dependency_json += "\"" + dependencies[index] + "\"";
    }
    dependency_json += "]";
    writeText(mod_root / "mod.json",
              "{\"schema_version\":1,\"id\":\"" + id +
                  "\",\"version\":\"1.0.0\",\"load_priority\":" +
                  std::to_string(priority) + ",\"dependencies\":" + dependency_json +
                  ",\"scenes\":[\"scenes/" + id + "\"]}");
    writeText(scene_root / "scene.json",
              "{\"schema_version\":1,\"id\":\"scene." + id +
                  "\",\"controller\":\"controller." + id +
                  "\",\"document\":\"screen.rml\"}");
    writeText(scene_root / "screen.rml", "<rml><body></body></rml>");
}

void writeNativeManifest(const std::filesystem::path& root, const std::string& id, int priority,
                         const std::string& plugin_name, bool trusted,
                         const std::vector<std::string>& dependencies = {}) {
    std::string dependency_json = "[";
    for (std::size_t index = 0U; index < dependencies.size(); ++index) {
        if (index != 0U) dependency_json += ",";
        dependency_json += "\"" + dependencies[index] + "\"";
    }
    dependency_json += "]";
    writeText(root / id / "mod.json",
              "{\"schema_version\":1,\"id\":\"" + id +
                  "\",\"version\":\"1.0.0\",\"load_priority\":" +
                  std::to_string(priority) + ",\"dependencies\":" + dependency_json +
                  ",\"native_plugin\":\"" + plugin_name +
                  "\",\"trusted_native\":" + (trusted ? "true" : "false") +
                  ",\"scenes\":[\"scenes/" + id + "\"]}");
}

void registryOrdersDependenciesAndPriorities() {
    const auto root = std::filesystem::temp_directory_path() /
                      "genomes-ui-content-registry-contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);
    writeMod(root, "base", 20, {});
    writeMod(root, "side", -10, {});
    writeMod(root, "addon", -100, {"base"});

    const auto result = genomes::ui::UiContentRegistry::discover(root);
    assert(result);
    const auto& mods = result.value().mods();
    assert(mods.size() == 3U);
    // Dependency order wins over priority: addon cannot precede base even
    // though addon has the numerically lowest priority.
    assert(mods[0].id == "side");
    assert(mods[1].id == "base");
    assert(mods[2].id == "addon");
    assert(result.value().find_scene("scene.addon") != nullptr);
    std::filesystem::remove_all(root, cleanup_error);
}

void duplicateCandidateDoesNotReplacePublishedRegistry() {
    const auto root = std::filesystem::temp_directory_path() /
                      "genomes-ui-content-registry-duplicate";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);
    writeMod(root, "stable", 0, {});
    const auto published = genomes::ui::UiContentRegistry::discover(root);
    assert(published);
    assert(published.value().mods().size() == 1U);

    writeMod(root, "duplicate", 1, {});
    // Make the second candidate collide with the already-known scene ID.
    writeText(root / "duplicate" / "scenes" / "duplicate" / "scene.json",
              "{\"schema_version\":1,\"id\":\"scene.stable\",\"controller\":\"x\",\"document\":\"screen.rml\"}");
    const auto rejected = genomes::ui::UiContentRegistry::discover(root);
    assert(!rejected);
    assert(published.value().mods().size() == 1U);
    assert(published.value().find_scene("scene.stable") != nullptr);
    assert(published.value().find_scene("scene.duplicate") == nullptr);
    std::filesystem::remove_all(root, cleanup_error);
}

void nativePluginRequiresTrustedManifest() {
    const auto root = std::filesystem::temp_directory_path() /
                      "genomes-ui-content-native-permission";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);
    writeMod(root, "untrusted", 0, {});
    writeText(root / "untrusted" / "mod.json",
              "{\"schema_version\":1,\"id\":\"untrusted\",\"version\":\"1.0.0\","
              "\"load_priority\":0,\"dependencies\":[],"
              "\"native_plugin\":\"missing-plugin.dll\",\"trusted_native\":false,"
              "\"scenes\":[\"scenes/untrusted\"]}");
    const auto registry = genomes::ui::UiContentRegistry::discover(root);
    assert(registry);
    genomes::ui::UiNativePluginManager plugins;
    genomes::ui::UiPluginError error;
    assert(!plugins.load(registry.value(), true, &error));
    assert(!error.message.empty());
    assert(plugins.registered_scene_controllers().empty());
    assert(plugins.registered_ui_actions().empty());
    std::filesystem::remove_all(root, cleanup_error);
}

void nativePluginFixtureExercisesLoadAndRollback() {
    const auto plugin_source_a = std::filesystem::path{GENOMES_TEST_NATIVE_PLUGIN_A_PATH};
    const auto plugin_source_b = std::filesystem::path{GENOMES_TEST_NATIVE_PLUGIN_B_PATH};
    assert(std::filesystem::is_regular_file(plugin_source_a));
    assert(std::filesystem::is_regular_file(plugin_source_b));
    const auto root = std::filesystem::temp_directory_path() /
                      "genomes-ui-content-native-fixture";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);
    writeMod(root, "good", 0, {});
    const auto plugin_name_a = plugin_source_a.filename().string();
    std::filesystem::copy_file(plugin_source_a, root / "good" / plugin_name_a,
                               std::filesystem::copy_options::overwrite_existing);
    writeNativeManifest(root, "good", 0, plugin_name_a, true);
    writeMod(root, "bad", 1, {});
    writeNativeManifest(root, "bad", 1, "missing-plugin.dll", true);

    const auto candidate = genomes::ui::UiContentRegistry::discover(root);
    assert(candidate);
    genomes::ui::UiNativePluginManager plugins;
    genomes::ui::UiPluginError error;
    // The good plugin is loaded first, then the missing second plugin fails;
    // the manager must unload the partial candidate and clear registrations.
    assert(!plugins.load(candidate.value(), true, &error));
    assert(!error.message.empty());
    assert(plugins.registered_scene_controllers().empty());
    assert(plugins.registered_ui_actions().empty());

    std::filesystem::remove_all(root / "bad", cleanup_error);
    writeMod(root, "dependent", 1, {"good"});
    const auto plugin_name_b = plugin_source_b.filename().string();
    std::filesystem::copy_file(plugin_source_b, root / "dependent" / plugin_name_b,
                               std::filesystem::copy_options::overwrite_existing);
    writeNativeManifest(root, "dependent", 1, plugin_name_b, true, {"good"});
    const auto successful = genomes::ui::UiContentRegistry::discover(root);
    assert(successful);
    assert(plugins.load(successful.value(), true, &error));
    assert(plugins.registered_scene_controllers().size() == 2U);
    assert(plugins.registered_ui_actions().size() == 2U);
    assert(plugins.registered_scene_controllers()[0] == "test.plugin.a.scene");
    assert(plugins.registered_scene_controllers()[1] == "test.plugin.b.scene");
    plugins.unload();
    // Unload callbacks retain a valid host context and run dependent first.
    assert(plugins.callback_trace().size() == 6U);
    assert(plugins.callback_trace()[4] == "test.plugin.b.unload");
    assert(plugins.callback_trace()[5] == "test.plugin.a.unload");
    assert(plugins.registered_scene_controllers().empty());
    assert(plugins.registered_ui_actions().empty());
    std::filesystem::remove_all(root, cleanup_error);
}

} // namespace

int main() {
    const auto result = genomes::ui::UiContentRegistry::discover(
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods");
    assert(result);
    const auto& registry = result.value();
    assert(registry.mods().size() == 1);
    for (const auto& mod : registry.mods()) {
        for (const auto& manifest : mod.scenes) {
            assert(std::filesystem::is_regular_file(manifest.root / manifest.document));
            for (const auto& stylesheet : manifest.stylesheets)
                assert(std::filesystem::is_regular_file(manifest.root / stylesheet));
            for (const auto& overlay : manifest.overlays)
                assert(registry.find_scene(overlay) != nullptr);
        }
    }
    const auto* scene = registry.find_scene("scene.main-menu");
    assert(scene != nullptr);
    assert(registry.is_allowed_path(*scene, "screen.rml"));
    assert(registry.is_allowed_path(*scene, "screen.rcss"));
    assert(!registry.is_allowed_path(*scene, "screen.lua"));
    assert(!registry.is_allowed_path(*scene, "../mod.json"));
    assert(!registry.is_allowed_path(*scene, std::filesystem::path{"C:/outside.rml"}));
    assert(registry.find_scene("scene.pause") != nullptr);
    const auto* mass_battle = registry.find_scene("scene.infantry-mass-battle");
    assert(mass_battle != nullptr);
    assert(mass_battle->controller == "builtin.battlefield");
    assert(mass_battle->document == "screen.rml");
    assert(mass_battle->stylesheets == std::vector<std::string>{"screen.rcss"});
    assert(mass_battle->overlays == std::vector<std::string>{"scene.pause"});
    assert(std::filesystem::is_regular_file(mass_battle->root / mass_battle->document));
    assert(std::filesystem::is_regular_file(mass_battle->root /
                                            mass_battle->stylesheets.front()));
    genomes::ui::UiNativePluginManager plugins;
    genomes::ui::UiPluginError plugin_error;
    assert(plugins.load(registry, false, &plugin_error));
    assert(plugins.registered_scene_controllers().empty());
    assert(plugins.registered_ui_actions().empty());
    registryOrdersDependenciesAndPriorities();
    duplicateCandidateDoesNotReplacePublishedRegistry();
    nativePluginRequiresTrustedManifest();
    nativePluginFixtureExercisesLoadAndRollback();
    return 0;
}
