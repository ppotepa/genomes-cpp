add_library(genomes_jobs STATIC
    src/JobSystem.cpp)

target_include_directories(genomes_jobs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_jobs PUBLIC genomes::foundation)
target_compile_features(genomes_jobs PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_jobs)

add_library(genomes::jobs ALIAS genomes_jobs)
