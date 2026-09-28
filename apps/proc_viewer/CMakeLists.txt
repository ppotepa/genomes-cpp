add_library(genomes_proc_viewer_core STATIC
    ProcViewerApp.cpp)
target_include_directories(genomes_proc_viewer_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(genomes_proc_viewer_core PUBLIC genomes::gameplay genomes::world)
genomes_set_project_warnings(genomes_proc_viewer_core)

add_library(genomes::proc_viewer_core ALIAS genomes_proc_viewer_core)

add_executable(genomes_proc_viewer main.cpp)
target_link_libraries(genomes_proc_viewer PRIVATE genomes::proc_viewer_core)
genomes_set_project_warnings(genomes_proc_viewer)
