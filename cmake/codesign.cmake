message(STATUS "Integrating dynlibs and signing app bundle ${SIGN_DIRECTORY}")

include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
# We need the IGNORE Python because of https://gitlab.kitware.com/cmake/cmake/-/issues/20165
# Patching the bundle utils could fix it: https://stackoverflow.com/questions/59415784/cmake-macos-bundleutilities-adds-python-interpreter-to-app-and-doesnt-do-fi
fixup_bundle("${SIGN_DIRECTORY}"  ""  "" IGNORE_ITEM "Python")
message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}'")

# Get a list of all .so files in the SIGN_DIRECTORY and its subdirectories
file(GLOB_RECURSE SO_FILES "${SIGN_DIRECTORY}/*.so")

# Loop over the list of .so files and sign each one
foreach(SO_FILE IN LISTS SO_FILES)
    execute_process(
            COMMAND codesign --force --timestamp --sign "${CODESIGN_CERTIFICATE_NAME}" "${SO_FILE}"
    )
endforeach()

# Lastly, sign our executable
execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements "${ENTITLEMENTS_FILE}" --sign "${CODESIGN_CERTIFICATE_NAME}" "${SIGN_DIRECTORY}")
