#include <genomes/ui/UiNativePluginManager.hpp>

#include <filesystem>
#include <utility>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace genomes::ui {

namespace {
void set_error(UiPluginError* error, std::string message) {
    if (error != nullptr) error->message = std::move(message);
}

int register_id(GenomesModString id, std::vector<std::string>& target) {
    if (id.data == nullptr || id.size == 0 || id.size > 4096) return 0;
    target.emplace_back(id.data, id.size);
    return 1;
}
}

UiNativePluginManager::~UiNativePluginManager() { unload(); }

bool UiNativePluginManager::load(const UiContentRegistry& registry, bool allow_native_plugins,
                                 UiPluginError* error) {
    unload();
    for (const auto& mod : registry.mods()) {
        if (mod.native_plugin.empty()) continue;
        if (!allow_native_plugins || !mod.trusted_native) {
            set_error(error, "native plugin requires explicit trust and permission: " + mod.id);
            unload();
            return false;
        }
        const auto relative = std::filesystem::path{mod.native_plugin};
        if (!relative.is_relative() || relative.lexically_normal().string().starts_with("..")) {
            set_error(error, "native plugin escapes mod root: " + mod.id);
            unload();
            return false;
        }
        const auto path = (mod.root / relative).lexically_normal();
        if (!std::filesystem::is_regular_file(path)) {
            set_error(error, "native plugin is missing: " + path.string());
            unload();
            return false;
        }
#if defined(_WIN32)
        auto handle = LoadLibraryW(path.wstring().c_str());
        if (handle == nullptr) { set_error(error, "cannot load native plugin: " + path.string()); unload(); return false; }
        auto version = reinterpret_cast<uint32_t (*)()>(GetProcAddress(handle, "genomes_mod_api_version"));
        auto load_fn = reinterpret_cast<int (*)(const GenomesModHostApi*)>(GetProcAddress(handle, "genomes_mod_load"));
        auto unload_fn = reinterpret_cast<void (*)()>(GetProcAddress(handle, "genomes_mod_unload"));
#else
        auto handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) { set_error(error, "cannot load native plugin: " + path.string()); unload(); return false; }
        auto version = reinterpret_cast<uint32_t (*)()>(dlsym(handle, "genomes_mod_api_version"));
        auto load_fn = reinterpret_cast<int (*)(const GenomesModHostApi*)>(dlsym(handle, "genomes_mod_load"));
        auto unload_fn = reinterpret_cast<void (*)()>(dlsym(handle, "genomes_mod_unload"));
#endif
        bool compatible = false;
        try {
            compatible = version != nullptr && load_fn != nullptr && unload_fn != nullptr &&
                         version() == GENOMES_MOD_API_VERSION;
        } catch (...) {
            compatible = false;
        }
        if (!compatible) {
#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            set_error(error, "incompatible native plugin API: " + mod.id);
            unload();
            return false;
        }
        auto host = std::make_unique<GenomesModHostApi>(GenomesModHostApi{
            sizeof(GenomesModHostApi), GENOMES_MOD_API_VERSION, this, nullptr,
            [](GenomesModString id, void* data) {
                auto* manager = static_cast<UiNativePluginManager*>(data);
                try { return manager == nullptr ? 0 : register_id(id, manager->scenes_); }
                catch (...) { return 0; }
            },
            [](GenomesModString id, void* data) {
                auto* manager = static_cast<UiNativePluginManager*>(data);
                try { return manager == nullptr ? 0 : register_id(id, manager->actions_); }
                catch (...) { return 0; }
            }});
        bool accepted = false;
        try {
            accepted = load_fn(host.get()) != 0;
        } catch (...) {
            accepted = false;
        }
        if (!accepted) {
#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            set_error(error, "native plugin rejected host API: " + mod.id);
            unload();
            return false;
        }
        plugins_.push_back({reinterpret_cast<void*>(handle), unload_fn, std::move(host)});
    }
    return true;
}

void UiNativePluginManager::unload() noexcept {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        if (it->unload != nullptr) {
            try { it->unload(); } catch (...) {}
        }
#if defined(_WIN32)
        if (it->handle != nullptr) FreeLibrary(static_cast<HMODULE>(it->handle));
#else
        if (it->handle != nullptr) dlclose(it->handle);
#endif
    }
    plugins_.clear();
    scenes_.clear();
    actions_.clear();
}

} // namespace genomes::ui
