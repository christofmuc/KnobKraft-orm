# Get the version from our sub cmakefile
execute_process(
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/gitversion.cmake"
    ERROR_VARIABLE PROJECT_VERSION
    ERROR_STRIP_TRAILING_WHITESPACE
)

# Cleanup output
string(REGEX REPLACE "^[[:space:]]+|[[:space:]]+$" "" PROJECT_VERSION "${PROJECT_VERSION}")
string(REGEX REPLACE "\n$" "" PROJECT_VERSION "${PROJECT_VERSION}")
message("${PROJECT_VERSION}")

