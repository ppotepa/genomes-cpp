add_library(genomes_compute STATIC
    src/ComputeService.cpp
    src/BackendSelector.cpp)

target_include_directories(genomes_compute PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_compute PUBLIC genomes::foundation genomes::jobs)
target_compile_features(genomes_compute PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_compute)

add_library(genomes::compute ALIAS genomes_compute)

add_subdirectory(cuda)
