add_library(genomes_physics STATIC
    core/src/PhysicsWorld.cpp)

target_include_directories(genomes_physics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
target_link_libraries(genomes_physics PUBLIC genomes::foundation)
target_compile_features(genomes_physics PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_physics)

add_library(genomes::physics ALIAS genomes_physics)
