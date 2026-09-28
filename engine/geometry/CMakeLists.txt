add_library(genomes_geometry STATIC
    src/MeshBuilder.cpp
    src/ProfileSurface.cpp
    src/ParametricSurface.cpp
    src/MeshValidation.cpp)

target_include_directories(genomes_geometry PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_geometry PUBLIC genomes::foundation)
target_compile_features(genomes_geometry PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_geometry)

add_library(genomes::geometry ALIAS genomes_geometry)
