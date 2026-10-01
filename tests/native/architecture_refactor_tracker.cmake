if(NOT DEFINED GENOMES_SOURCE_DIR)
    message(FATAL_ERROR "GENOMES_SOURCE_DIR is required")
endif()

set(tracker "${GENOMES_SOURCE_DIR}/docs/refactor/ARCHITECTURE_REFACTOR_TRACKER.md")
set(source_map "${GENOMES_SOURCE_DIR}/docs/refactor/SOURCE_MAP.md")
foreach(required_file IN ITEMS "${tracker}" "${source_map}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing architecture refactor document: ${required_file}")
    endif()
endforeach()

file(READ "${tracker}" tracker_text)

if(NOT tracker_text MATCHES "4735977aa8b839c8ef53cd7631d8f15dbc0068f1")
    message(FATAL_ERROR "Architecture tracker lost its exact baseline SHA")
endif()
if(NOT tracker_text MATCHES "55 decisions / 18 packages / 28 acceptance scenarios")
    message(FATAL_ERROR "Architecture tracker denominator declaration changed")
endif()

string(REGEX MATCHALL "\\| R[0-9][0-9][0-9] \\|" decision_rows "${tracker_text}")
list(LENGTH decision_rows decision_count)
if(NOT decision_count EQUAL 55)
    message(FATAL_ERROR "Expected 55 decision rows, found ${decision_count}")
endif()

string(REGEX MATCHALL "\n\\| PR[0-9][0-9] \\|" package_register_rows "${tracker_text}")
list(LENGTH package_register_rows package_count)
if(NOT package_count EQUAL 18)
    message(FATAL_ERROR "Expected 18 package-register rows, found ${package_count}")
endif()

string(REGEX MATCHALL "\\| T[0-9][0-9] \\|" scenario_rows "${tracker_text}")
list(LENGTH scenario_rows scenario_count)
if(NOT scenario_count EQUAL 28)
    message(FATAL_ERROR "Expected 28 acceptance scenarios, found ${scenario_count}")
endif()

foreach(required_text IN ITEMS
        "Code SHA"
        "Verification"
        "Verified SHA"
        "R001-R004 are source-confirmed"
        "JS fixtures")
    string(FIND "${tracker_text}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Architecture tracker lost required contract: ${required_text}")
    endif()
endforeach()
