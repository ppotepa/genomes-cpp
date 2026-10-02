add_library(genomes_gameplay STATIC
    src/WorldScenario.cpp)

if(GENOMES_ENABLE_INFANTRY)
    target_sources(genomes_gameplay PRIVATE
        src/BattlefieldScenario.cpp
        src/BattlefieldRuntime.cpp
        src/InfantryMassBattleRuntime.cpp)
endif()

target_include_directories(genomes_gameplay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_gameplay PUBLIC
    genomes::foundation
    genomes::jobs
    genomes::world_generation
    genomes::buildings
    genomes::terrain
    genomes::simulation)
if(GENOMES_ENABLE_INFANTRY)
    target_link_libraries(genomes_gameplay PUBLIC
        genomes::spatial
        genomes::navigation
        genomes::physics
        genomes::combat
        genomes::infantry
        genomes::weapons
        genomes::ballistics
        genomes::destruction)
    target_compile_definitions(genomes_gameplay PUBLIC GENOMES_HAS_INFANTRY=1)
else()
    target_compile_definitions(genomes_gameplay PUBLIC GENOMES_HAS_INFANTRY=0)
endif()
target_compile_features(genomes_gameplay PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_gameplay)

add_library(genomes::gameplay ALIAS genomes_gameplay)
