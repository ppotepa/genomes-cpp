add_executable(genomes_menu main.cpp)
target_link_libraries(genomes_menu PRIVATE genomes::game_scenes genomes::simulation)
genomes_set_project_warnings(genomes_menu)
