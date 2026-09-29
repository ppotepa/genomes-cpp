add_library(genomes_geometry_threepp STATIC
    src/PrimitiveProvider.cpp)
target_include_directories(genomes_geometry_threepp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_geometry_threepp
    PUBLIC genomes::geometry
    PRIVATE threepp::threepp)
target_compile_features(genomes_geometry_threepp PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_geometry_threepp)
add_library(genomes::geometry_threepp ALIAS genomes_geometry_threepp)
