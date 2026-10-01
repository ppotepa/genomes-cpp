add_library(genomes_combat STATIC
    src/CombatSystem.cpp
    src/PerceptionBroadphase.cpp
    src/LineOfSight.cpp
    src/LineOfSightGPU.cpp
    src/TacticalAIProfile.cpp
    src/TacticalAISystem.cpp
    src/SquadSystem.cpp
    src/AIJobPipeline.cpp
    src/InfluenceFieldCPU.cpp)

target_include_directories(genomes_combat PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_combat PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_combat PUBLIC genomes::content genomes::foundation genomes::simulation genomes::weapons genomes::spatial genomes::world_core genomes::jobs)
target_compile_features(genomes_combat PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_combat)

add_library(genomes::combat ALIAS genomes_combat)
