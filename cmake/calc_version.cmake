# Get the version from our sub cmakefile
execute_process(
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/gitversion.cmake"
    OUTPUT_VARIABLE PROJECT_VERSION
    ERROR_VARIABLE PROJECT_VERSION_ERROR
    RESULT_VARIABLE PROJECT_VERSION_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT PROJECT_VERSION_RESULT EQUAL 0)
    if(PROJECT_VERSION_ERROR)
        message(FATAL_ERROR "Version calculation failed: ${PROJECT_VERSION_ERROR}")
    else()
        message(FATAL_ERROR "Version calculation failed with exit code ${PROJECT_VERSION_RESULT}")
    endif()
endif()

# Strip CMake's status prefix and surrounding whitespace
string(REGEX REPLACE "^--[ \t]*" "" PROJECT_VERSION "${PROJECT_VERSION}")
string(REGEX REPLACE "^[[:space:]]+|[[:space:]]+$" "" PROJECT_VERSION "${PROJECT_VERSION}")

if(PROJECT_VERSION STREQUAL "")
    message(FATAL_ERROR "Version calculation returned an empty string")
endif()

# Emit plain text so shell captures are clean
execute_process(
    COMMAND ${CMAKE_COMMAND} -E echo "${PROJECT_VERSION}"
)

