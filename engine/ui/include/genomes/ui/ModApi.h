#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  define GENOMES_MOD_EXPORT __declspec(dllexport)
#else
#  define GENOMES_MOD_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GENOMES_MOD_API_VERSION 1u

typedef struct GenomesModString {
    const char* data;
    uint32_t size;
} GenomesModString;

typedef struct GenomesModHostApi {
    uint32_t size;
    uint32_t api_version;
    void* user_data;
    void (*log)(uint32_t level, GenomesModString message);
    int (*register_scene_controller)(GenomesModString id, void* user_data);
    int (*register_ui_action)(GenomesModString id, void* user_data);
} GenomesModHostApi;

GENOMES_MOD_EXPORT uint32_t genomes_mod_api_version(void);
GENOMES_MOD_EXPORT int genomes_mod_load(const GenomesModHostApi* host);
GENOMES_MOD_EXPORT void genomes_mod_unload(void);

#ifdef __cplusplus
}
#endif
