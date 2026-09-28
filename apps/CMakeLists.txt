add_subdirectory(headless)
add_subdirectory(menu)
add_subdirectory(proc_viewer)

if(GENOMES_ENABLE_DILIGENT)
    add_subdirectory(render_smoke)
endif()

if(GENOMES_ENABLE_SDL AND GENOMES_ENABLE_DILIGENT)
    add_subdirectory(game)
endif()
