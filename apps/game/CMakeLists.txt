add_executable(genomes_game main.cpp GameApplication.cpp)
target_link_libraries(genomes_game
    PRIVATE
        genomes::runtime
        genomes::simulation
        genomes::platform
        genomes::render_diligent)
genomes_set_project_warnings(genomes_game)
