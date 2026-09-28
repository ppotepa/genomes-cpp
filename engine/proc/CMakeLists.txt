add_library(genomes_proc STATIC
    src/Seed.cpp
    src/GeneratorRegistry.cpp
    src/ArtifactCache.cpp)

target_include_directories(genomes_proc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_proc PUBLIC genomes::foundation genomes::jobs)
target_compile_features(genomes_proc PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_proc)

add_library(genomes::proc ALIAS genomes_proc)
