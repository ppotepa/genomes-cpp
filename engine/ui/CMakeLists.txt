add_library(genomes_ui STATIC
    src/UiRuntime.cpp
    src/UiContentRegistry.cpp
    src/UiNativePluginManager.cpp)
target_link_libraries(genomes_ui PUBLIC genomes::foundation PRIVATE genomes::content ${CMAKE_DL_LIBS})
target_include_directories(genomes_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(genomes_ui PUBLIC cxx_std_20)
target_include_directories(genomes_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/json/single_include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../input/include")
genomes_set_project_warnings(genomes_ui)

add_library(genomes::ui ALIAS genomes_ui)
