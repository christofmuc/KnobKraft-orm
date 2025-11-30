message(STATUS "Integrating dynlibs and signing app bundle ${SIGN_DIRECTORY}")

# Custom Python Framework embedding
# this confused me, so I just copy all with rsync
# https://blog.glyph.im/2024/09/python-macos-framework-builds.html
if(NOT DEFINED PYTHON_VERSION)
    set(PYTHON_VERSION "3.12")
endif()
set(PYTHON_FRAMEWORK_DEST_DIR "${SIGN_DIRECTORY}/Contents/Frameworks/Python.framework/Versions/${PYTHON_VERSION}")
if(NOT EXISTS "${PYTHON_SOURCE}/Resources/Info.plist")
    message(FATAL_ERROR "Python framework resources not found at '${PYTHON_SOURCE}/Resources'. Point PYTHON_EXECUTABLE to a framework build (python.org pkg).")
endif()

function(_kk_recreate_symlink target link_name)
    if(EXISTS "${link_name}" OR IS_SYMLINK "${link_name}")
        file(REMOVE "${link_name}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${target}" "${link_name}")
endfunction()

# Remove the backslashes from the variable inserted there by passing it around too many times
string(REPLACE "\\" "" CODESIGN_CERTIFICATE_NAME "${CODESIGN_CERTIFICATE_NAME}")

# First copy everything
execute_process(
        COMMAND /bin/bash -c "mkdir -p \"${PYTHON_FRAMEWORK_DEST_DIR}\" && rsync -av --delete --exclude='__pycache__' --exclude=test --exclude=python.o \"${PYTHON_SOURCE}/\" \"${PYTHON_FRAMEWORK_DEST_DIR}/\""
)

# Make sure Python.framework/Versions/Current exists so the runtime can resolve it
get_filename_component(PYTHON_FRAMEWORK_DIR "${PYTHON_FRAMEWORK_DEST_DIR}/.." ABSOLUTE)
_kk_recreate_symlink("${PYTHON_VERSION}" "${PYTHON_FRAMEWORK_DIR}/Current")
_kk_recreate_symlink("Versions/Current/Resources" "${PYTHON_FRAMEWORK_DIR}/Resources")
_kk_recreate_symlink("Versions/Current/Headers" "${PYTHON_FRAMEWORK_DIR}/Headers")
_kk_recreate_symlink("Versions/Current/Python" "${PYTHON_FRAMEWORK_DIR}/Python")

# Now fixup our linking and executable paths
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
# We need the IGNORE Python because of https://gitlab.kitware.com/cmake/cmake/-/issues/20165
# Patching the bundle utils could fix it: https://stackoverflow.com/questions/59415784/cmake-macos-bundleutilities-adds-python-interpreter-to-app-and-doesnt-do-fi
fixup_bundle("${SIGN_DIRECTORY}"  ""  "" IGNORE_ITEM "Python")

# Sign all Python framework binaries and .so after fixup_bundle mutated install names
cmake_policy(SET CMP0009 NEW)  # Do not follow symlinks
file(GLOB_RECURSE SO_FILES "${SIGN_DIRECTORY}/*.so"
                         "${SIGN_DIRECTORY}/Contents/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/bin/*"
                         "${SIGN_DIRECTORY}/Contents/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/Python")
list(REMOVE_DUPLICATES SO_FILES)
foreach(SO_FILE IN LISTS SO_FILES)
    if(EXISTS "${SO_FILE}")
        message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}': ${SO_FILE}")
        execute_process(
                COMMAND codesign --verbose --force --timestamp --sign "${CODESIGN_CERTIFICATE_NAME}"  --keychain build.keychain "${SO_FILE}"
        )
    endif()
endforeach()

# Lastly, sign our executable
message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}'")
execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements "${ENTITLEMENTS_FILE}" --sign "${CODESIGN_CERTIFICATE_NAME}"  --keychain build.keychain "${SIGN_DIRECTORY}")
