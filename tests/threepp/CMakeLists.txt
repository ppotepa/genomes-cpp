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
