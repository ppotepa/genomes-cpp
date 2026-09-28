add_library(genomes_ballistics STATIC
    src/Ballistics.cpp
    src/Fragmentation.cpp
    src/Detonation.cpp
    src/ProjectileState.cpp
    src/ProjectileStorage.cpp
    src/FlightIntegrator.cpp
    src/ProjectileTrace.cpp
    src/BallisticsWorld.cpp
    src/BallisticsBatch.cpp
    src/ContactResolver.cpp)

target_include_directories(genomes_ballistics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_ballistics PUBLIC genomes::foundation genomes::physics genomes::world
                                                genomes::destruction)
target_compile_features(genomes_ballistics PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_ballistics)

add_library(genomes::ballistics ALIAS genomes_ballistics)
