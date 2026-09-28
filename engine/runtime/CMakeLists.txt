add_library(genomes_runtime STATIC
    src/BattlefieldScene.cpp
    src/BuildingLabScene.cpp
src/BuiltinScenes.cpp
    
src/MainMenuScene.cpp
src/WorldConfigScene.cpp
    src/SceneDirector.cpp)

if(GENOMES_ENABLE_INFANTRY)
    target_sources(genomes_runtime PRIVATE
        src/InfantryPresentation.cpp
        src/UnitLabScene.cpp)
endif()

target_include_directories(genomes_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_runtime PUBLIC
    genomes::foundation
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
    genomes::gameplay
    genomes::combat
    genomes::physics)
if(GENOMES_ENABLE_INFANTRY)
    target_link_libraries(genomes_runtime PUBLIC genomes::infantry)
    target_compile_definitions(genomes_runtime PUBLIC GENOMES_HAS_INFANTRY=1)
else()
    target_compile_definitions(genomes_runtime PUBLIC GENOMES_HAS_INFANTRY=0)
endif()
target_compile_features(genomes_runtime PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_runtime)

add_library(genomes::runtime ALIAS genomes_runtime)
