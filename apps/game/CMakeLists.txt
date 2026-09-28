add_executable(genomes_game main.cpp GameApplication.cpp)
target_link_libraries(genomes_game
    PRIVATE
        genomes::runtime
        genomes::simulation
        genomes::platform
        genomes::render_diligent)
if(GENOMES_ENABLE_RMLUI)
    target_link_libraries(genomes_game PRIVATE genomes::ui_rml)
    target_compile_definitions(genomes_game PRIVATE GENOMES_HAS_RMLUI=1)
endif()
genomes_set_project_warnings(genomes_game)
