if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

list(APPEND CMAKE_MODULE_PATH "${GENOMES_SOURCE_DIR}/cmake")
include(GenomesFixtureManifest)

set(tracker "${GENOMES_SOURCE_DIR}/docs/refactor/ARCHITECTURE_REFACTOR_TRACKER.md")
set(source_map "${GENOMES_SOURCE_DIR}/docs/refactor/SOURCE_MAP.md")
foreach(required_file IN ITEMS "${tracker}" "${source_map}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing architecture refactor document: ${required_file}")
    endif()
endforeach()

set(presets_file "${GENOMES_SOURCE_DIR}/CMakePresets.json")
if(NOT EXISTS "${presets_file}")
    message(FATAL_ERROR "Missing CMake preset matrix: ${presets_file}")
endif()
file(READ "${presets_file}" presets_text)
string(JSON configure_count LENGTH "${presets_text}" configurePresets)
string(JSON build_count LENGTH "${presets_text}" buildPresets)
string(JSON test_count LENGTH "${presets_text}" testPresets)
set(configure_names)
set(found_infantry_off FALSE)
math(EXPR configure_last "${configure_count} - 1")
foreach(index RANGE ${configure_last})
    string(JSON preset_name GET "${presets_text}" configurePresets ${index} name)
    list(APPEND configure_names "${preset_name}")
    if(preset_name STREQUAL "headless-core-infantry-off")
        set(found_infantry_off TRUE)
    endif()
endforeach()
if(NOT found_infantry_off)
    message(FATAL_ERROR "Preset matrix is missing headless-core-infantry-off")
endif()
foreach(required_preset IN ITEMS
        dev-debug dev-release headless-core-debug headless-core-release
        toolkit-full-debug toolkit-full-release headless-core-infantry-off)
    if(NOT required_preset IN_LIST configure_names)
        message(FATAL_ERROR "Configure preset matrix is missing ${required_preset}")
    endif()
endforeach()
set(found_infantry_off_build FALSE)
math(EXPR build_last "${build_count} - 1")
foreach(index RANGE ${build_last})
    string(JSON preset_name GET "${presets_text}" buildPresets ${index} name)
    if(preset_name STREQUAL "headless-core-infantry-off")
        set(found_infantry_off_build TRUE)
    endif()
endforeach()
if(NOT found_infantry_off_build)
    message(FATAL_ERROR "Build preset matrix is missing headless-core-infantry-off")
endif()
set(found_infantry_off_test FALSE)
math(EXPR test_last "${test_count} - 1")
foreach(index RANGE ${test_last})
    string(JSON preset_name GET "${presets_text}" testPresets ${index} name)
    if(preset_name STREQUAL "headless-core-infantry-off")
        set(found_infantry_off_test TRUE)
    endif()
endforeach()
if(NOT found_infantry_off_test)
    message(FATAL_ERROR "Test preset matrix is missing headless-core-infantry-off")
endif()

genomes_validate_fixture_manifest(
    "${GENOMES_SOURCE_DIR}/reference/fixtures/manifest.json")

file(READ "${GENOMES_SOURCE_DIR}/engine/world/CMakeLists.txt" world_targets)
foreach(required_world_target IN ITEMS
        "add_library(genomes_world_core STATIC"
        "add_library(genomes_world_generation STATIC"
        "add_library(genomes::world_core ALIAS genomes_world_core"
        "add_library(genomes::world_generation ALIAS genomes_world_generation")
    string(FIND "${world_targets}" "${required_world_target}" target_position)
    if(target_position EQUAL -1)
        message(FATAL_ERROR "World target split is missing ${required_world_target}")
    endif()
endforeach()
string(REGEX MATCH "target_link_libraries\\(genomes_world_core PUBLIC([^)]*)\\)" world_core_links "${world_targets}")
if(world_core_links STREQUAL "")
    message(FATAL_ERROR "World core link closure is not declared")
endif()
foreach(required_core_dependency IN ITEMS "genomes::foundation" "genomes::proc" "genomes::io")
    if(NOT world_core_links MATCHES "${required_core_dependency}")
        message(FATAL_ERROR "World core is missing ${required_core_dependency}")
    endif()
endforeach()
if(world_core_links MATCHES "genomes(_|::)(hydrology|roads)")
    message(FATAL_ERROR "World core must not link generation hydrology/roads targets")
endif()
set(world_core_position_header
    "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world_core/WorldPosition.hpp")
set(world_compat_position_header
    "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/WorldPosition.hpp")
foreach(required_world_position_header IN ITEMS
        "${world_core_position_header}" "${world_compat_position_header}")
    if(NOT EXISTS "${required_world_position_header}")
        message(FATAL_ERROR "World coordinate boundary header is missing: ${required_world_position_header}")
    endif()
endforeach()
file(READ "${world_core_position_header}" world_core_position_text)
if(NOT world_core_position_text MATCHES "namespace genomes::world_core")
    message(FATAL_ERROR "Canonical world coordinate header lost world_core namespace")
endif()
file(READ "${world_compat_position_header}" world_compat_position_text)
if(NOT world_compat_position_text MATCHES
        "#include[ \t]+<genomes/world_core/WorldPosition\\.hpp>")
    message(FATAL_ERROR "Legacy world coordinate header must forward to world_core")
endif()
if(world_compat_position_text MATCHES "struct WorldPosition|struct RegionCoord|struct WorldCoordinateConfig")
    message(FATAL_ERROR "Legacy world coordinate header must not own coordinate definitions")
endif()

set(world_core_site_header
    "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world_core/BuildingSite.hpp")
set(world_compat_site_header
    "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/BuildingSite.hpp")
foreach(required_site_header IN ITEMS "${world_core_site_header}" "${world_compat_site_header}")
    if(NOT EXISTS "${required_site_header}")
        message(FATAL_ERROR "World site boundary header is missing: ${required_site_header}")
    endif()
endforeach()
file(READ "${world_core_site_header}" world_core_site_text)
if(NOT world_core_site_text MATCHES "namespace genomes::world_core")
    message(FATAL_ERROR "Canonical world site header lost world_core namespace")
endif()
foreach(required_site_type IN ITEMS "BuildingSiteRequest" "BuildingSiteResolution"
        "SiteAccessClass" "SiteAccessSurface")
    if(NOT world_core_site_text MATCHES "${required_site_type}")
        message(FATAL_ERROR "Canonical world site header is missing ${required_site_type}")
    endif()
endforeach()
file(READ "${world_compat_site_header}" world_compat_site_text)
if(NOT world_compat_site_text MATCHES
        "#include[ \t]+<genomes/world_core/BuildingSite\\.hpp>")
    message(FATAL_ERROR "Legacy world site header must forward to world_core")
endif()
if(world_compat_site_text MATCHES "struct BuildingSiteRequest|struct BuildingSiteResolution|enum class SiteAccess")
    message(FATAL_ERROR "Legacy world site header must not own site definitions")
endif()

foreach(world_boundary IN ITEMS
        "WorldQuery.hpp" "WorldQuerySnapshot.hpp" "DirtyBounds.hpp" "WorldSave.hpp")
    set(world_core_boundary_header
        "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world_core/${world_boundary}")
    set(world_compat_boundary_header
        "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/${world_boundary}")
    foreach(required_boundary_header IN ITEMS
            "${world_core_boundary_header}" "${world_compat_boundary_header}")
        if(NOT EXISTS "${required_boundary_header}")
            message(FATAL_ERROR "World core boundary header is missing: ${required_boundary_header}")
        endif()
    endforeach()
    file(READ "${world_core_boundary_header}" world_core_boundary_text)
    file(READ "${world_compat_boundary_header}" world_compat_boundary_text)
    if(NOT world_core_boundary_text MATCHES "namespace genomes::world_core")
        message(FATAL_ERROR "Canonical world core header lost world_core namespace: ${world_boundary}")
    endif()
    if(NOT world_compat_boundary_text MATCHES
            "#include[ \\t]+<genomes/world_core/${world_boundary}>")
        message(FATAL_ERROR "Legacy world header must forward to world_core: ${world_boundary}")
    endif()
endforeach()
file(READ "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/WorldSave.hpp" world_save_compat_text)
if(world_save_compat_text MATCHES "struct WorldSaveMetadata|class WorldSaveCodec")
    message(FATAL_ERROR "Legacy world save header must not own save definitions")
endif()
file(READ "${GENOMES_SOURCE_DIR}/engine/world/include/genomes/world/WorldQuerySnapshot.hpp" world_query_compat_text)
if(world_query_compat_text MATCHES "struct QueryCandidate|class WorldQuerySnapshot")
    message(FATAL_ERROR "Legacy world query header must not own query definitions")
endif()

file(READ "${GENOMES_SOURCE_DIR}/engine/runtime/CMakeLists.txt" runtime_targets)
foreach(required_runtime_target IN ITEMS
        "add_library(genomes_runtime_core STATIC"
        "add_library(genomes::runtime_core ALIAS genomes_runtime_core")
    string(FIND "${runtime_targets}" "${required_runtime_target}" target_position)
    if(target_position EQUAL -1)
        message(FATAL_ERROR "Runtime target split is missing ${required_runtime_target}")
    endif()
endforeach()
string(REGEX MATCH "target_link_libraries\\(genomes_runtime_core PUBLIC([^)]*)\\)" runtime_core_links "${runtime_targets}")
if(runtime_core_links STREQUAL "")
    message(FATAL_ERROR "Runtime core link closure is not declared")
endif()
if(runtime_core_links MATCHES "genomes::(gameplay|infantry|buildings|world_render|combat|physics)")
    message(FATAL_ERROR "Runtime core must not link product gameplay targets")
endif()

# R055 target closure: every in-tree world target now names the narrow core or
# generation target. The old aggregate is removed only after this source scan
# proves that no project CMake consumer still requests it.
file(GLOB_RECURSE project_cmake_files LIST_DIRECTORIES FALSE
     "${GENOMES_SOURCE_DIR}/*/CMakeLists.txt")
foreach(project_cmake IN LISTS project_cmake_files)
    if(project_cmake MATCHES "[\\/]external[\\/]|[\\/]build[\\/]")
        continue()
    endif()
    file(READ "${project_cmake}" project_cmake_text)
    if(project_cmake MATCHES "[\\/]engine[\\/]world[\\/]CMakeLists[.]txt$")
        continue()
    endif()
    if(project_cmake_text MATCHES "genomes::world([^_A-Za-z0-9]|$)")
        message(FATAL_ERROR
                "Removed genomes::world aggregate has a CMake consumer: ${project_cmake}")
    endif()
endforeach()
set(world_cmake_file "${GENOMES_SOURCE_DIR}/engine/world/CMakeLists.txt")
file(READ "${world_cmake_file}" world_cmake_text)
if(world_cmake_text MATCHES "genomes_world[ \\t\\r\\n)]" OR
   world_cmake_text MATCHES "genomes::world([^_A-Za-z0-9]|$)")
    message(FATAL_ERROR "World aggregate target must remain removed")
endif()
if(runtime_core_links MATCHES "genomes::world([^_A-Za-z0-9]|$)" OR
   runtime_core_links MATCHES "genomes::world_generation([^_A-Za-z0-9]|$)")
    message(FATAL_ERROR "Neutral runtime core must not link the world compatibility or generation targets")
endif()
if(runtime_targets MATCHES "genomes_game_scenes|src/(BattlefieldScene|BuildingLabScene|BuiltinScenes|InfantryPresentation|MainMenuScene|UnitLabScene|WorldConfigScene|WorldLabScene)\\.cpp")
    message(FATAL_ERROR "Neutral runtime target must not compile product-scene sources")
endif()
set(application_scenes_cmake "${GENOMES_SOURCE_DIR}/engine/game_scenes/CMakeLists.txt")
if(NOT EXISTS "${application_scenes_cmake}")
    message(FATAL_ERROR "Application scene composition target is missing")
endif()
file(READ "${application_scenes_cmake}" application_scenes_targets)
foreach(required_application_scene_text IN ITEMS
        "add_library(genomes_game_scenes STATIC"
        "genomes::runtime_core"
        "genomes::game_scenes"
        "add_library(genomes_application_scenes STATIC"
        "genomes::application_scenes")
    string(FIND "${application_scenes_targets}" "${required_application_scene_text}" application_scene_position)
    if(application_scene_position EQUAL -1)
        message(FATAL_ERROR "Application scene composition target lost ${required_application_scene_text}")
    endif()
endforeach()
foreach(product_scene_source IN ITEMS
        BattlefieldScene.cpp BuildingLabScene.cpp BuiltinScenes.cpp
        InfantryPresentation.cpp MainMenuScene.cpp UnitLabScene.cpp
        WorldConfigScene.cpp WorldLabScene.cpp)
    if(NOT EXISTS "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/${product_scene_source}")
        message(FATAL_ERROR "Product scene source was not relocated: ${product_scene_source}")
    endif()
    if(EXISTS "${GENOMES_SOURCE_DIR}/engine/runtime/src/${product_scene_source}")
        message(FATAL_ERROR "Product scene source remains under neutral runtime: ${product_scene_source}")
    endif()
endforeach()

file(READ "${GENOMES_SOURCE_DIR}/engine/runtime/src/SceneDirector.cpp" scene_director_source)
if(scene_director_source MATCHES "MainMenuScene|BattlefieldScene|BuildingLabScene|UnitLabScene|WorldLabScene|scene\\.|settings\\.|application\\.")
    message(FATAL_ERROR "Neutral SceneDirector still includes a product scene")
endif()
foreach(product_consumer IN ITEMS
        "${GENOMES_SOURCE_DIR}/apps/game/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/apps/menu/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/tests/native/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/tests/infantry_upgrade/CMakeLists.txt")
    file(READ "${product_consumer}" product_consumer_text)
    if(product_consumer_text MATCHES "genomes::runtime([^_A-Za-z0-9]|$)")
        message(FATAL_ERROR "Product consumer still links compatibility genomes::runtime: ${product_consumer}")
    endif()
endforeach()

# PR17 closure audit: the runtime compatibility aggregate had no in-tree
# target consumer. Keep the narrow aliases explicit and prevent the removed
# aggregate from returning through a new CMake consumer.
if(runtime_targets MATCHES "add_library\\(genomes_runtime[ \\t\\r\\n]" OR
   runtime_targets MATCHES "add_library\\(genomes::runtime[ \\t\\r\\n]")
    message(FATAL_ERROR "Runtime compatibility aggregate must stay removed")
endif()
foreach(runtime_consumer_cmake IN ITEMS
        "${GENOMES_SOURCE_DIR}/engine/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/modules/gameplay/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/apps/game/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/apps/menu/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/tests/native/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/tests/infantry_upgrade/CMakeLists.txt"
        "${GENOMES_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    file(READ "${runtime_consumer_cmake}" runtime_consumer_text)
    if(runtime_consumer_text MATCHES "genomes::runtime([^_A-Za-z0-9]|$)")
        message(FATAL_ERROR
                "Removed runtime compatibility aggregate has a CMake consumer: ${runtime_consumer_cmake}")
    endif()
endforeach()

# PR17 closure audit: CodeGraph found no project clients for these former
# runtime forwarding paths after tests moved to canonical application headers.
# Keep the deletion explicit so an obsolete bridge cannot silently return.
foreach(removed_runtime_bridge IN ITEMS
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/ApplicationCommand.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/WorldConfig.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/BuiltinScenes.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/WorldConfigScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/BattlefieldScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/MainMenuScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/BuildingLabScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/WorldLabScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/UnitLabScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/InfantryPresentation.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/UnitLabCommandParsing.hpp"
        "${GENOMES_SOURCE_DIR}/engine/runtime/include/genomes/runtime/UnitLabModelRequestGate.hpp")
    if(EXISTS "${removed_runtime_bridge}")
        message(FATAL_ERROR
                "Retired runtime forwarding header was reintroduced: ${removed_runtime_bridge}")
    endif()
endforeach()

# The production scene has one authoritative pipeline.  BattlefieldRuntime
# owns the ECS, physics and combat tick; a failed start is terminal for the
# scene and must not revive the removed scene-local graph/physics fallback.
file(READ "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BattlefieldScene.cpp"
     battlefield_scene_source)
foreach(removed_battlefield_fallback IN ITEMS
        "simulation_graph_.run"
        "simulation_graph_.compiled"
        "infantry_->fixedUpdate"
        "infantry_->stepPhysics"
        "infantry_->emitCombatEvents"
        "simulation::CommandCommitter"
        "std::make_unique<infantry::InfantrySimulation>"
        "EntityStore entities_"
        "SimplePhysicsWorld physics_"
        "GridNavigationWorld")
    if(battlefield_scene_source MATCHES "${removed_battlefield_fallback}")
        message(FATAL_ERROR
                "BattlefieldScene still contains removed legacy fallback: ${removed_battlefield_fallback}")
    endif()
endforeach()
if(NOT battlefield_scene_source MATCHES "battlefield_runtime_->fixedUpdate")
    message(FATAL_ERROR
            "BattlefieldScene does not dispatch its authoritative BattlefieldRuntime")
endif()
if(NOT battlefield_scene_source MATCHES "battlefield_runtime_->renderStates\(\)")
    message(FATAL_ERROR
            "BattlefieldScene presentation must consume BattlefieldRuntime render states")
endif()
if(battlefield_scene_source MATCHES "WeaponHandlingSystem|weapon_handling_\\.step")
    message(FATAL_ERROR
            "BattlefieldScene must not own the production weapon handling step")
endif()
if(NOT battlefield_scene_source MATCHES "weaponPoseTasks")
    message(FATAL_ERROR
            "BattlefieldScene must consume read-only BattlefieldRuntime weapon pose tasks")
endif()
if(NOT battlefield_scene_source MATCHES "simulation_failed_ = true")
    message(FATAL_ERROR
            "BattlefieldScene must fail closed when BattlefieldRuntime cannot start")
endif()

# R032 boundary: FixtureHitscan is an isolated compatibility/unit fixture. It
# must not be reachable from the production runtime or gameplay ownership
# boundary; authoritative firing is FireIntent -> FireRequest -> Ballistics ->
# ImpactEvent/DamageCommand. Keep the named adapter itself in combat for tests.
foreach(production_weapon_consumer IN ITEMS
        "${GENOMES_SOURCE_DIR}/engine/game_scenes/include/genomes/game_scenes/BattlefieldScene.hpp"
        "${GENOMES_SOURCE_DIR}/engine/game_scenes/src/BattlefieldScene.cpp"
        "${GENOMES_SOURCE_DIR}/modules/gameplay/include/genomes/gameplay/BattlefieldRuntime.hpp"
        "${GENOMES_SOURCE_DIR}/modules/gameplay/include/genomes/gameplay/BattlefieldScenario.hpp"
        "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldRuntime.cpp"
        "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldScenario.cpp")
    file(READ "${production_weapon_consumer}" production_weapon_consumer_text)
    if(production_weapon_consumer_text MATCHES "FixtureHitscan")
        message(FATAL_ERROR
                "Production battlefield path must not reference FixtureHitscan: ${production_weapon_consumer}")
    endif()
endforeach()
file(READ "${GENOMES_SOURCE_DIR}/modules/combat/include/genomes/combat/CombatSystem.hpp"
     combat_system_header)
if(NOT combat_system_header MATCHES "class FixtureHitscan")
    message(FATAL_ERROR "Named FixtureHitscan compatibility fixture was removed")
endif()

# T10/R031-R032: the runtime pipeline has one explicit physics owner and a
# declared weapon -> ballistics -> impact -> damage boundary. Infantry may
# prepare commands and consume the result, but it must not perform the
# authoritative world step on behalf of BattlefieldRuntime.
file(READ "${GENOMES_SOURCE_DIR}/modules/gameplay/src/BattlefieldRuntime.cpp"
     battlefield_runtime_source)
foreach(required_pipeline_text IN ITEMS
        "queueFire(static_cast<float>(context.fixed_dt));"
        "advanceBallistics();"
        "submitImpact(impact)"
        "commitDamage()"
        "infantry_->applyPhysicsCommands();"
        "physics_.step(static_cast<float>(context.fixed_dt));"
        "infantry_->syncPhysicsState();"
        "damage.access.resource_writes")
    string(FIND "${battlefield_runtime_source}" "${required_pipeline_text}"
           pipeline_text_position)
    if(pipeline_text_position EQUAL -1)
        message(FATAL_ERROR
                "Battlefield authoritative pipeline contract is missing: ${required_pipeline_text}")
    endif()
endforeach()
if(battlefield_runtime_source MATCHES "infantry_->stepPhysics\\(")
    message(FATAL_ERROR
            "BattlefieldScenario must own the single PhysicsWorld::step call")
endif()
string(FIND "${battlefield_runtime_source}" "infantry_->applyPhysicsCommands();"
       pipeline_apply_position)
string(FIND "${battlefield_runtime_source}"
       "physics_.step(static_cast<float>(context.fixed_dt));" pipeline_step_position)
string(FIND "${battlefield_runtime_source}" "infantry_->syncPhysicsState();"
       pipeline_sync_position)
if(pipeline_apply_position GREATER pipeline_step_position OR
   pipeline_step_position GREATER pipeline_sync_position)
    message(FATAL_ERROR "Battlefield PhysicsStep ownership order is invalid")
endif()

file(READ "${tracker}" tracker_text)

if(NOT tracker_text MATCHES "4735977aa8b839c8ef53cd7631d8f15dbc0068f1")
    message(FATAL_ERROR "Architecture tracker lost its exact baseline SHA")
endif()
if(NOT tracker_text MATCHES "55 decisions / 18 packages / 28 acceptance scenarios")
    message(FATAL_ERROR "Architecture tracker denominator declaration changed")
endif()

# Decision rows have the package ID immediately after Rxxx. Package-register
# rows also mention Rxxx (for example R052), so matching only the decision
# table shape keeps the denominator structural rather than text-fragile.
string(REGEX MATCHALL "\\| R[0-9][0-9][0-9] \\| PR[0-9][0-9] \\|" decision_rows "${tracker_text}")
list(LENGTH decision_rows decision_count)
if(NOT decision_count EQUAL 55)
    message(FATAL_ERROR "Expected 55 decision rows, found ${decision_count}")
endif()

# The final-disposition register is intentionally separate from implementation
# and verification columns. Keep one explicit, controlled disposition for each
# decision so an unresolved R row cannot hide behind CODE_READY arithmetic.
string(REGEX MATCHALL "\\| R[0-9][0-9][0-9] \\| (implemented|verified|accepted-deferred|rejected-with-reason) \\|"
       disposition_rows "${tracker_text}")
list(LENGTH disposition_rows disposition_count)
if(NOT disposition_count EQUAL 55)
    message(FATAL_ERROR
            "Expected 55 final-disposition rows, found ${disposition_count}")
endif()
if(NOT tracker_text MATCHES "## Final disposition register")
    message(FATAL_ERROR "Architecture tracker lost final disposition register")
endif()
foreach(disposition_id RANGE 1 55)
    if(disposition_id LESS 10)
        set(disposition_key "R00${disposition_id}")
    elseif(disposition_id LESS 100)
        set(disposition_key "R0${disposition_id}")
    else()
        set(disposition_key "R${disposition_id}")
    endif()
    if(NOT tracker_text MATCHES "\\| ${disposition_key} \\| (implemented|verified|accepted-deferred|rejected-with-reason) \\|")
        message(FATAL_ERROR
                "Final disposition register is missing ${disposition_key}")
    endif()
endforeach()

string(REGEX MATCHALL "\n\\| PR[0-9][0-9] \\|" package_register_rows "${tracker_text}")
list(LENGTH package_register_rows package_count)
if(NOT package_count EQUAL 18)
    message(FATAL_ERROR "Expected 18 package-register rows, found ${package_count}")
endif()

string(REGEX MATCHALL "\\| T[0-9][0-9] \\|" scenario_rows "${tracker_text}")
list(LENGTH scenario_rows scenario_count)
if(NOT scenario_count EQUAL 28)
    message(FATAL_ERROR "Expected 28 acceptance scenarios, found ${scenario_count}")
endif()

foreach(required_text IN ITEMS
        "Code SHA"
        "Verification"
        "Verified SHA"
        "R001-R004 are source-confirmed"
        "JS fixtures")
    string(FIND "${tracker_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Architecture tracker lost required contract: ${required_text}")
    endif()
endforeach()
