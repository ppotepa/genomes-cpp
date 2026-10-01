add_library(genomes_destruction STATIC
    src/MaterialAssembly.cpp
    src/ImpactSolver.cpp
    src/DamageField.cpp
    src/StructuralGraph.cpp
    src/DebrisLifecycle.cpp
    src/RubbleField.cpp
    src/RubbleCollisionCompiler.cpp
    src/DestructionPhysicsAdapter.cpp
    src/DestructionInvalidation.cpp)

target_include_directories(genomes_destruction PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_destruction PUBLIC
    genomes::foundation
    genomes::content
    genomes::proc
    genomes::physics
    genomes::world)
target_include_directories(genomes_destruction PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_compile_features(genomes_destruction PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_destruction)

add_library(genomes::destruction ALIAS genomes_destruction)
