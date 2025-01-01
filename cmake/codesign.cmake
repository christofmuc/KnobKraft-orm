message(STATUS "Integrating dynlibs and signing app bundle ${SIGN_DIRECTORY}")

# Custom Python Framework embedding
set(PYTHON_LIB_DEST_DIR "${SIGN_DIRECTORY}/Contents/Frameworks/Python.framework/Versions/3.12")

# Get a list of all .so files in the PYTHON_SOURCE directory and its subdirectories
cmake_policy(SET CMP0009 NEW)  # Do not follow symlinks
file(GLOB_RECURSE SO_FILES "${PYTHON_SOURCE}/*.so")

# First copy everything
execute_process(
#        COMMAND /bin/bash -c "mkdir -p \"${PYTHON_LIB_DEST_DIR}\" && cp -Rv \"${PYTHON_SOURCE}\" \"${PYTHON_LIB_DEST_DIR}\""
        COMMAND /bin/bash -c "mkdir -p \"${PYTHON_LIB_DEST_DIR}\" && rsync -av --exclude='__pycache__' --exclude=text \"${PYTHON_SOURCE}\" \"${PYTHON_LIB_DEST_DIR}\""
)

#file(COPY "${PYTHON_SOURCE}" DESTINATION "${PYTHON_LIB_DEST_DIR}"
#        PATTERN "*.so" EXCLUDE)

# Loop over the list of .dylib files
#[[
foreach(SO_FILE IN LISTS SO_FILES)
    # Extract the relative path of the file from PYTHON_SOURCE
    string(REPLACE "${PYTHON_SOURCE}/" "" RELATIVE_PATH ${SO_FILE})

    # Define the destination path for the file, preserving the directory structure
    set(DEST_PATH "${PYTHON_LIB_DEST_DIR}/${RELATIVE_PATH}")
    if(NOT EXISTS "${DEST_PATH}")
        # Create the destination directory (if it doesn't exist)
        message(STATUS "Copying and re-signing ${SO_FILE} to ${DEST_PATH}")
        get_filename_component(DEST_DIR ${DEST_PATH} DIRECTORY)
        file(MAKE_DIRECTORY ${DEST_DIR})
        file(COPY ${SO_FILE} DESTINATION ${DEST_DIR})

        execute_process(
                COMMAND codesign --force --timestamp --sign "${CODESIGN_CERTIFICATE_NAME}" "${DEST_PATH}"
        )
    endif()
endforeach()
]]



# Now fixup our linking and executable paths
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
# We need the IGNORE Python because of https://gitlab.kitware.com/cmake/cmake/-/issues/20165
# Patching the bundle utils could fix it: https://stackoverflow.com/questions/59415784/cmake-macos-bundleutilities-adds-python-interpreter-to-app-and-doesnt-do-fi
fixup_bundle("${SIGN_DIRECTORY}"  ""  "" IGNORE_ITEM "Python")

# Lastly, sign our executable
message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}'")
execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements "${ENTITLEMENTS_FILE}" --sign "${CODESIGN_CERTIFICATE_NAME}" "${SIGN_DIRECTORY}")
