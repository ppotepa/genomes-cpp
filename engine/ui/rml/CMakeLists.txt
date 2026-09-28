add_library(genomes_ui_rml STATIC
    RmlUiRuntime.cpp)

target_link_libraries(genomes_ui_rml
    PUBLIC
        genomes::ui
        RmlUi::RmlUi)
target_include_directories(genomes_ui_rml PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(genomes_ui_rml PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_ui_rml)

add_library(genomes::ui_rml ALIAS genomes_ui_rml)
