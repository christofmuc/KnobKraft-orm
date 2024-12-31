message(STATUS "Integrating dynlibs and signing app bundle ${SIGN_DIRECTORY}")

include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS TRUE)
fixup_bundle("${SIGN_DIRECTORY}"  ""  "" IGNORE_ITEM "Python")
message(STATUS "Signing with '${CODESIGN_CERTIFICATE_NAME}'")
execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements "${ENTITLEMENTS_FILE}" --sign "${CODESIGN_CERTIFICATE_NAME}" "${SIGN_DIRECTORY}")

