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

add_library(genomes::runtime_core ALIAS genomes_runtime_core)
