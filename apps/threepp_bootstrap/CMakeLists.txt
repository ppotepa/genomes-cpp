add_executable(genomes_threepp_bootstrap main.cpp)
target_link_libraries(genomes_threepp_bootstrap PRIVATE
    genomes::platform genomes::render_threepp threepp::threepp)
target_compile_features(genomes_threepp_bootstrap PRIVATE cxx_std_20)
genomes_set_project_warnings(genomes_threepp_bootstrap)
set_target_properties(genomes_threepp_bootstrap PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(genomes_threepp_bootstrap PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${CMAKE_BINARY_DIR}/bin")
endforeach()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
    RESULT_VARIABLE revision_result OUTPUT_VARIABLE GENOMES_BOOTSTRAP_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT revision_result EQUAL 0)
    set(GENOMES_BOOTSTRAP_REVISION "unknown")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" diff --quiet HEAD --
    RESULT_VARIABLE dirty_result)
set(GENOMES_BOOTSTRAP_TRACKED_STATE "dirty-or-unavailable")
if(dirty_result EQUAL 0)
    set(GENOMES_BOOTSTRAP_TRACKED_STATE "clean-at-configure")
endif()
configure_file(BuildIdentity.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/BuildIdentity.hpp" @ONLY)
target_include_directories(genomes_threepp_bootstrap PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
