if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(profile "${GENOMES_SOURCE_DIR}/mods/core/profiles/world-generation.json")
set(header "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/WorldGenerationProfile.hpp")
set(source "${GENOMES_SOURCE_DIR}/engine/world/src/WorldGenerationProfile.cpp")
set(plan "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/WorldPlan.hpp")
set(city "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/CityPlan.hpp")
set(world_config "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/WorldConfig.hpp")
set(scene_catalog "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/BuiltinScenes.hpp")
set(building_site "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world_core/BuildingSite.hpp")
set(hydrology_spec "${GENOMES_SOURCE_DIR}/modules/hydrology/include/genomes/hydrology/HydrologySpec.hpp")

foreach(path IN LISTS profile header source plan city world_config scene_catalog building_site hydrology_spec)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing world generation profile source: ${path}")
    endif()
endforeach()

file(READ "${building_site}" building_site_text)
foreach(forbidden IN ITEMS
        "preferred_footprint{12.0F, 1.0F, 10.0F}"
        "floors_min{1}"
        "floors_max{3}"
        "access_width{1.2F}"
        "clearance_m{1.0F}")
    string(FIND "${building_site_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "building site request retains a native default: ${forbidden}")
    endif()
endforeach()

file(READ "${hydrology_spec}" hydrology_text)
foreach(forbidden IN ITEMS
        "seed{0x5EED2026"
        "map_size_m{600}"
        "river_probability{0.35F}")
    string(FIND "${hydrology_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "hydrology request retains a native world default: ${forbidden}")
    endif()
endforeach()

file(READ "${profile}" profile_text)
string(JSON schema ERROR_VARIABLE schema_error GET "${profile_text}" schema_version)
string(JSON id ERROR_VARIABLE id_error GET "${profile_text}" id)
string(JSON seed ERROR_VARIABLE seed_error GET "${profile_text}" default_seed)
string(JSON map_size ERROR_VARIABLE map_error GET "${profile_text}" map_size_m)
string(JSON vegetation ERROR_VARIABLE vegetation_error GET "${profile_text}" vegetation)
string(JSON buildings ERROR_VARIABLE buildings_error GET "${profile_text}" buildings)
string(JSON fenced ERROR_VARIABLE fenced_error GET "${profile_text}" fenced_parcels)
string(JSON hydrology ERROR_VARIABLE hydrology_error GET "${profile_text}" hydrology_mode)
string(JSON river ERROR_VARIABLE river_error GET "${profile_text}" river_probability)
if(schema_error OR id_error OR seed_error OR map_error OR vegetation_error OR
   buildings_error OR fenced_error OR hydrology_error OR river_error)
    message(FATAL_ERROR "world generation profile is missing a required field")
endif()
if(NOT schema EQUAL 1 OR NOT id STREQUAL "world-generation-default" OR
   NOT seed EQUAL 1592598566 OR NOT map_size EQUAL 600 OR
   NOT hydrology STREQUAL "seeded-optional")
    message(FATAL_ERROR "world generation profile core identity/defaults changed")
endif()

file(READ "${source}" source_text)
foreach(required IN ITEMS
        "readContentText(path, limits)"
        "exactFields(json, fields)"
        "ContentSnapshotBuilder"
        "makeSimConfigHash"
        "result.frozen_ = true")
    string(FIND "${source_text}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "world generation loader contract is missing: ${required}")
    endif()
endforeach()

file(READ "${plan}" plan_text)
foreach(forbidden IN ITEMS
        "proc::Seed seed{0x5EED2026"
        "std::uint32_t map_size_m{600}"
        "float vegetation{0.62F}"
        "float buildings{0.55F}"
        "float fenced_parcels{0.48F}"
        "float river_probability{0.35F}")
    string(FIND "${plan_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "native world request default remains active: ${forbidden}")
    endif()
endforeach()

file(READ "${city}" city_text)
foreach(forbidden IN ITEMS
        "proc::Seed seed{0x5EED2026"
        "std::uint32_t map_size_m{600}"
        "float buildings{0.55F}"
        "float fenced_parcels{0.48F}")
    string(FIND "${city_text}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "native city request default remains active: ${forbidden}")
    endif()
endforeach()

file(READ "${world_config}" world_config_text)
string(FIND "${world_config_text}" "explicit_seed{0x5EED2026" legacy_seed)
if(NOT legacy_seed EQUAL -1)
    message(FATAL_ERROR "application seed input retains a native world default")
endif()

file(READ "${scene_catalog}" scene_catalog_text)
foreach(required IN ITEMS
        "FrozenWorldGenerationProfile"
        "world_generation_profile")
    string(FIND "${scene_catalog_text}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "scene catalog does not own the frozen world profile: ${required}")
    endif()
endforeach()

set(production_clients
    "${GENOMES_SOURCE_DIR}/apps/game/GameApplication.cpp"
    "${GENOMES_SOURCE_DIR}/apps/headless/main.cpp"
    "${GENOMES_SOURCE_DIR}/apps/menu/main.cpp"
    "${GENOMES_SOURCE_DIR}/apps/proc_viewer/main.cpp"
    "${GENOMES_SOURCE_DIR}/benchmarks/world/streaming_benchmark.cpp")
foreach(client IN LISTS production_clients)
    file(READ "${client}" client_text)
    string(FIND "${client_text}" "loadWorldGenerationProfile" loaded)
    if(loaded EQUAL -1)
        message(FATAL_ERROR "production client bypasses the world profile: ${client}")
    endif()
    string(FIND "${client_text}" "WorldGenerationRequest request{}" native_request)
    if(NOT native_request EQUAL -1)
        message(FATAL_ERROR "production client constructs an unresolved world request: ${client}")
    endif()
endforeach()

message(STATUS "World generation profile source contract passed")
