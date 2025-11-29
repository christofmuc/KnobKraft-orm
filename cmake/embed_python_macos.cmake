if(NOT DEFINED APP_BUNDLE OR NOT DEFINED PYTHON_SOURCE OR NOT DEFINED PYTHON_VERSION)
    message(FATAL_ERROR "embed_python_macos.cmake requires APP_BUNDLE, PYTHON_SOURCE, and PYTHON_VERSION.")
endif()

if(NOT EXISTS "${PYTHON_SOURCE}")
    message(FATAL_ERROR "Python source path '${PYTHON_SOURCE}' not found. Set PYTHON_EXECUTABLE to a python.org framework install.")
endif()

set(FRAMEWORK_ROOT "${APP_BUNDLE}/Contents/Frameworks/Python.framework")
set(VERSION_DIR "${FRAMEWORK_ROOT}/Versions/${PYTHON_VERSION}")

# Ensure the destination folders exist
file(MAKE_DIRECTORY "${VERSION_DIR}")

# Copy the Python lib directory (strip __pycache__ to keep the bundle clean)
execute_process(
        COMMAND /bin/bash -c "rsync -a --delete --exclude='__pycache__' --exclude='test' --exclude='*.pyc' \"${PYTHON_SOURCE}/\" \"${VERSION_DIR}/\""
        RESULT_VARIABLE RSYNC_RESULT
)
if(NOT RSYNC_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to copy Python from '${PYTHON_SOURCE}' into '${VERSION_DIR}'")
endif()

# Create/refresh the Current symlink so GenericAdaptation finds the framework
file(MAKE_DIRECTORY "${FRAMEWORK_ROOT}/Versions")
execute_process(
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${PYTHON_VERSION}" "${FRAMEWORK_ROOT}/Versions/Current"
)
