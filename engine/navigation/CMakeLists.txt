add_library(genomes_navigation STATIC
    core/src/NavigationWorld.cpp
    src/FlowFieldCPU.cpp)

target_include_directories(genomes_navigation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_navigation PUBLIC genomes::api genomes::foundation)
target_compile_features(genomes_navigation PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_navigation)

add_library(genomes::navigation ALIAS genomes_navigation)
