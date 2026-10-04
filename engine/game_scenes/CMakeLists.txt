# Product scenes and application composition live outside the neutral runtime.
# Their public headers live under the application-scene surface; only the
# neutral lifecycle headers remain under runtime/include.
add_library(genomes_game_scenes STATIC
    src/BattlefieldScene.cpp
    src/BuildingLabScene.cpp
    src/MassBattlePresentationScheduler.cpp
    src/MainMenuScene.cpp
    src/WorldConfigScene.cpp
    src/WorldLabScene.cpp)

target_include_directories(genomes_game_scenes PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_game_scenes PUBLIC
    genomes::runtime_core
    genomes::foundation
    genomes::camera
    genomes::render
    genomes::ui
    genomes::input
    genomes::world_generation
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

add_library(genomes::game_scenes ALIAS genomes_game_scenes)

# Application composition owns the built-in scene catalog and routing. It is
# deliberately separate from the scene implementation target so a neutral
# runtime consumer cannot acquire the product catalog accidentally.
add_library(genomes_application_scenes STATIC
    src/BuiltinScenes.cpp)

target_include_directories(genomes_application_scenes PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_application_scenes PUBLIC
    genomes::game_scenes
    genomes::simulation)
target_compile_features(genomes_application_scenes PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_application_scenes)

add_library(genomes::application_scenes ALIAS genomes_application_scenes)
