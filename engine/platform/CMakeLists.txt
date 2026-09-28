add_library(genomes_platform STATIC
    src/SdlPlatform.cpp)

target_link_libraries(genomes_platform
    PUBLIC
        genomes::foundation
        genomes::input
        SDL3::SDL3-static)

target_include_directories(genomes_platform
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(genomes_platform PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_platform)

add_library(genomes::platform ALIAS genomes_platform)
