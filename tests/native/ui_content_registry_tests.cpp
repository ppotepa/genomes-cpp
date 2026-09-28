#include <genomes/ui/UiContentRegistry.hpp>
#include <genomes/ui/UiNativePluginManager.hpp>

#include <cassert>
#include <filesystem>

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
    genomes::ui::UiNativePluginManager plugins;
    genomes::ui::UiPluginError plugin_error;
    assert(plugins.load(registry, &plugin_error));
    assert(plugins.registered_scene_controllers().empty());
    assert(plugins.registered_ui_actions().empty());
    return 0;
}
