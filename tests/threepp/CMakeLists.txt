# CPU-only header contracts. Available without SDL, a display, or threepp.
add_executable(genomes_threepp_window_contract_tests window_contract_tests.cpp)
target_include_directories(genomes_threepp_window_contract_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/platform/include")
target_link_libraries(genomes_threepp_window_contract_tests PRIVATE genomes::foundation genomes::input)
target_compile_features(genomes_threepp_window_contract_tests PRIVATE cxx_std_20)
genomes_set_project_warnings(genomes_threepp_window_contract_tests)
add_test(NAME threepp.window_contracts COMMAND genomes_threepp_window_contract_tests)
set_tests_properties(threepp.window_contracts PROPERTIES LABELS "threepp;cpu")
# Never register window/GPU probes in ordinary headless CTest. User runs G02.

if(TARGET genomes_geometry_threepp)
    add_executable(genomes_threepp_primitive_provider_tests primitive_provider_tests.cpp)
    target_link_libraries(genomes_threepp_primitive_provider_tests PRIVATE genomes::geometry_threepp)
    target_compile_features(genomes_threepp_primitive_provider_tests PRIVATE cxx_std_20)
    genomes_set_project_warnings(genomes_threepp_primitive_provider_tests)
    add_test(NAME threepp.primitive_provider COMMAND genomes_threepp_primitive_provider_tests)
    set_tests_properties(threepp.primitive_provider PROPERTIES LABELS "threepp;cpu;geometry")
endif()

if(TARGET genomes_render_threepp)
    add_executable(genomes_threepp_input_adapter_tests input_adapter_tests.cpp)
    target_link_libraries(genomes_threepp_input_adapter_tests PRIVATE genomes::render_threepp)
    target_compile_features(genomes_threepp_input_adapter_tests PRIVATE cxx_std_20)
    genomes_set_project_warnings(genomes_threepp_input_adapter_tests)
    add_test(NAME threepp.input_adapter COMMAND genomes_threepp_input_adapter_tests)
    set_tests_properties(threepp.input_adapter PROPERTIES LABELS "threepp;cpu;input")
endif()
