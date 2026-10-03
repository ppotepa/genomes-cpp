function(genomes_enable_test_assertions target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

add_executable(genomes_test_math_types math/math_types_tests.cpp)
target_link_libraries(genomes_test_math_types PRIVATE genomes::math)
genomes_set_project_warnings(genomes_test_math_types)
genomes_enable_test_assertions(genomes_test_math_types)
add_test(NAME math.types COMMAND genomes_test_math_types)
set_tests_properties(math.types PROPERTIES LABELS "cpu;math")

add_executable(genomes_test_camera_types camera/camera_tests.cpp)
target_link_libraries(genomes_test_camera_types PRIVATE genomes::camera)
genomes_set_project_warnings(genomes_test_camera_types)
genomes_enable_test_assertions(genomes_test_camera_types)
add_test(NAME camera.types COMMAND genomes_test_camera_types)
set_tests_properties(camera.types PROPERTIES LABELS "cpu;camera")
add_executable(genomes_test_camera_controller camera/controller_tests.cpp)
target_link_libraries(genomes_test_camera_controller PRIVATE genomes::camera)
genomes_set_project_warnings(genomes_test_camera_controller)
genomes_enable_test_assertions(genomes_test_camera_controller)
add_test(NAME camera.controller COMMAND genomes_test_camera_controller)
add_executable(genomes_test_viewport_controller camera/viewport_controller_tests.cpp)
target_link_libraries(genomes_test_viewport_controller PRIVATE genomes::runtime_core)
genomes_set_project_warnings(genomes_test_viewport_controller)
genomes_enable_test_assertions(genomes_test_viewport_controller)
add_test(NAME camera.viewport_controller COMMAND genomes_test_viewport_controller)
set_tests_properties(camera.viewport_controller PROPERTIES LABELS "cpu;camera;runtime")
set_tests_properties(camera.controller PROPERTIES LABELS "cpu;camera")

add_executable(genomes_test_platform_window_contract platform_window_contract_tests.cpp)
target_link_libraries(genomes_test_platform_window_contract PRIVATE genomes::foundation genomes::input)
target_include_directories(genomes_test_platform_window_contract PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/platform/include")
genomes_set_project_warnings(genomes_test_platform_window_contract)
genomes_enable_test_assertions(genomes_test_platform_window_contract)
add_test(NAME platform.window_contract COMMAND genomes_test_platform_window_contract)
set_tests_properties(platform.window_contract PROPERTIES LABELS "cpu;platform")

add_test(NAME config.boundaries
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/config_boundaries.cmake")
set_tests_properties(config.boundaries PROPERTIES LABELS "source;cpu;configuration")
add_test(NAME architecture.refactor_tracker
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/architecture_refactor_tracker.cmake")
set_tests_properties(architecture.refactor_tracker PROPERTIES
    LABELS "source;cpu;architecture")
add_test(NAME architecture.scene_api_boundary
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/scene_api_boundary_guard.cmake")
set_tests_properties(architecture.scene_api_boundary PROPERTIES
    LABELS "source;cpu;architecture;api")
add_test(NAME simulation.battlefield_pipeline_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/battlefield_pipeline_guard.cmake")
set_tests_properties(simulation.battlefield_pipeline_source_contract PROPERTIES
    LABELS "source;cpu;simulation;combat")
add_test(NAME simulation.session_clock_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/session_simulation_clock_guard.cmake")
set_tests_properties(simulation.session_clock_source_contract PROPERTIES
    LABELS "source;cpu;simulation;architecture")
add_test(NAME world.resolved_artifact_revision_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/resolved_world_artifact_guard.cmake")
set_tests_properties(world.resolved_artifact_revision_source_contract PROPERTIES
    LABELS "source;cpu;world;architecture")
add_test(NAME architecture.composition_root
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/composition_root_guard.cmake")
set_tests_properties(architecture.composition_root PROPERTIES
    LABELS "source;cpu;architecture")
add_test(NAME architecture.target_closure
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/target_closure_guard.cmake")
set_tests_properties(architecture.target_closure PROPERTIES
    LABELS "source;cpu;architecture;build")
add_test(NAME architecture.application_command_boundary
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/application_command_boundary_guard.cmake")
set_tests_properties(architecture.application_command_boundary PROPERTIES
    LABELS "source;cpu;architecture;application")

# The Diligent backend is not instantiated by headless tests, but its
# RenderLane/device-loss ownership rules remain a source-level architecture
# contract and must run in every profile.
add_test(NAME render.diligent_source_contracts
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/../infantry_upgrade/diligent_source_contracts.cmake")
set_tests_properties(render.diligent_source_contracts PROPERTIES
    LABELS "source;cpu;render;architecture;diligent")

add_test(NAME ballistics.ammunition_catalog_source
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ballistics_ammunition_catalog_guard.cmake")
set_tests_properties(ballistics.ammunition_catalog_source PROPERTIES
    LABELS "source;cpu;ballistics")
add_test(NAME weapons.catalog_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/weapons_catalog_guard.cmake")
set_tests_properties(weapons.catalog_source_contract PROPERTIES
    LABELS "source;cpu;weapons")
add_test(NAME combat.tactical_ai_catalog_source
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/combat_tactical_ai_catalog_guard.cmake")
set_tests_properties(combat.tactical_ai_catalog_source PROPERTIES
    LABELS "source;cpu;combat;content")
add_test(NAME infantry.appearance_catalog_source
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/appearance_catalog_guard.cmake")
set_tests_properties(infantry.appearance_catalog_source PROPERTIES
    LABELS "source;cpu;infantry;content")
add_test(NAME buildings.profile_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/building_profile_guard.cmake")
set_tests_properties(buildings.profile_source_contract PROPERTIES
    LABELS "source;cpu;buildings;content")
add_test(NAME pr16.measurement_benchmark_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/pr16_measurement_benchmark_guard.cmake")
set_tests_properties(pr16.measurement_benchmark_source_contract PROPERTIES
    LABELS "source;cpu;benchmark;performance")
add_test(NAME world.generation_profile_source_contract
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/world_generation_profile_guard.cmake")
set_tests_properties(world.generation_profile_source_contract PROPERTIES
    LABELS "source;cpu;world;content")

add_executable(genomes_test_public_header_consumer
    public_header_consumer.cpp
    public_header_building_profile.cpp
    public_header_content.cpp
    public_header_gameplay.cpp
    public_header_navigation.cpp
    public_header_physics.cpp
    public_header_world.cpp
    public_header_world_core.cpp
    public_header_world_generation.cpp
    public_header_world_generation_profile.cpp
    public_header_world_render.cpp)
target_link_libraries(genomes_test_public_header_consumer PRIVATE
    genomes::buildings
    genomes::content
    genomes::gameplay
    genomes::navigation
    genomes::physics
    genomes::world_core
    genomes::world_generation
    genomes::world_render)
genomes_set_project_warnings(genomes_test_public_header_consumer)
genomes_enable_test_assertions(genomes_test_public_header_consumer)
add_test(NAME architecture.public_header_consumer
    COMMAND genomes_test_public_header_consumer)
set_tests_properties(architecture.public_header_consumer PROPERTIES
    LABELS "source;cpu;architecture")

add_executable(genomes_test_public_header_runtime_consumer public_header_runtime.cpp)
target_link_libraries(genomes_test_public_header_runtime_consumer PRIVATE genomes::runtime_core)
genomes_set_project_warnings(genomes_test_public_header_runtime_consumer)
genomes_enable_test_assertions(genomes_test_public_header_runtime_consumer)
add_test(NAME architecture.public_header_runtime_consumer
    COMMAND genomes_test_public_header_runtime_consumer)
set_tests_properties(architecture.public_header_runtime_consumer PROPERTIES
    LABELS "source;cpu;architecture")

add_executable(genomes_test_public_header_simulation_consumer public_header_simulation.cpp)
target_link_libraries(genomes_test_public_header_simulation_consumer PRIVATE genomes::simulation)
genomes_set_project_warnings(genomes_test_public_header_simulation_consumer)
genomes_enable_test_assertions(genomes_test_public_header_simulation_consumer)
add_test(NAME architecture.public_header_simulation_consumer
    COMMAND genomes_test_public_header_simulation_consumer)
set_tests_properties(architecture.public_header_simulation_consumer PROPERTIES
    LABELS "source;cpu;architecture")

add_executable(genomes_test_public_header_render_consumer public_header_render.cpp)
target_link_libraries(genomes_test_public_header_render_consumer PRIVATE genomes::render_core)
genomes_set_project_warnings(genomes_test_public_header_render_consumer)
genomes_enable_test_assertions(genomes_test_public_header_render_consumer)
add_test(NAME architecture.public_header_render_consumer
    COMMAND genomes_test_public_header_render_consumer)
set_tests_properties(architecture.public_header_render_consumer PROPERTIES
    LABELS "source;cpu;architecture")

add_executable(genomes_test_public_header_game_scenes_consumer public_header_game_scenes.cpp)
target_link_libraries(genomes_test_public_header_game_scenes_consumer PRIVATE genomes::application_scenes)
genomes_set_project_warnings(genomes_test_public_header_game_scenes_consumer)
genomes_enable_test_assertions(genomes_test_public_header_game_scenes_consumer)
add_test(NAME architecture.public_header_game_scenes_consumer
    COMMAND genomes_test_public_header_game_scenes_consumer)
set_tests_properties(architecture.public_header_game_scenes_consumer PROPERTIES
    LABELS "source;cpu;architecture")

if(GENOMES_ENABLE_INFANTRY)
    add_executable(genomes_test_public_header_infantry_consumer public_header_infantry.cpp)
    target_link_libraries(genomes_test_public_header_infantry_consumer PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_public_header_infantry_consumer)
    genomes_enable_test_assertions(genomes_test_public_header_infantry_consumer)
    add_test(NAME architecture.public_header_infantry_consumer
        COMMAND genomes_test_public_header_infantry_consumer)
    set_tests_properties(architecture.public_header_infantry_consumer PROPERTIES
        LABELS "source;cpu;architecture;infantry")
endif()

add_executable(genomes_test_world_core_namespace_boundary
    world_core_namespace_boundary_tests.cpp)
target_link_libraries(genomes_test_world_core_namespace_boundary PRIVATE
    genomes::world_core)
genomes_set_project_warnings(genomes_test_world_core_namespace_boundary)
genomes_enable_test_assertions(genomes_test_world_core_namespace_boundary)
add_test(NAME world_core.namespace_boundary
    COMMAND genomes_test_world_core_namespace_boundary)
set_tests_properties(world_core.namespace_boundary PROPERTIES
    LABELS "source;cpu;architecture;world")

add_test(NAME ui.scene_guard
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/scene_ui_guard.cmake")
set_tests_properties(ui.scene_guard PROPERTIES LABELS "source;cpu;ui")
add_executable(genomes_test_polygon geometry/polygon_tests.cpp)
target_link_libraries(genomes_test_polygon PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_polygon)
genomes_enable_test_assertions(genomes_test_polygon)
add_test(NAME geometry.polygon COMMAND genomes_test_polygon)
set_tests_properties(geometry.polygon PROPERTIES LABELS "cpu;geometry")
add_executable(genomes_test_geometry_primitives geometry/primitive_tests.cpp)
target_link_libraries(genomes_test_geometry_primitives PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_geometry_primitives)
genomes_enable_test_assertions(genomes_test_geometry_primitives)
add_test(NAME geometry.primitives COMMAND genomes_test_geometry_primitives)
set_tests_properties(geometry.primitives PROPERTIES LABELS "cpu;geometry")
add_executable(genomes_test_mesh_data geometry/mesh_data_tests.cpp)
target_link_libraries(genomes_test_mesh_data PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_mesh_data)
genomes_enable_test_assertions(genomes_test_mesh_data)
add_test(NAME geometry.mesh_data COMMAND genomes_test_mesh_data)
set_tests_properties(geometry.mesh_data PROPERTIES LABELS "cpu;geometry")
add_executable(genomes_test_lightweight_toolkit integration/lightweight_toolkit_tests.cpp)
target_link_libraries(genomes_test_lightweight_toolkit PRIVATE genomes::render_core)
genomes_set_project_warnings(genomes_test_lightweight_toolkit)
genomes_enable_test_assertions(genomes_test_lightweight_toolkit)
add_test(NAME integration.lightweight_toolkit COMMAND genomes_test_lightweight_toolkit)
set_tests_properties(integration.lightweight_toolkit PROPERTIES LABELS "cpu;integration;geometry;camera")
add_executable(genomes_test_tangent_space geometry/tangent_space_tests.cpp)
target_link_libraries(genomes_test_tangent_space PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_tangent_space)
genomes_enable_test_assertions(genomes_test_tangent_space)
add_test(NAME geometry.tangent_space COMMAND genomes_test_tangent_space)
set_tests_properties(geometry.tangent_space PROPERTIES LABELS "cpu;geometry")
if(GENOMES_ENABLE_CSG)
    add_executable(genomes_test_solid_ops geometry/solid_ops_tests.cpp)
    target_link_libraries(genomes_test_solid_ops PRIVATE genomes::geometry)
    genomes_set_project_warnings(genomes_test_solid_ops)
    genomes_enable_test_assertions(genomes_test_solid_ops)
    add_test(NAME geometry.solid_ops COMMAND genomes_test_solid_ops)
    set_tests_properties(geometry.solid_ops PROPERTIES LABELS "cpu;geometry;csg")
endif()
if(GENOMES_ENABLE_ASSETS)
    add_executable(genomes_test_gltf_import assets/gltf_import_tests.cpp)
    target_link_libraries(genomes_test_gltf_import PRIVATE genomes::assets)
    target_compile_definitions(genomes_test_gltf_import PRIVATE GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_gltf_import)
    genomes_enable_test_assertions(genomes_test_gltf_import)
    add_test(NAME assets.gltf_import COMMAND genomes_test_gltf_import)
    set_tests_properties(assets.gltf_import PROPERTIES LABELS "cpu;assets")
endif()

if(TARGET genomes_render_smoke)
    add_test(NAME render.diligent_headless COMMAND genomes_render_smoke)
    set_tests_properties(render.diligent_headless PROPERTIES LABELS "cpu;diligent")
endif()

add_executable(genomes_test_foundation_smoke foundation_smoke.cpp)
target_link_libraries(genomes_test_foundation_smoke PRIVATE genomes::foundation)
genomes_set_project_warnings(genomes_test_foundation_smoke)
genomes_enable_test_assertions(genomes_test_foundation_smoke)

add_test(NAME foundation_smoke COMMAND genomes_test_foundation_smoke)

add_executable(genomes_test_api_module_host api/module_host_tests.cpp)
target_link_libraries(genomes_test_api_module_host PRIVATE genomes::api)
genomes_set_project_warnings(genomes_test_api_module_host)
genomes_enable_test_assertions(genomes_test_api_module_host)
add_test(NAME api.module_host COMMAND genomes_test_api_module_host)
set_tests_properties(api.module_host PROPERTIES LABELS "cpu;api;architecture")

add_executable(genomes_test_config_hash config_hash_tests.cpp)
target_link_libraries(genomes_test_config_hash PRIVATE genomes::foundation)
genomes_set_project_warnings(genomes_test_config_hash)
genomes_enable_test_assertions(genomes_test_config_hash)
add_test(NAME foundation.config_hash COMMAND genomes_test_config_hash)

add_executable(genomes_test_content_snapshot content_snapshot_tests.cpp)
target_link_libraries(genomes_test_content_snapshot PRIVATE genomes::content)
genomes_set_project_warnings(genomes_test_content_snapshot)
genomes_enable_test_assertions(genomes_test_content_snapshot)
add_test(NAME content.snapshot COMMAND genomes_test_content_snapshot)

add_executable(genomes_test_configuration_snapshot configuration_snapshot_tests.cpp)
target_link_libraries(genomes_test_configuration_snapshot PRIVATE genomes::content)
genomes_set_project_warnings(genomes_test_configuration_snapshot)
genomes_enable_test_assertions(genomes_test_configuration_snapshot)
add_test(NAME content.configuration_snapshot COMMAND genomes_test_configuration_snapshot)

add_executable(genomes_test_artifact_cache artifact_cache_tests.cpp)
target_link_libraries(genomes_test_artifact_cache PRIVATE genomes::proc)
genomes_set_project_warnings(genomes_test_artifact_cache)
genomes_enable_test_assertions(genomes_test_artifact_cache)
add_test(NAME proc.artifact_cache COMMAND genomes_test_artifact_cache)

add_executable(genomes_test_memory_budget
    stress/memory_budget_tests.cpp)
target_link_libraries(genomes_test_memory_budget PRIVATE genomes::foundation)
genomes_set_project_warnings(genomes_test_memory_budget)
genomes_enable_test_assertions(genomes_test_memory_budget)
add_test(NAME foundation.memory_budget COMMAND genomes_test_memory_budget)

add_executable(genomes_test_menu_smoke menu_smoke.cpp)
target_link_libraries(genomes_test_menu_smoke PRIVATE genomes::application_scenes)
target_compile_definitions(genomes_test_menu_smoke PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_menu_smoke)
genomes_enable_test_assertions(genomes_test_menu_smoke)

add_test(NAME menu_smoke COMMAND genomes_test_menu_smoke)

add_executable(genomes_test_world_lab_smoke world_lab_smoke.cpp)
target_link_libraries(genomes_test_world_lab_smoke PRIVATE genomes::application_scenes)
target_compile_definitions(genomes_test_world_lab_smoke PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_world_lab_smoke)
genomes_enable_test_assertions(genomes_test_world_lab_smoke)
add_test(NAME runtime.world_lab_smoke COMMAND genomes_test_world_lab_smoke)

add_library(genomes_test_native_ui_plugin_a SHARED
    plugins/ui_test_plugin.cpp)
target_include_directories(genomes_test_native_ui_plugin_a PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/ui/include")
target_compile_definitions(genomes_test_native_ui_plugin_a PRIVATE GENOMES_TEST_PLUGIN_ID="test.plugin.a")
set_target_properties(genomes_test_native_ui_plugin_a PROPERTIES
    PREFIX "")
genomes_set_project_warnings(genomes_test_native_ui_plugin_a)

add_library(genomes_test_native_ui_plugin_b SHARED
    plugins/ui_test_plugin.cpp)
target_include_directories(genomes_test_native_ui_plugin_b PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/ui/include")
target_compile_definitions(genomes_test_native_ui_plugin_b PRIVATE GENOMES_TEST_PLUGIN_ID="test.plugin.b")
set_target_properties(genomes_test_native_ui_plugin_b PROPERTIES PREFIX "")
genomes_set_project_warnings(genomes_test_native_ui_plugin_b)

add_executable(genomes_test_ui_content_registry ui_content_registry_tests.cpp)
target_link_libraries(genomes_test_ui_content_registry PRIVATE genomes::ui)
target_compile_definitions(genomes_test_ui_content_registry PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    GENOMES_TEST_NATIVE_PLUGIN_A_PATH="$<TARGET_FILE:genomes_test_native_ui_plugin_a>"
    GENOMES_TEST_NATIVE_PLUGIN_B_PATH="$<TARGET_FILE:genomes_test_native_ui_plugin_b>")
add_dependencies(genomes_test_ui_content_registry
    genomes_test_native_ui_plugin_a genomes_test_native_ui_plugin_b)
genomes_set_project_warnings(genomes_test_ui_content_registry)
genomes_enable_test_assertions(genomes_test_ui_content_registry)
add_test(NAME ui.content_registry COMMAND genomes_test_ui_content_registry)

add_executable(genomes_test_ui_data_model ui_data_model_tests.cpp)
target_link_libraries(genomes_test_ui_data_model PRIVATE genomes::ui)
genomes_set_project_warnings(genomes_test_ui_data_model)
genomes_enable_test_assertions(genomes_test_ui_data_model)
add_test(NAME ui.data_model COMMAND genomes_test_ui_data_model)

add_executable(genomes_test_ui_services ui_services_tests.cpp)
target_link_libraries(genomes_test_ui_services PRIVATE genomes::ui)
genomes_set_project_warnings(genomes_test_ui_services)
genomes_enable_test_assertions(genomes_test_ui_services)
add_test(NAME ui.services COMMAND genomes_test_ui_services)

add_executable(genomes_test_ui_route_lifecycle ui_route_lifecycle_tests.cpp)
target_link_libraries(genomes_test_ui_route_lifecycle PRIVATE genomes::ui)
genomes_set_project_warnings(genomes_test_ui_route_lifecycle)
genomes_enable_test_assertions(genomes_test_ui_route_lifecycle)
add_test(NAME ui.route_lifecycle COMMAND genomes_test_ui_route_lifecycle)

if(GENOMES_ENABLE_RMLUI)
    add_executable(genomes_test_ui_rml_smoke ui_rml_smoke.cpp)
    target_link_libraries(genomes_test_ui_rml_smoke PRIVATE genomes::ui_rml)
    target_compile_definitions(genomes_test_ui_rml_smoke PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_ui_rml_smoke)
    genomes_enable_test_assertions(genomes_test_ui_rml_smoke)
    add_test(NAME ui.rml_smoke COMMAND genomes_test_ui_rml_smoke)
endif()

if(GENOMES_ENABLE_INFANTRY)
    add_executable(genomes_test_infantry_simulation infantry_simulation_tests.cpp)
    target_link_libraries(genomes_test_infantry_simulation PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_simulation)
    genomes_enable_test_assertions(genomes_test_infantry_simulation)
    add_test(NAME infantry.simulation COMMAND genomes_test_infantry_simulation)

    add_executable(genomes_test_unit_lab_smoke unit_lab_smoke.cpp)
    target_link_libraries(genomes_test_unit_lab_smoke PRIVATE genomes::game_scenes)
    genomes_set_project_warnings(genomes_test_unit_lab_smoke)
    genomes_enable_test_assertions(genomes_test_unit_lab_smoke)
    add_test(NAME runtime.unit_lab_smoke COMMAND genomes_test_unit_lab_smoke)

    add_executable(genomes_test_unit_lab_command_parsing unit_lab_command_parsing_tests.cpp)
    target_link_libraries(genomes_test_unit_lab_command_parsing PRIVATE genomes::game_scenes)
    genomes_set_project_warnings(genomes_test_unit_lab_command_parsing)
    genomes_enable_test_assertions(genomes_test_unit_lab_command_parsing)
    add_test(NAME runtime.unit_lab_command_parsing COMMAND genomes_test_unit_lab_command_parsing)

    add_executable(genomes_test_battlefield_profile battlefield_profile_tests.cpp)
    target_link_libraries(genomes_test_battlefield_profile PRIVATE genomes::gameplay)
    genomes_set_project_warnings(genomes_test_battlefield_profile)
    genomes_enable_test_assertions(genomes_test_battlefield_profile)
    add_test(NAME gameplay.battlefield_profile COMMAND genomes_test_battlefield_profile)

    add_executable(genomes_test_battlefield_determinism battlefield_determinism_tests.cpp)
    target_link_libraries(genomes_test_battlefield_determinism PRIVATE genomes::gameplay)
    genomes_set_project_warnings(genomes_test_battlefield_determinism)
    genomes_enable_test_assertions(genomes_test_battlefield_determinism)
    add_test(NAME simulation.battlefield_determinism COMMAND genomes_test_battlefield_determinism)
    set_tests_properties(simulation.battlefield_determinism PROPERTIES
        LABELS "cpu;simulation;determinism;infantry")

    add_executable(genomes_test_infantry_mass_battle_runtime
        infantry_mass_battle_runtime_tests.cpp)
    target_link_libraries(genomes_test_infantry_mass_battle_runtime PRIVATE genomes::gameplay)
    genomes_set_project_warnings(genomes_test_infantry_mass_battle_runtime)
    genomes_enable_test_assertions(genomes_test_infantry_mass_battle_runtime)
    add_test(NAME simulation.infantry_mass_battle COMMAND genomes_test_infantry_mass_battle_runtime)
    set_tests_properties(simulation.infantry_mass_battle PROPERTIES
        LABELS "cpu;simulation;determinism;infantry;mass-battle")

    add_executable(genomes_test_infantry_presentation infantry_presentation_tests.cpp)
    target_link_libraries(genomes_test_infantry_presentation PRIVATE genomes::game_scenes)
    target_compile_definitions(genomes_test_infantry_presentation PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_infantry_presentation)
    genomes_enable_test_assertions(genomes_test_infantry_presentation)
    add_test(NAME infantry.presentation COMMAND genomes_test_infantry_presentation)

    add_executable(genomes_test_infantry_mass_battle_scene
        infantry_mass_battle_scene_tests.cpp)
    target_link_libraries(genomes_test_infantry_mass_battle_scene PRIVATE genomes::game_scenes)
    genomes_set_project_warnings(genomes_test_infantry_mass_battle_scene)
    genomes_enable_test_assertions(genomes_test_infantry_mass_battle_scene)
    add_test(NAME infantry.mass_battle_scene COMMAND genomes_test_infantry_mass_battle_scene)
endif()

add_executable(genomes_test_foundation_primitives
    foundation/primitives_smoke.cpp)
target_link_libraries(genomes_test_foundation_primitives PRIVATE genomes::foundation)
genomes_set_project_warnings(genomes_test_foundation_primitives)
genomes_enable_test_assertions(genomes_test_foundation_primitives)
add_test(NAME foundation.primitives COMMAND genomes_test_foundation_primitives)

add_executable(genomes_test_published_snapshot_exchange
    foundation/published_snapshot_exchange_tests.cpp)
target_link_libraries(genomes_test_published_snapshot_exchange PRIVATE genomes::foundation)
genomes_set_project_warnings(genomes_test_published_snapshot_exchange)
genomes_enable_test_assertions(genomes_test_published_snapshot_exchange)
add_test(NAME foundation.published_snapshot_exchange
    COMMAND genomes_test_published_snapshot_exchange)
set_tests_properties(foundation.published_snapshot_exchange PROPERTIES
    LABELS "cpu;foundation;snapshot")

add_executable(genomes_test_atomic_file
    io/atomic_file_tests.cpp)
target_link_libraries(genomes_test_atomic_file PRIVATE genomes::io)
genomes_set_project_warnings(genomes_test_atomic_file)
genomes_enable_test_assertions(genomes_test_atomic_file)
add_test(NAME io.atomic_file COMMAND genomes_test_atomic_file)

add_executable(genomes_test_simulation_fixed_step
    simulation/fixed_step_clock_smoke.cpp)
target_link_libraries(genomes_test_simulation_fixed_step PRIVATE genomes::simulation)
genomes_set_project_warnings(genomes_test_simulation_fixed_step)
genomes_enable_test_assertions(genomes_test_simulation_fixed_step)
add_test(NAME simulation.fixed_step COMMAND genomes_test_simulation_fixed_step)

add_executable(genomes_test_session_simulation_clock session_simulation_clock_tests.cpp)
target_link_libraries(genomes_test_session_simulation_clock PRIVATE genomes::simulation)
genomes_set_project_warnings(genomes_test_session_simulation_clock)
genomes_enable_test_assertions(genomes_test_session_simulation_clock)
add_test(NAME simulation.session_clock COMMAND genomes_test_session_simulation_clock)

add_executable(genomes_test_frame_coordinator runtime/frame_coordinator_tests.cpp)
target_link_libraries(genomes_test_frame_coordinator PRIVATE genomes::runtime_core)
genomes_set_project_warnings(genomes_test_frame_coordinator)
genomes_enable_test_assertions(genomes_test_frame_coordinator)
add_test(NAME runtime.frame_coordinator COMMAND genomes_test_frame_coordinator)
set_tests_properties(runtime.frame_coordinator PROPERTIES LABELS "cpu;runtime;simulation")

add_executable(genomes_test_world_grid_layout world_grid_layout_tests.cpp)
target_link_libraries(genomes_test_world_grid_layout PRIVATE genomes::world_core)
genomes_set_project_warnings(genomes_test_world_grid_layout)
genomes_enable_test_assertions(genomes_test_world_grid_layout)
add_test(NAME world.grid_layout COMMAND genomes_test_world_grid_layout)

add_executable(genomes_test_world_mesh_compiler world_mesh_compiler_tests.cpp)
target_link_libraries(genomes_test_world_mesh_compiler PRIVATE genomes::world_render)
genomes_set_project_warnings(genomes_test_world_mesh_compiler)
genomes_enable_test_assertions(genomes_test_world_mesh_compiler)
add_test(NAME world.mesh_compiler COMMAND genomes_test_world_mesh_compiler)

add_executable(genomes_test_system_graph_batch
    simulation/system_graph_batch_tests.cpp)
target_link_libraries(genomes_test_system_graph_batch PRIVATE genomes::simulation)
genomes_set_project_warnings(genomes_test_system_graph_batch)
genomes_enable_test_assertions(genomes_test_system_graph_batch)
add_test(NAME simulation.system_graph_batch COMMAND genomes_test_system_graph_batch)
set_tests_properties(simulation.system_graph_batch PROPERTIES LABELS "cpu;simulation;jobs")

add_executable(genomes_test_command_commit
    simulation/command_commit_tests.cpp)
target_link_libraries(genomes_test_command_commit PRIVATE genomes::simulation)
genomes_set_project_warnings(genomes_test_command_commit)
genomes_enable_test_assertions(genomes_test_command_commit)
add_test(NAME simulation.command_commit COMMAND genomes_test_command_commit)
set_tests_properties(simulation.command_commit PROPERTIES LABELS "cpu;simulation")

add_executable(genomes_test_jobs_smoke
    jobs/job_system_smoke.cpp)
target_link_libraries(genomes_test_jobs_smoke PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_test_jobs_smoke)
genomes_enable_test_assertions(genomes_test_jobs_smoke)
add_test(NAME jobs.core COMMAND genomes_test_jobs_smoke)

add_executable(genomes_test_linear_arena
    jobs/linear_arena_tests.cpp)
target_link_libraries(genomes_test_linear_arena PRIVATE genomes::memory)
genomes_set_project_warnings(genomes_test_linear_arena)
genomes_enable_test_assertions(genomes_test_linear_arena)
add_test(NAME jobs.linear_arena COMMAND genomes_test_linear_arena)
set_tests_properties(jobs.linear_arena PROPERTIES LABELS "cpu;jobs;memory")

add_executable(genomes_test_job_scheduler
    jobs/job_scheduler_tests.cpp)
target_link_libraries(genomes_test_job_scheduler PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_test_job_scheduler)
genomes_enable_test_assertions(genomes_test_job_scheduler)
add_test(NAME jobs.scheduler COMMAND genomes_test_job_scheduler)
set_tests_properties(jobs.scheduler PROPERTIES LABELS "cpu;jobs")

add_executable(genomes_test_job_fence
    jobs/job_fence_tests.cpp)
target_link_libraries(genomes_test_job_fence PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_test_job_fence)
genomes_enable_test_assertions(genomes_test_job_fence)
add_test(NAME jobs.fence COMMAND genomes_test_job_fence)
set_tests_properties(jobs.fence PROPERTIES LABELS "cpu;jobs")

add_executable(genomes_test_job_system_lifecycle
    jobs/job_system_lifecycle_tests.cpp)
target_link_libraries(genomes_test_job_system_lifecycle PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_test_job_system_lifecycle)
genomes_enable_test_assertions(genomes_test_job_system_lifecycle)
add_test(NAME jobs.lifecycle COMMAND genomes_test_job_system_lifecycle)
set_tests_properties(jobs.lifecycle PROPERTIES LABELS "cpu;jobs")

add_executable(genomes_test_job_system_worker_violation
    jobs/job_system_worker_violation.cpp)
target_link_libraries(genomes_test_job_system_worker_violation PRIVATE genomes::jobs)
genomes_set_project_warnings(genomes_test_job_system_worker_violation)
genomes_enable_test_assertions(genomes_test_job_system_worker_violation)
add_test(NAME jobs.worker_shutdown_violation
    COMMAND genomes_test_job_system_worker_violation --shutdown)
add_test(NAME jobs.worker_destructor_violation
    COMMAND genomes_test_job_system_worker_violation --destructor)
set_tests_properties(jobs.worker_shutdown_violation jobs.worker_destructor_violation
    PROPERTIES
        LABELS "cpu;jobs"
        TIMEOUT 5
        PASS_REGULAR_EXPRESSION "genomes\\.jobs\\.contract_violation")

add_executable(genomes_test_thread_scaling_gate
    stress/thread_scaling_smoke.cpp)
target_link_libraries(genomes_test_thread_scaling_gate PRIVATE
    genomes::thread_scaling_gate genomes::jobs)
target_compile_definitions(genomes_test_thread_scaling_gate PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_thread_scaling_gate)
genomes_enable_test_assertions(genomes_test_thread_scaling_gate)
add_test(NAME stress.thread_scaling_gate COMMAND genomes_test_thread_scaling_gate)

add_executable(genomes_test_proc_seed_stream
    proc/seed_stream_smoke.cpp)
target_link_libraries(genomes_test_proc_seed_stream PRIVATE genomes::proc)
genomes_set_project_warnings(genomes_test_proc_seed_stream)
genomes_enable_test_assertions(genomes_test_proc_seed_stream)
add_test(NAME proc.seed_stream COMMAND genomes_test_proc_seed_stream)

add_executable(genomes_test_proc_registry
    proc/generator_registry_smoke.cpp)
target_link_libraries(genomes_test_proc_registry PRIVATE genomes::proc)
genomes_set_project_warnings(genomes_test_proc_registry)
genomes_enable_test_assertions(genomes_test_proc_registry)
add_test(NAME proc.registry COMMAND genomes_test_proc_registry)

add_executable(genomes_test_procedural_runtime
    proc/procedural_runtime_tests.cpp)
target_link_libraries(genomes_test_procedural_runtime PRIVATE genomes::proc)
genomes_set_project_warnings(genomes_test_procedural_runtime)
genomes_enable_test_assertions(genomes_test_procedural_runtime)
add_test(NAME proc.runtime COMMAND genomes_test_procedural_runtime)
set_tests_properties(proc.runtime PROPERTIES LABELS "cpu;proc;jobs")

add_executable(genomes_test_domain_generator_registry
    proc/domain_generator_registry_tests.cpp)
target_link_libraries(genomes_test_domain_generator_registry PRIVATE
    genomes::buildings genomes::hydrology genomes::terrain
    genomes::world_generation genomes::gameplay)
if(GENOMES_ENABLE_INFANTRY)
    target_link_libraries(genomes_test_domain_generator_registry PRIVATE
        genomes::infantry genomes::weapons)
endif()
genomes_set_project_warnings(genomes_test_domain_generator_registry)
genomes_enable_test_assertions(genomes_test_domain_generator_registry)
add_test(NAME proc.domain_generator_registry COMMAND genomes_test_domain_generator_registry)
set_tests_properties(proc.domain_generator_registry PROPERTIES LABELS "cpu;proc")

add_executable(genomes_test_world_coordinates
    world/world_coordinates_smoke.cpp)
target_link_libraries(genomes_test_world_coordinates PRIVATE genomes::world_core)
genomes_set_project_warnings(genomes_test_world_coordinates)
genomes_enable_test_assertions(genomes_test_world_coordinates)
add_test(NAME world.coordinates COMMAND genomes_test_world_coordinates)

add_executable(genomes_test_building_generator
    buildings/building_generator_tests.cpp)
target_link_libraries(genomes_test_building_generator PRIVATE genomes::buildings)
genomes_set_project_warnings(genomes_test_building_generator)
genomes_enable_test_assertions(genomes_test_building_generator)
add_test(NAME buildings.generator COMMAND genomes_test_building_generator)
set_tests_properties(buildings.generator PROPERTIES LABELS "cpu;buildings")

add_executable(genomes_test_building_profile
    buildings/building_profile_tests.cpp)
target_link_libraries(genomes_test_building_profile PRIVATE genomes::buildings)
target_compile_definitions(genomes_test_building_profile PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_building_profile)
genomes_enable_test_assertions(genomes_test_building_profile)
add_test(NAME buildings.profile COMMAND genomes_test_building_profile)
set_tests_properties(buildings.profile PROPERTIES LABELS "cpu;buildings;content")

add_executable(genomes_test_world_generation_profile
    world_generation_profile_tests.cpp)
target_link_libraries(genomes_test_world_generation_profile PRIVATE
    genomes::world_generation)
target_compile_definitions(genomes_test_world_generation_profile PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_world_generation_profile)
genomes_enable_test_assertions(genomes_test_world_generation_profile)
add_test(NAME world.generation_profile COMMAND genomes_test_world_generation_profile)
set_tests_properties(world.generation_profile PROPERTIES LABELS "cpu;world;content")

add_executable(genomes_test_flow_field
    navigation/flow_field_tests.cpp)
target_link_libraries(genomes_test_flow_field PRIVATE genomes::navigation)
genomes_set_project_warnings(genomes_test_flow_field)
genomes_enable_test_assertions(genomes_test_flow_field)
add_test(NAME navigation.flow_field COMMAND genomes_test_flow_field)

add_executable(genomes_test_world_query
    world/world_query_tests.cpp)
target_link_libraries(genomes_test_world_query PRIVATE genomes::world_core)
genomes_set_project_warnings(genomes_test_world_query)
genomes_enable_test_assertions(genomes_test_world_query)
add_test(NAME world.query COMMAND genomes_test_world_query)

add_executable(genomes_test_terrain_model
    terrain/terrain_model_smoke.cpp)
target_link_libraries(genomes_test_terrain_model PRIVATE genomes::terrain)
genomes_set_project_warnings(genomes_test_terrain_model)
genomes_enable_test_assertions(genomes_test_terrain_model)
add_test(NAME terrain.model COMMAND genomes_test_terrain_model)

add_executable(genomes_test_render_backend
    render/render_backend_smoke.cpp)
target_link_libraries(genomes_test_render_backend PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_render_backend)
genomes_enable_test_assertions(genomes_test_render_backend)
add_test(NAME render.backend COMMAND genomes_test_render_backend)

add_executable(genomes_test_render_presentation_contract
    render/presentation_contract_tests.cpp)
target_link_libraries(genomes_test_render_presentation_contract PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_render_presentation_contract)
genomes_enable_test_assertions(genomes_test_render_presentation_contract)
add_test(NAME render.presentation_contract COMMAND genomes_test_render_presentation_contract)

add_executable(genomes_test_skinned_deformer
    render/skinned_deformer_tests.cpp)
target_link_libraries(genomes_test_skinned_deformer PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_skinned_deformer)
genomes_enable_test_assertions(genomes_test_skinned_deformer)
add_test(NAME render.skinned_deformer COMMAND genomes_test_skinned_deformer)

add_executable(genomes_test_color_space
    render/color_space_tests.cpp)
target_link_libraries(genomes_test_color_space PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_color_space)
genomes_enable_test_assertions(genomes_test_color_space)
add_test(NAME render.color_space COMMAND genomes_test_color_space)

add_executable(genomes_test_geometry_mesh_validation
    geometry_mesh_validation_tests.cpp)
target_link_libraries(genomes_test_geometry_mesh_validation PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_geometry_mesh_validation)
genomes_enable_test_assertions(genomes_test_geometry_mesh_validation)
add_test(NAME geometry.mesh_validation COMMAND genomes_test_geometry_mesh_validation)

add_executable(genomes_test_gpu_scene
    render/gpu_scene_smoke.cpp)
target_link_libraries(genomes_test_gpu_scene PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_gpu_scene)
genomes_enable_test_assertions(genomes_test_gpu_scene)
add_test(NAME render.gpu_scene COMMAND genomes_test_gpu_scene)

add_executable(genomes_test_render_extraction
    render/render_extraction_smoke.cpp)
target_link_libraries(genomes_test_render_extraction PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_render_extraction)
genomes_enable_test_assertions(genomes_test_render_extraction)
add_test(NAME render.extraction COMMAND genomes_test_render_extraction)

add_executable(genomes_test_render_graph
    render/render_graph_smoke.cpp)
target_link_libraries(genomes_test_render_graph PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_render_graph)
genomes_enable_test_assertions(genomes_test_render_graph)
add_test(NAME render.graph COMMAND genomes_test_render_graph)

add_executable(genomes_test_render_handles
    render/render_handles_smoke.cpp)
target_link_libraries(genomes_test_render_handles PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_render_handles)
genomes_enable_test_assertions(genomes_test_render_handles)
add_test(NAME render.handles COMMAND genomes_test_render_handles)

add_executable(genomes_test_shader_assets
    render/shader_asset_smoke.cpp)
target_link_libraries(genomes_test_shader_assets PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_shader_assets)
genomes_enable_test_assertions(genomes_test_shader_assets)
add_test(NAME render.shader_assets COMMAND genomes_test_shader_assets)

add_executable(genomes_test_field_atlas
    render/field_atlas_tests.cpp)
target_link_libraries(genomes_test_field_atlas PRIVATE genomes::render)
genomes_set_project_warnings(genomes_test_field_atlas)
genomes_enable_test_assertions(genomes_test_field_atlas)
add_test(NAME render.field_atlas COMMAND genomes_test_field_atlas)

add_executable(genomes_test_compute_backend_selector
    compute/backend_selector_tests.cpp)
target_link_libraries(genomes_test_compute_backend_selector PRIVATE genomes::compute)
genomes_set_project_warnings(genomes_test_compute_backend_selector)
genomes_enable_test_assertions(genomes_test_compute_backend_selector)
add_test(NAME compute.backend_selector COMMAND genomes_test_compute_backend_selector)

add_executable(genomes_test_compute_cuda_optional
    compute/cuda_optional_tests.cpp)
target_link_libraries(genomes_test_compute_cuda_optional PRIVATE genomes::compute_cuda)
genomes_set_project_warnings(genomes_test_compute_cuda_optional)
genomes_enable_test_assertions(genomes_test_compute_cuda_optional)
add_test(NAME compute.cuda_optional COMMAND genomes_test_compute_cuda_optional)

add_executable(genomes_test_compute_cuda_interop
    compute/cuda_interop_tests.cpp)
target_link_libraries(genomes_test_compute_cuda_interop PRIVATE genomes::compute_cuda)
genomes_set_project_warnings(genomes_test_compute_cuda_interop)
genomes_enable_test_assertions(genomes_test_compute_cuda_interop)
add_test(NAME compute.cuda_interop COMMAND genomes_test_compute_cuda_interop)

add_executable(genomes_test_destruction_materials
    destruction/material_solid_tests.cpp)
target_link_libraries(genomes_test_destruction_materials PRIVATE genomes::destruction)
target_compile_definitions(genomes_test_destruction_materials PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_destruction_materials)
genomes_enable_test_assertions(genomes_test_destruction_materials)
add_test(NAME destruction.materials COMMAND genomes_test_destruction_materials)
add_test(NAME destruction.materials_source_contract
    COMMAND ${CMAKE_COMMAND}
        -DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/destruction_material_catalog_guard.cmake")
set_tests_properties(destruction.materials_source_contract PROPERTIES
    LABELS "source;cpu;destruction")

add_executable(genomes_test_destruction_impact
    destruction/impact_solver_tests.cpp)
target_link_libraries(genomes_test_destruction_impact PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_impact)
genomes_enable_test_assertions(genomes_test_destruction_impact)
add_test(NAME destruction.impact COMMAND genomes_test_destruction_impact)

add_executable(genomes_test_destruction_damage_field
    destruction/damage_field_tests.cpp)
target_link_libraries(genomes_test_destruction_damage_field PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_damage_field)
genomes_enable_test_assertions(genomes_test_destruction_damage_field)
add_test(NAME destruction.damage_field COMMAND genomes_test_destruction_damage_field)

add_executable(genomes_test_destruction_structural_graph
    destruction/structural_graph_tests.cpp)
target_link_libraries(genomes_test_destruction_structural_graph PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_structural_graph)
genomes_enable_test_assertions(genomes_test_destruction_structural_graph)
add_test(NAME destruction.structural_graph COMMAND genomes_test_destruction_structural_graph)

add_executable(genomes_test_destruction_debris
    destruction/debris_lifecycle_tests.cpp)
target_link_libraries(genomes_test_destruction_debris PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_debris)
genomes_enable_test_assertions(genomes_test_destruction_debris)
add_test(NAME destruction.debris COMMAND genomes_test_destruction_debris)

add_executable(genomes_test_destruction_rubble_field
    destruction/rubble_field_tests.cpp)
target_link_libraries(genomes_test_destruction_rubble_field PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_rubble_field)
genomes_enable_test_assertions(genomes_test_destruction_rubble_field)
add_test(NAME destruction.rubble_field COMMAND genomes_test_destruction_rubble_field)

add_executable(genomes_test_destruction_physics
    destruction/destruction_physics_tests.cpp)
target_link_libraries(genomes_test_destruction_physics PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_physics)
genomes_enable_test_assertions(genomes_test_destruction_physics)
add_test(NAME destruction.physics COMMAND genomes_test_destruction_physics)

add_executable(genomes_test_destruction_invalidation
    destruction/destruction_invalidation_tests.cpp)
target_link_libraries(genomes_test_destruction_invalidation PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_invalidation)
genomes_enable_test_assertions(genomes_test_destruction_invalidation)
add_test(NAME destruction.invalidation COMMAND genomes_test_destruction_invalidation)

add_executable(genomes_test_destruction_validation
    validation/destruction_validation_tests.cpp)
target_link_libraries(genomes_test_destruction_validation PRIVATE genomes::destruction)
genomes_set_project_warnings(genomes_test_destruction_validation)
genomes_enable_test_assertions(genomes_test_destruction_validation)
add_test(NAME destruction.validation COMMAND genomes_test_destruction_validation)

add_executable(genomes_test_ballistics_projectile_state
    ballistics_projectile_state_tests.cpp)
target_link_libraries(genomes_test_ballistics_projectile_state PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_test_ballistics_projectile_state)
genomes_enable_test_assertions(genomes_test_ballistics_projectile_state)
add_test(NAME ballistics.projectile_state COMMAND genomes_test_ballistics_projectile_state)

add_executable(genomes_test_ballistics_flight
    ballistics_flight_tests.cpp)
target_link_libraries(genomes_test_ballistics_flight PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_test_ballistics_flight)
genomes_enable_test_assertions(genomes_test_ballistics_flight)
add_test(NAME ballistics.flight COMMAND genomes_test_ballistics_flight)

add_executable(genomes_test_ballistics_contact
    ballistics_contact_resolution_tests.cpp)
target_link_libraries(genomes_test_ballistics_contact PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_test_ballistics_contact)
genomes_enable_test_assertions(genomes_test_ballistics_contact)
add_test(NAME ballistics.contact COMMAND genomes_test_ballistics_contact)

add_executable(genomes_test_ballistics_he_fragmentation
    ballistics_he_fragmentation_tests.cpp)
target_link_libraries(genomes_test_ballistics_he_fragmentation PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_test_ballistics_he_fragmentation)
genomes_enable_test_assertions(genomes_test_ballistics_he_fragmentation)
add_test(NAME ballistics.he_fragmentation COMMAND genomes_test_ballistics_he_fragmentation)

add_executable(genomes_test_ballistics_batch_parity
    ballistics_batch_parity_tests.cpp)
target_link_libraries(genomes_test_ballistics_batch_parity PRIVATE genomes::ballistics)
genomes_set_project_warnings(genomes_test_ballistics_batch_parity)
genomes_enable_test_assertions(genomes_test_ballistics_batch_parity)
add_test(NAME ballistics.batch_parity COMMAND genomes_test_ballistics_batch_parity)

add_executable(genomes_test_ballistics_validation
    validation/ballistics_validation_tests.cpp)
target_link_libraries(genomes_test_ballistics_validation PRIVATE genomes::ballistics)
target_compile_definitions(genomes_test_ballistics_validation PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_ballistics_validation)
genomes_enable_test_assertions(genomes_test_ballistics_validation)
add_test(NAME ballistics.validation COMMAND genomes_test_ballistics_validation)

add_executable(genomes_test_ballistics_ammunition_catalog
    ballistics_ammunition_catalog_tests.cpp)
target_link_libraries(genomes_test_ballistics_ammunition_catalog PRIVATE genomes::ballistics)
target_compile_definitions(genomes_test_ballistics_ammunition_catalog PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_ballistics_ammunition_catalog)
genomes_enable_test_assertions(genomes_test_ballistics_ammunition_catalog)
add_test(NAME ballistics.ammunition_catalog COMMAND genomes_test_ballistics_ammunition_catalog)

add_executable(genomes_test_weapon_catalog_geometry
    weapon_catalog_geometry_tests.cpp)
target_link_libraries(genomes_test_weapon_catalog_geometry PRIVATE genomes::weapons)
target_compile_definitions(genomes_test_weapon_catalog_geometry PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_weapon_catalog_geometry)
genomes_enable_test_assertions(genomes_test_weapon_catalog_geometry)
add_test(NAME weapons.catalog_geometry COMMAND genomes_test_weapon_catalog_geometry)

add_executable(genomes_test_weapon_handling
    weapon_handling_tests.cpp)
target_link_libraries(genomes_test_weapon_handling PRIVATE genomes::weapons)
genomes_set_project_warnings(genomes_test_weapon_handling)
genomes_enable_test_assertions(genomes_test_weapon_handling)
add_test(NAME weapons.handling COMMAND genomes_test_weapon_handling)

add_executable(genomes_test_combat_command_flow
    combat_command_flow_tests.cpp)
target_link_libraries(genomes_test_combat_command_flow PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_combat_command_flow)
genomes_enable_test_assertions(genomes_test_combat_command_flow)
add_test(NAME combat.command_flow COMMAND genomes_test_combat_command_flow)

add_executable(genomes_test_perception_broadphase
    perception_broadphase_tests.cpp)
target_link_libraries(genomes_test_perception_broadphase PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_perception_broadphase)
genomes_enable_test_assertions(genomes_test_perception_broadphase)
add_test(NAME combat.perception_broadphase COMMAND genomes_test_perception_broadphase)

add_executable(genomes_test_los_batch
    los_batch_tests.cpp)
target_link_libraries(genomes_test_los_batch PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_los_batch)
genomes_enable_test_assertions(genomes_test_los_batch)
add_test(NAME combat.los_batch COMMAND genomes_test_los_batch)

add_executable(genomes_test_visibility_batch
    visibility_batch_tests.cpp)
target_link_libraries(genomes_test_visibility_batch PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_visibility_batch)
genomes_enable_test_assertions(genomes_test_visibility_batch)
add_test(NAME combat.visibility_batch COMMAND genomes_test_visibility_batch)

add_executable(genomes_test_tactical_ai
    tactical_ai_tests.cpp)
target_link_libraries(genomes_test_tactical_ai PRIVATE genomes::combat)
target_compile_definitions(genomes_test_tactical_ai PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_tactical_ai)
genomes_enable_test_assertions(genomes_test_tactical_ai)
add_test(NAME combat.tactical_ai COMMAND genomes_test_tactical_ai)

add_executable(genomes_test_squad_signal
    squad_signal_tests.cpp)
target_link_libraries(genomes_test_squad_signal PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_squad_signal)
genomes_enable_test_assertions(genomes_test_squad_signal)
add_test(NAME combat.squad_signal COMMAND genomes_test_squad_signal)

add_executable(genomes_test_ai_parallel
    ai_parallel_tests.cpp)
target_link_libraries(genomes_test_ai_parallel PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_ai_parallel)
genomes_enable_test_assertions(genomes_test_ai_parallel)
add_test(NAME combat.ai_parallel COMMAND genomes_test_ai_parallel)

add_executable(genomes_test_influence_field
    influence_field_tests.cpp)
target_link_libraries(genomes_test_influence_field PRIVATE genomes::combat)
genomes_set_project_warnings(genomes_test_influence_field)
genomes_enable_test_assertions(genomes_test_influence_field)
add_test(NAME combat.influence_field COMMAND genomes_test_influence_field)

add_executable(genomes_test_native_world_assembly
    native_world_assembly_tests.cpp)
target_link_libraries(genomes_test_native_world_assembly PRIVATE genomes::gameplay)
target_compile_definitions(genomes_test_native_world_assembly PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_native_world_assembly)
genomes_enable_test_assertions(genomes_test_native_world_assembly)
add_test(NAME integration.native_world_assembly COMMAND genomes_test_native_world_assembly)

add_executable(genomes_test_world_parity
    world_parity_tests.cpp)
target_link_libraries(genomes_test_world_parity PRIVATE genomes::gameplay)
target_compile_definitions(genomes_test_world_parity PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_world_parity)
genomes_enable_test_assertions(genomes_test_world_parity)
add_test(NAME integration.world_parity COMMAND genomes_test_world_parity)

add_executable(genomes_test_world_streaming
    world_streaming_tests.cpp)
target_link_libraries(genomes_test_world_streaming PRIVATE genomes::world_generation)
target_compile_definitions(genomes_test_world_streaming PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_world_streaming)
genomes_enable_test_assertions(genomes_test_world_streaming)
add_test(NAME integration.world_streaming COMMAND genomes_test_world_streaming)

add_executable(genomes_test_world_save
    world_save_tests.cpp)
target_link_libraries(genomes_test_world_save PRIVATE genomes::world_core)
genomes_set_project_warnings(genomes_test_world_save)
genomes_enable_test_assertions(genomes_test_world_save)
add_test(NAME integration.world_save COMMAND genomes_test_world_save)

add_executable(genomes_test_proc_viewer
    proc_viewer_smoke_tests.cpp)
target_link_libraries(genomes_test_proc_viewer PRIVATE genomes::proc_viewer_core)
target_compile_definitions(genomes_test_proc_viewer PRIVATE
    GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
genomes_set_project_warnings(genomes_test_proc_viewer)
genomes_enable_test_assertions(genomes_test_proc_viewer)
add_test(NAME tools.proc_viewer COMMAND genomes_test_proc_viewer)

add_executable(genomes_test_full_presentation
    full_presentation_smoke_tests.cpp)
target_link_libraries(genomes_test_full_presentation PRIVATE genomes::render_core)
genomes_set_project_warnings(genomes_test_full_presentation)
genomes_enable_test_assertions(genomes_test_full_presentation)
add_test(NAME render.full_presentation COMMAND genomes_test_full_presentation)

if(GENOMES_ENABLE_INFANTRY)
    add_library(genomes_test_infantry_fixture STATIC infantry_fixture_reader.cpp)
    target_include_directories(genomes_test_infantry_fixture PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/external/json/single_include")
    target_compile_features(genomes_test_infantry_fixture PUBLIC cxx_std_20)
    genomes_set_project_warnings(genomes_test_infantry_fixture)

    add_executable(genomes_test_infantry_fixture_reader infantry_fixture_reader_tests.cpp)
    target_link_libraries(genomes_test_infantry_fixture_reader PRIVATE genomes_test_infantry_fixture)
    target_compile_definitions(genomes_test_infantry_fixture_reader PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_infantry_fixture_reader)
    genomes_enable_test_assertions(genomes_test_infantry_fixture_reader)
    add_test(NAME infantry.fixture_reader COMMAND genomes_test_infantry_fixture_reader)

    add_executable(genomes_test_infantry_boundary
        infantry_boundary_tests.cpp)
    target_link_libraries(genomes_test_infantry_boundary PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_boundary)
    genomes_enable_test_assertions(genomes_test_infantry_boundary)
    add_test(NAME infantry.boundary COMMAND genomes_test_infantry_boundary)

    add_executable(genomes_test_infantry_genome_phenotype
        infantry_genome_phenotype_tests.cpp)
    target_link_libraries(genomes_test_infantry_genome_phenotype PRIVATE genomes::infantry)
    target_include_directories(genomes_test_infantry_genome_phenotype PRIVATE
        "${CMAKE_SOURCE_DIR}/external/json/single_include")
    target_compile_definitions(genomes_test_infantry_genome_phenotype PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_infantry_genome_phenotype)
    genomes_enable_test_assertions(genomes_test_infantry_genome_phenotype)
    add_test(NAME infantry.genome_phenotype COMMAND genomes_test_infantry_genome_phenotype)

    add_executable(genomes_test_infantry_rig
        infantry_rig_tests.cpp)
    target_link_libraries(genomes_test_infantry_rig PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_rig)
    genomes_enable_test_assertions(genomes_test_infantry_rig)
    add_test(NAME infantry.rig COMMAND genomes_test_infantry_rig)

    add_executable(genomes_test_infantry_appearance
        infantry_appearance_tests.cpp)
    target_link_libraries(genomes_test_infantry_appearance PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_appearance)
    genomes_enable_test_assertions(genomes_test_infantry_appearance)
    add_test(NAME infantry.appearance COMMAND genomes_test_infantry_appearance)

    add_executable(genomes_test_infantry_equipment
        infantry_equipment_tests.cpp)
    target_link_libraries(genomes_test_infantry_equipment PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_equipment)
    genomes_enable_test_assertions(genomes_test_infantry_equipment)
    add_test(NAME infantry.equipment COMMAND genomes_test_infantry_equipment)

    add_executable(genomes_test_infantry_equipment_catalog
        infantry_equipment_catalog_tests.cpp)
    target_link_libraries(genomes_test_infantry_equipment_catalog PRIVATE genomes::infantry)
    target_compile_definitions(genomes_test_infantry_equipment_catalog PRIVATE
        GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    genomes_set_project_warnings(genomes_test_infantry_equipment_catalog)
    genomes_enable_test_assertions(genomes_test_infantry_equipment_catalog)
    add_test(NAME infantry.equipment_catalog COMMAND genomes_test_infantry_equipment_catalog)
    add_test(NAME infantry.equipment_catalog_source_contract
        COMMAND ${CMAKE_COMMAND}
            -DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/infantry_equipment_catalog_guard.cmake")

    add_executable(genomes_test_infantry_locomotion
        infantry_locomotion_posture_tests.cpp)
    target_link_libraries(genomes_test_infantry_locomotion PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_locomotion)
    genomes_enable_test_assertions(genomes_test_infantry_locomotion)
    add_test(NAME infantry.locomotion COMMAND genomes_test_infantry_locomotion)

    add_executable(genomes_test_infantry_ik
        infantry_ik_ground_contact_tests.cpp)
    target_link_libraries(genomes_test_infantry_ik PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_ik)
    genomes_enable_test_assertions(genomes_test_infantry_ik)
    add_test(NAME infantry.ik_ground_contact COMMAND genomes_test_infantry_ik)

    add_executable(genomes_test_infantry_face
        infantry_face_animation_tests.cpp)
    target_link_libraries(genomes_test_infantry_face PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_face)
    genomes_enable_test_assertions(genomes_test_infantry_face)
    add_test(NAME infantry.face_animation COMMAND genomes_test_infantry_face)

    add_executable(genomes_test_infantry_animation_parallel
        infantry_animation_parallel_tests.cpp)
    target_link_libraries(genomes_test_infantry_animation_parallel PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_animation_parallel)
    genomes_enable_test_assertions(genomes_test_infantry_animation_parallel)
    add_test(NAME infantry.animation_parallel COMMAND genomes_test_infantry_animation_parallel)

    add_executable(genomes_test_infantry_animation_transition
        infantry_animation_transition_tests.cpp)
    target_link_libraries(genomes_test_infantry_animation_transition PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_animation_transition)
    genomes_enable_test_assertions(genomes_test_infantry_animation_transition)
    add_test(NAME infantry.animation_transition COMMAND genomes_test_infantry_animation_transition)

    add_executable(genomes_test_infantry_animation_weapon_overlay
        infantry_animation_weapon_overlay_tests.cpp)
    target_link_libraries(genomes_test_infantry_animation_weapon_overlay PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_animation_weapon_overlay)
    genomes_enable_test_assertions(genomes_test_infantry_animation_weapon_overlay)
    add_test(NAME infantry.animation_weapon_overlay COMMAND genomes_test_infantry_animation_weapon_overlay)

    add_executable(genomes_test_infantry_damage_ragdoll
        infantry_damage_ragdoll_tests.cpp)
    target_link_libraries(genomes_test_infantry_damage_ragdoll PRIVATE genomes::infantry)
    genomes_set_project_warnings(genomes_test_infantry_damage_ragdoll)
    genomes_enable_test_assertions(genomes_test_infantry_damage_ragdoll)
    add_test(NAME infantry.damage_ragdoll COMMAND genomes_test_infantry_damage_ragdoll)

    if(GENOMES_BUILD_INFANTRY_REFERENCE_COMPARISON)
        add_executable(genomes_test_infantry_reference_comparison
            infantry_reference_parity_tests.cpp)
        target_link_libraries(genomes_test_infantry_reference_comparison PRIVATE
            genomes::infantry genomes_test_infantry_fixture)
        target_compile_definitions(genomes_test_infantry_reference_comparison PRIVATE
            GENOMES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        genomes_set_project_warnings(genomes_test_infantry_reference_comparison)
        genomes_enable_test_assertions(genomes_test_infantry_reference_comparison)
        add_test(NAME infantry.reference_comparison
            COMMAND genomes_test_infantry_reference_comparison)
        set_tests_properties(infantry.reference_comparison PROPERTIES
            LABELS "diagnostic;reference;non-gating")
    endif()

add_executable(genomes_test_infantry_appearance_topology infantry_appearance_topology_tests.cpp)
target_link_libraries(genomes_test_infantry_appearance_topology PRIVATE genomes::infantry)
genomes_set_project_warnings(genomes_test_infantry_appearance_topology)
genomes_enable_test_assertions(genomes_test_infantry_appearance_topology)
add_test(NAME infantry.appearance_topology COMMAND genomes_test_infantry_appearance_topology)

add_executable(genomes_test_infantry_skin_usage infantry_skin_usage_tests.cpp)
target_link_libraries(genomes_test_infantry_skin_usage PRIVATE genomes::infantry)
genomes_set_project_warnings(genomes_test_infantry_skin_usage)
genomes_enable_test_assertions(genomes_test_infantry_skin_usage)
add_test(NAME infantry.skin_usage COMMAND genomes_test_infantry_skin_usage)

add_executable(genomes_test_infantry_phenotype_units infantry_phenotype_units_tests.cpp)
target_link_libraries(genomes_test_infantry_phenotype_units PRIVATE genomes::infantry)
genomes_set_project_warnings(genomes_test_infantry_phenotype_units)
genomes_enable_test_assertions(genomes_test_infantry_phenotype_units)
add_test(NAME infantry.phenotype_units COMMAND genomes_test_infantry_phenotype_units)

add_executable(genomes_test_infantry_hair_semantics infantry_hair_semantics_tests.cpp)
target_link_libraries(genomes_test_infantry_hair_semantics PRIVATE genomes::infantry)
genomes_set_project_warnings(genomes_test_infantry_hair_semantics)
genomes_enable_test_assertions(genomes_test_infantry_hair_semantics)
add_test(NAME infantry.hair_semantics COMMAND genomes_test_infantry_hair_semantics)

add_executable(genomes_test_infantry_model_compiler infantry_model_compiler_tests.cpp)
target_link_libraries(genomes_test_infantry_model_compiler PRIVATE genomes::infantry)
genomes_set_project_warnings(genomes_test_infantry_model_compiler)
genomes_enable_test_assertions(genomes_test_infantry_model_compiler)
add_test(NAME infantry.model_compiler COMMAND genomes_test_infantry_model_compiler)
endif()

add_executable(genomes_test_geometry_shared_operations
    geometry_shared_operations_tests.cpp)
target_link_libraries(genomes_test_geometry_shared_operations PRIVATE genomes::geometry)
genomes_set_project_warnings(genomes_test_geometry_shared_operations)
genomes_enable_test_assertions(genomes_test_geometry_shared_operations)
add_test(NAME geometry.shared_operations COMMAND genomes_test_geometry_shared_operations)
