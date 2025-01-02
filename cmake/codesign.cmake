message(STATUS "Integrating dynlibs and signing app bundle ${SIGN_DIRECTORY}")

# Custom Python Framework embedding
# this confused me, so I just copy all with rsync
# https://blog.glyph.im/2024/09/python-macos-framework-builds.html
set(PYTHON_LIB_DEST_DIR "${SIGN_DIRECTORY}/Contents/Frameworks/Python.framework/Versions/3.12")

# Remove the backslashes from the variable inserted there by passing it around too many times
string(REPLACE "\\" "" CODESIGN_CERTIFICATE_NAME "${CODESIGN_CERTIFICATE_NAME}")

# First copy everything
execute_process(
        COMMAND /bin/bash -c "mkdir -p \"${PYTHON_LIB_DEST_DIR}\" && rsync -av --exclude='__pycache__' --exclude=test --exclude=python.o \"${PYTHON_SOURCE}\" \"${PYTHON_LIB_DEST_DIR}\""
)

# Get a list of all .so files in the PYTHON_SOURCE directory and its subdirectories
cmake_policy(SET CMP0009 NEW)  # Do not follow symlinks
file(GLOB_RECURSE SO_FILES "${SIGN_DIRECTORY}/*.so")
foreach(SO_FILE IN LISTS SO_FILES)
    message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}': ${SO_FILE}")
    execute_process(
            COMMAND codesign --verbose --force --timestamp --sign "${CODESIGN_CERTIFICATE_NAME}" "${SO_FILE}"
    )
endforeach()

# Now fixup our linking and executable paths
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
# We need the IGNORE Python because of https://gitlab.kitware.com/cmake/cmake/-/issues/20165
# Patching the bundle utils could fix it: https://stackoverflow.com/questions/59415784/cmake-macos-bundleutilities-adds-python-interpreter-to-app-and-doesnt-do-fi
fixup_bundle("${SIGN_DIRECTORY}"  ""  "" IGNORE_ITEM "Python")

# Lastly, sign our executable
message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}'")
execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements "${ENTITLEMENTS_FILE}" --sign "${CODESIGN_CERTIFICATE_NAME}" "${SIGN_DIRECTORY}")
