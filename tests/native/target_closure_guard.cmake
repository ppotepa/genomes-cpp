if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(_root "${GENOMES_SOURCE_DIR}")
set(_runtime_file "${_root}/engine/runtime/CMakeLists.txt")
set(_game_scenes_file "${_root}/engine/game_scenes/CMakeLists.txt")
set(_world_file "${_root}/engine/world/CMakeLists.txt")
set(_tests_file "${_root}/tests/native/CMakeLists.txt")
set(_game_file "${_root}/apps/game/CMakeLists.txt")
set(_menu_file "${_root}/apps/menu/CMakeLists.txt")
set(_apps_file "${_root}/apps/CMakeLists.txt")
set(_engine_file "${_root}/engine/CMakeLists.txt")
set(_geometry_file "${_root}/engine/geometry/CMakeLists.txt")
set(_compute_file "${_root}/engine/compute/cuda/CMakeLists.txt")
set(_cuda_file "${_root}/cmake/GenomesCuda.cmake")
set(_toolkit_file "${_root}/cmake/GenomesToolkitDependencies.cmake")
set(_options_file "${_root}/cmake/GenomesOptions.cmake")
set(_presets_file "${_root}/CMakePresets.json")
set(_fixture_manifest "${_root}/reference/fixtures/manifest.json")

foreach(_required IN ITEMS
        "${_runtime_file}" "${_game_scenes_file}" "${_world_file}"
        "${_tests_file}" "${_game_file}" "${_menu_file}" "${_apps_file}"
        "${_engine_file}" "${_geometry_file}" "${_compute_file}"
        "${_cuda_file}" "${_toolkit_file}" "${_options_file}"
        "${_presets_file}" "${_fixture_manifest}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR "Target closure input is missing: ${_required}")
    endif()
endforeach()

function(_require_text _variable _needle _message)
    string(FIND "${${_variable}}" "${_needle}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "${_message}: '${_needle}'")
    endif()
endfunction()

function(_forbid_text _variable _needle _message)
    string(FIND "${${_variable}}" "${_needle}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "${_message}: '${_needle}'")
    endif()
endfunction()

function(_preset_cache_value _json _preset _key _output)
    string(JSON _count LENGTH "${_json}" configurePresets)
    math(EXPR _last "${_count} - 1")
    set(_value "<missing>")
    if(_count GREATER 0)
        foreach(_index RANGE 0 ${_last})
            string(JSON _name GET "${_json}" configurePresets ${_index} name)
            if(_name STREQUAL "${_preset}")
                string(JSON _candidate ERROR_VARIABLE _json_error GET
                       "${_json}" configurePresets ${_index} cacheVariables ${_key})
                if(NOT _json_error)
                    set(_value "${_candidate}")
                endif()
            endif()
        endforeach()
    endif()
    set(${_output} "${_value}" PARENT_SCOPE)
endfunction()

file(READ "${_runtime_file}" _runtime)
file(READ "${_game_scenes_file}" _game_scenes)
file(READ "${_world_file}" _world)
file(READ "${_tests_file}" _tests)
file(READ "${_game_file}" _game)
file(READ "${_menu_file}" _menu)
file(READ "${_apps_file}" _apps)
file(READ "${_engine_file}" _engine)
file(READ "${_geometry_file}" _geometry)
file(READ "${_compute_file}" _compute)
file(READ "${_cuda_file}" _cuda)
file(READ "${_toolkit_file}" _toolkit)
file(READ "${_options_file}" _options)
file(READ "${_presets_file}" _presets)

# Neutral runtime owns lifecycle only. It must not acquire product gameplay or
# product-scene implementation through its public target closure.
foreach(_required IN ITEMS
        "add_library(genomes_runtime_core STATIC"
        "add_library(genomes::runtime_core ALIAS genomes_runtime_core"
        "target_include_directories(genomes_runtime_core PUBLIC"
        "target_link_libraries(genomes_runtime_core PUBLIC"
        "genomes::simulation")
    _require_text(_runtime "${_required}" "Runtime target contract is incomplete")
endforeach()
foreach(_forbidden IN ITEMS
        "genomes::gameplay" "genomes::infantry" "genomes::buildings"
        "genomes::world_render" "genomes::combat" "genomes::physics"
        "genomes::world_generation" "genomes::game_scenes"
        "src/BattlefieldScene.cpp" "src/BuiltinScenes.cpp" "src/UnitLabScene.cpp")
    _forbid_text(_runtime "${_forbidden}"
                 "Neutral runtime target leaks product closure")
endforeach()

# Product scene implementation and application composition are separate
# targets. Builtin catalog/routing belongs only to application_scenes.
foreach(_required IN ITEMS
        "add_library(genomes_game_scenes STATIC"
        "add_library(genomes::game_scenes ALIAS genomes_game_scenes"
        "add_library(genomes_application_scenes STATIC"
        "genomes::game_scenes"
        "add_library(genomes::application_scenes ALIAS genomes_application_scenes")
    _require_text(_game_scenes "${_required}"
                  "Game-scene/application target separation is incomplete")
endforeach()

string(REGEX MATCH "add_library\\(genomes_game_scenes STATIC[^)]*\\)"
       _game_scenes_impl "${_game_scenes}")
string(REGEX MATCH "add_library\\(genomes_application_scenes STATIC[^)]*\\)"
       _application_scenes_impl "${_game_scenes}")
_forbid_text(_game_scenes_impl "src/BuiltinScenes.cpp"
             "Builtin scene catalog leaked into game_scenes implementation target")
_require_text(_application_scenes_impl "src/BuiltinScenes.cpp"
              "Application scene composition lost builtin catalog ownership")
_require_text(_game "genomes::application_scenes"
              "Game executable does not consume application scene composition")
_forbid_text(_game "genomes::game_scenes"
             "Game executable links product scene implementation directly")
_require_text(_menu "genomes::application_scenes"
              "Menu executable does not consume application scene composition")

# World core is the narrow neutral boundary; generation owns hydrology/roads.
string(REGEX MATCH "target_link_libraries\\(genomes_world_core PUBLIC[^)]*\\)"
       _world_core_links "${_world}")
string(REGEX MATCH "target_link_libraries\\(genomes_world_generation PUBLIC[^)]*\\)"
       _world_generation_links "${_world}")
if(_world_core_links STREQUAL "" OR _world_generation_links STREQUAL "")
    message(FATAL_ERROR "World core/generation public link closures are missing")
endif()
foreach(_required IN ITEMS "genomes::foundation" "genomes::proc" "genomes::io")
    _require_text(_world_core_links "${_required}"
                  "World core is missing a narrow dependency")
endforeach()
foreach(_forbidden IN ITEMS "genomes::hydrology" "genomes::roads")
    _forbid_text(_world_core_links "${_forbidden}"
                 "World core links a generation-only dependency")
endforeach()
foreach(_required IN ITEMS "genomes_world_core" "genomes_hydrology" "genomes_roads")
    _require_text(_world_generation_links "${_required}"
                  "World generation closure is incomplete")
endforeach()

# Optional dependencies must stay behind explicit options and must not enter
# the default headless core preset.
_require_text(_engine "if(GENOMES_ENABLE_ASSETS)"
              "Assets are not isolated behind GENOMES_ENABLE_ASSETS")
_require_text(_geometry "if(GENOMES_ENABLE_CSG)"
              "CSG is not isolated behind GENOMES_ENABLE_CSG")
_require_text(_toolkit "if(NOT GENOMES_ENABLE_ASSETS)"
              "fastgltf/assets configuration is not optional")
_require_text(_toolkit "if(NOT GENOMES_ENABLE_CSG)"
              "Manifold/CSG configuration is not optional")
_require_text(_apps "if(GENOMES_ENABLE_ASSETS AND GENOMES_BUILD_TOOLKIT_TOOLS)"
              "Asset tooling is not behind its optional boundary")
_require_text(_cuda "set(GENOMES_CUDA_ENABLED OFF CACHE INTERNAL"
              "CUDA default-disabled state is not explicit")
_require_text(_cuda "if(CUDAToolkit_FOUND AND TARGET CUDA::cudart)"
              "CUDA enablement does not require an explicit toolkit target")
_require_text(_compute "if(GENOMES_CUDA_ENABLED)"
              "CUDA target does not branch on resolved enablement")
_require_text(_compute "target_link_libraries(genomes_compute_cuda PRIVATE CUDA::cudart)"
              "CUDA runtime is not private to the optional backend")
_require_text(_compute "GENOMES_CUDA_ENABLED=0"
              "CUDA fallback target is missing")

string(JSON _fixture_manifest_schema GET
       "${_presets}" version)
if(NOT _fixture_manifest_schema STREQUAL "3")
    message(FATAL_ERROR "CMake preset schema is not version 3")
endif()
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_RENDER_BACKEND" _headless_backend)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_DILIGENT" _headless_diligent)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_SDL" _headless_sdl)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_RMLUI" _headless_rml)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_ASSETS" _headless_assets)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_CSG" _headless_csg)
_preset_cache_value("${_presets}" "headless-core-debug" "GENOMES_ENABLE_CUDA" _headless_cuda)
foreach(_pair IN ITEMS
        "${_headless_backend}|HEADLESS"
        "${_headless_diligent}|OFF"
        "${_headless_sdl}|OFF"
        "${_headless_rml}|OFF"
        "${_headless_assets}|OFF"
        "${_headless_csg}|OFF"
        "${_headless_cuda}|OFF")
    string(REPLACE "|" ";" _values "${_pair}")
    list(GET _values 0 _actual)
    list(GET _values 1 _expected)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR
                "headless-core-debug optional boundary is '${_actual}', expected '${_expected}'")
    endif()
endforeach()
_preset_cache_value("${_presets}" "toolkit-full-debug" "GENOMES_ENABLE_ASSETS" _toolkit_assets)
_preset_cache_value("${_presets}" "toolkit-full-debug" "GENOMES_ENABLE_CSG" _toolkit_csg)
if(NOT _toolkit_assets STREQUAL "ON" OR NOT _toolkit_csg STREQUAL "ON")
    message(FATAL_ERROR "toolkit-full-debug does not explicitly opt into assets and CSG")
endif()

# The public-header consumer must remain a real target with explicit narrow
# library links; source-only presence of headers is not enough evidence.
foreach(_required IN ITEMS
        "add_executable(genomes_test_public_header_consumer"
        "target_link_libraries(genomes_test_public_header_consumer PRIVATE"
        "genomes::buildings" "genomes::content" "genomes::gameplay"
        "genomes::navigation" "genomes::physics" "genomes::world_core"
        "genomes::world_generation" "genomes::world_render"
        "add_test(NAME architecture.public_header_consumer")
    _require_text(_tests "${_required}"
                  "Public-header consumer closure is incomplete")
endforeach()
foreach(_consumer IN ITEMS
        "genomes_test_public_header_runtime_consumer"
        "genomes_test_public_header_simulation_consumer"
        "genomes_test_public_header_render_consumer"
        "genomes_test_public_header_game_scenes_consumer")
    _require_text(_tests "${_consumer}"
                  "Minimal public-header consumer is missing")
endforeach()
if(_tests MATCHES "GENOMES_ENABLE_INFANTRY")
    _require_text(_tests "genomes_test_public_header_infantry_consumer"
                  "Infantry public-header consumer is missing")
endif()
foreach(_header_consumer IN ITEMS
        public_header_building_profile.cpp public_header_content.cpp
        public_header_gameplay.cpp public_header_navigation.cpp
        public_header_physics.cpp public_header_world.cpp
        public_header_world_core.cpp public_header_world_generation.cpp
        public_header_world_generation_profile.cpp public_header_world_render.cpp)
    if(NOT EXISTS "${_root}/tests/native/${_header_consumer}")
        message(FATAL_ERROR "Public-header consumer source is missing: ${_header_consumer}")
    endif()
endforeach()
foreach(_header_consumer IN ITEMS public_header_runtime.cpp public_header_simulation.cpp
        public_header_render.cpp public_header_game_scenes.cpp public_header_infantry.cpp)
    if(NOT EXISTS "${_root}/tests/native/${_header_consumer}")
        message(FATAL_ERROR "Minimal public-header source is missing: ${_header_consumer}")
    endif()
endforeach()

message(STATUS "Target closure, optional-boundary, and public-header source contract passed")
