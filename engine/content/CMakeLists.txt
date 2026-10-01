add_library(genomes_content STATIC
    src/ContentSnapshot.cpp)

target_include_directories(genomes_content PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_content PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_content PUBLIC genomes::foundation)
target_compile_features(genomes_content PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_content)

add_library(genomes::content ALIAS genomes_content)
