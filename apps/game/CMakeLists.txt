add_executable(genomes_game main.cpp GameApplication.cpp)
target_link_libraries(genomes_game
    PRIVATE
        genomes::runtime
        genomes::simulation
        genomes::platform)

if(GENOMES_RENDER_BACKEND STREQUAL "THREEPP_GL")
    target_link_libraries(genomes_game PRIVATE genomes::render_threepp)
    target_compile_definitions(genomes_game PRIVATE GENOMES_GAME_THREEPP=1)
elseif(GENOMES_RENDER_BACKEND STREQUAL "DILIGENT_LEGACY")
    target_link_libraries(genomes_game PRIVATE genomes::render_diligent)
    target_compile_definitions(genomes_game PRIVATE GENOMES_GAME_DILIGENT=1)
else()
    message(FATAL_ERROR "genomes_game requires a presentation backend")
endif()

if(GENOMES_ENABLE_RMLUI)
    target_link_libraries(genomes_game PRIVATE genomes::ui_rml)
    target_compile_definitions(genomes_game PRIVATE GENOMES_HAS_RMLUI=1)
endif()
genomes_set_project_warnings(genomes_game)
