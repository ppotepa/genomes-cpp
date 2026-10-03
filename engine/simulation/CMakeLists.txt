add_library(genomes_simulation STATIC
    src/FixedStepClock.cpp
    src/EntityStore.cpp
    src/CadenceScheduler.cpp
    src/SystemGraph.cpp
    src/SystemExecutionPlan.cpp
    src/ChunkStorage.cpp
    src/Archetype.cpp
    src/WorldEcs.cpp
    src/CommandCommit.cpp)

target_include_directories(genomes_simulation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_simulation PUBLIC genomes::foundation genomes::jobs)
target_compile_features(genomes_simulation PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_simulation)

add_library(genomes::simulation ALIAS genomes_simulation)
