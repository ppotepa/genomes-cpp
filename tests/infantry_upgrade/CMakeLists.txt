add_executable(genomes_test_index_order index_order_tests.cpp)
target_link_libraries(genomes_test_index_order PRIVATE genomes::render genomes::geometry)
target_compile_definitions(genomes_test_index_order PRIVATE
    GENOMES_EXPECT_MESHOPTIMIZER=$<BOOL:${GENOMES_ENABLE_MESHOPTIMIZER}>)
genomes_set_project_warnings(genomes_test_index_order)
add_test(NAME geometry.index_order COMMAND genomes_test_index_order)
set_tests_properties(geometry.index_order PROPERTIES LABELS "cpu;infantry-upgrade")

add_test(NAME infantry_upgrade.config_boundaries
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/config_boundaries.cmake")
set_tests_properties(infantry_upgrade.config_boundaries PROPERTIES LABELS "cpu;infantry-upgrade")

if(GENOMES_ENABLE_INFANTRY)
    add_executable(genomes_test_infantry_draw_order infantry_draw_order_tests.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(genomes_test_infantry_draw_order PRIVATE genomes::runtime Threads::Threads)
    genomes_set_project_warnings(genomes_test_infantry_draw_order)
    add_test(NAME infantry.draw_order COMMAND genomes_test_infantry_draw_order)
    set_tests_properties(infantry.draw_order PROPERTIES LABELS "cpu;infantry-upgrade")
endif()

# These headers contain no Diligent SDK types: exercise ABI/material/frame/input
# contracts in a headless build without installing a graphics toolchain.
add_executable(genomes_test_diligent_restoration diligent_restoration_tests.cpp)
target_link_libraries(genomes_test_diligent_restoration PRIVATE genomes::render)
target_include_directories(genomes_test_diligent_restoration PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/render/diligent")
genomes_set_project_warnings(genomes_test_diligent_restoration)
add_test(NAME render.diligent_restoration COMMAND genomes_test_diligent_restoration)
set_tests_properties(render.diligent_restoration PROPERTIES LABELS "cpu;infantry-upgrade;diligent-restoration")

add_test(NAME render.diligent_source_contracts
    COMMAND "${CMAKE_COMMAND}" "-DGENOMES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/diligent_source_contracts.cmake")
set_tests_properties(render.diligent_source_contracts PROPERTIES LABELS "source;diligent-restoration")
