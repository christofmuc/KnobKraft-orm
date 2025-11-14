
find_package(Git)
if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --dirty=-dev --long
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE "PROJECT_VERSION_FULL"
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(PROJECT_VERSION_FULL "unknown")
endif()

# Break up the version, number of commits, and the dev tag into parts
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" PROJECT_VERSION ${PROJECT_VERSION_FULL})
string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.[0-9]+-([0-9]+).*" "\\1" PROJECT_TWEAK_VERSION ${PROJECT_VERSION_FULL})
string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+-g[0-9a-f]+(.*)" "\\1" PROJECT_DEV_TAG ${PROJECT_VERSION_FULL})

# If there were no new commits, PROJECT_TWEAK_VERSION will contain a hash or "-dev", so we need to set it to 0 in that case.
if(PROJECT_TWEAK_VERSION MATCHES "-g[0-9a-f]+.*")
    set(PROJECT_TWEAK_VERSION 0)
endif()

# Append the tweak version if it's not 0
if(NOT PROJECT_TWEAK_VERSION EQUAL 0)
    set(PROJECT_VERSION ${PROJECT_VERSION}.${PROJECT_TWEAK_VERSION})
endif()
cmake_policy(SET CMP0140 NEW)
message(${PROJECT_VERSION})
#return(PROPAGATE ${PROJECT_VERSION})
