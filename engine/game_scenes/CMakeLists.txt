# Application composition owns the built-in scene catalog and routing. The
# product scene implementation remains a dependency, while the neutral
# runtime target no longer compiles this catalog.
add_library(genomes_application_scenes STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/BuiltinScenes.cpp")

target_include_directories(genomes_application_scenes PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include")
target_link_libraries(genomes_application_scenes PUBLIC
    genomes::game_scenes
    genomes::simulation)
target_compile_features(genomes_application_scenes PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_application_scenes)

add_library(genomes::application_scenes ALIAS genomes_application_scenes)
