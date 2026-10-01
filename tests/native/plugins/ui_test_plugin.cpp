#include <genomes/ui/ModApi.h>

#include <cstdint>
#include <cstring>

namespace {

const GenomesModHostApi* host_api = nullptr;

GenomesModString literal(const char* value) {
    return {value, static_cast<std::uint32_t>(std::strlen(value))};
}

} // namespace

extern "C" GENOMES_MOD_EXPORT std::uint32_t genomes_mod_api_version(void) {
    return GENOMES_MOD_API_VERSION;
}

extern "C" GENOMES_MOD_EXPORT int genomes_mod_load(const GenomesModHostApi* host) {
    if (host == nullptr || host->size < sizeof(GenomesModHostApi) ||
        host->api_version != GENOMES_MOD_API_VERSION ||
        host->register_scene_controller == nullptr || host->register_ui_action == nullptr) {
        return 0;
    }
    host_api = host;
    if (host_api->register_scene_controller(literal("test.plugin.scene"), host_api->user_data) == 0 ||
        host_api->register_ui_action(literal("test.plugin.action"), host_api->user_data) == 0) {
        host_api = nullptr;
        return 0;
    }
    return 1;
}

extern "C" GENOMES_MOD_EXPORT void genomes_mod_unload(void) {
    host_api = nullptr;
}
