add_library(genomes_runtime_core STATIC
    src/SceneDirector.cpp)

target_include_directories(genomes_runtime_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_runtime_core PUBLIC
    genomes::foundation
    genomes::jobs
    genomes::camera
    genomes::render
    genomes::ui
    genomes::input
    genomes::world)
target_compile_features(genomes_runtime_core PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_runtime_core)

# Product scenes live behind an application-facing target. The source files
# remain under the runtime tree during the compatibility window so existing
# public forwarding headers and scene IDs do not change in one step.
add_library(genomes_game_scenes STATIC
    src/BattlefieldScene.cpp
    src/BuildingLabScene.cpp
    src/MainMenuScene.cpp
    src/WorldConfigScene.cpp
    src/WorldLabScene.cpp)

target_include_directories(genomes_game_scenes PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../game_scenes/include")
target_link_libraries(genomes_game_scenes PUBLIC
    genomes_runtime_core
    genomes::foundation
    genomes::camera
    genomes::render
    genomes::ui
    genomes::input
    genomes::world
    genomes::gameplay
    genomes::terrain
    genomes::simulation
    genomes::navigation
    genomes::buildings
    genomes::world_render
    genomes::combat
    genomes::physics)
if(GENOMES_ENABLE_INFANTRY)
    target_sources(genomes_game_scenes PRIVATE
        src/InfantryPresentation.cpp
        src/UnitLabScene.cpp)
    target_link_libraries(genomes_game_scenes PUBLIC genomes::infantry)
    target_compile_definitions(genomes_game_scenes PUBLIC GENOMES_HAS_INFANTRY=1)
else()
    target_compile_definitions(genomes_game_scenes PUBLIC GENOMES_HAS_INFANTRY=0)
endif()
target_compile_features(genomes_game_scenes PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_game_scenes)

# Compatibility aggregate. New composition roots may depend on the narrow
# runtime core or product-scene target explicitly; existing consumers keep
# the established genomes::runtime link contract during PR08.
add_library(genomes_runtime INTERFACE)
target_link_libraries(genomes_runtime INTERFACE genomes_game_scenes)

add_library(genomes::runtime_core ALIAS genomes_runtime_core)
add_library(genomes::game_scenes ALIAS genomes_game_scenes)
add_library(genomes::runtime ALIAS genomes_runtime)
