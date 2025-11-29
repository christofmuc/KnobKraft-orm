if(NOT DEFINED APP_BUNDLE OR NOT DEFINED PYTHON_SOURCE OR NOT DEFINED PYTHON_VERSION)
    message(FATAL_ERROR "embed_python_macos.cmake requires APP_BUNDLE, PYTHON_SOURCE, and PYTHON_VERSION.")
endif()

if(NOT EXISTS "${PYTHON_SOURCE}")
    message(FATAL_ERROR "Python source path '${PYTHON_SOURCE}' not found. Set PYTHON_EXECUTABLE to a python.org framework install.")
endif()

set(FRAMEWORK_ROOT "${APP_BUNDLE}/Contents/Frameworks/Python.framework")
set(VERSION_DIR "${FRAMEWORK_ROOT}/Versions/${PYTHON_VERSION}")

# Basic sanity check so we only copy a framework-style layout
if(NOT EXISTS "${PYTHON_SOURCE}/Resources/Info.plist")
    message(FATAL_ERROR "Python framework resources not found at '${PYTHON_SOURCE}/Resources'. Point PYTHON_EXECUTABLE to a framework build (python.org pkg).")
endif()

function(_kk_recreate_symlink target link_name)
    if(EXISTS "${link_name}" OR IS_SYMLINK "${link_name}")
        file(REMOVE "${link_name}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${target}" "${link_name}")
endfunction()

# Ensure the destination folders exist
file(MAKE_DIRECTORY "${VERSION_DIR}")

# Copy the Python framework version directory (strip __pycache__ to keep the bundle clean)
execute_process(
        COMMAND /bin/bash -c "rsync -a --delete --exclude='__pycache__' --exclude='test' --exclude='*.pyc' \"${PYTHON_SOURCE}/\" \"${VERSION_DIR}/\""
        RESULT_VARIABLE RSYNC_RESULT
)
if(NOT RSYNC_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to copy Python from '${PYTHON_SOURCE}' into '${VERSION_DIR}'")
endif()

# Create/refresh the Current symlink so GenericAdaptation finds the framework
file(MAKE_DIRECTORY "${FRAMEWORK_ROOT}/Versions")
_kk_recreate_symlink("${PYTHON_VERSION}" "${FRAMEWORK_ROOT}/Versions/Current")
_kk_recreate_symlink("Versions/Current/Resources" "${FRAMEWORK_ROOT}/Resources")
_kk_recreate_symlink("Versions/Current/Headers" "${FRAMEWORK_ROOT}/Headers")
_kk_recreate_symlink("Versions/Current/Python" "${FRAMEWORK_ROOT}/Python")
