message("Copying files from ${NO_PYCACHE_SOURCE_DIR} to ${NO_PYCACHE_DEST_DIR}")
file(COPY "${NO_PYCACHE_SOURCE_DIR}/" DESTINATION ${NO_PYCACHE_DEST_DIR}
        PATTERN "__pycache__" EXCLUDE
        )
