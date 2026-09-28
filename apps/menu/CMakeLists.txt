add_executable(genomes_menu main.cpp)
target_link_libraries(genomes_menu PRIVATE genomes::runtime genomes::simulation)
genomes_set_project_warnings(genomes_menu)
