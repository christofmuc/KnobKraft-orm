#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

# Get the version from our sub cmakefile
execute_process(
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/gitversion.cmake"
    ERROR_VARIABLE PROJECT_VERSION
    ERROR_STRIP_TRAILING_WHITESPACE
)
# Cleanup output
string(REGEX REPLACE "^[[:space:]]+|[[:space:]]+$" "" PROJECT_VERSION "${PROJECT_VERSION}")
string(REGEX REPLACE "\n$" "" PROJECT_VERSION "${PROJECT_VERSION}")
message(STATUS "Project version is '${PROJECT_VERSION}'")

# Start project
project(KnobKraftOrm VERSION ${PROJECT_VERSION})

# Now you can use PROJECT_DEV_TAG in your CMake scripts or in your code to handle the "-dev" part.
# For example, you could add a preprocessor definition that your code could use:
if(PROJECT_DEV_TAG STREQUAL "-dev")
    add_definitions(-DPROJECT_DEV)
endif()

# Export this variable to be use by the Azure
# Append the version number to the github action environment file to be used by subsequent steps 
if(DEFINED ENV{GITHUB_ENV})
file(APPEND $ENV{GITHUB_ENV} "ORM_VERSION=${KnobKraftOrm_VERSION}")
endif()

OPTION(CRASH_REPORTING "Turn on crash reporting via Internet/Sentry")
OPTION(SENTRY_LOGGING "Turn on logging of sentry events into the log window")
set(SENTRY_DSN "Sentry DSN URL" CACHE STRING "https://YYYYYYYYYYYYYYYYYY@ZZZZZ.ingest.sentry.io/XXXX")
set(D_LOG_SENTRY "")
OPTION(SPARKLE_UPDATES "Turn on WinSparkle update service")

configure_file("version.cpp.in" "version.cpp")

IF(CRASH_REPORTING)
	message("Internet-based crash reporting is turned on, using Sentry endpoint authorization from SENTRY_DSN CMake variable")
	configure_file("${CMAKE_CURRENT_LIST_DIR}/sentry-config.h.in" "${CMAKE_CURRENT_LIST_DIR}/sentry-config.h")
	SET(SENTRY_INSTALL_PATH "${CMAKE_CURRENT_LIST_DIR}/../third_party/sentry-native/install")
	SET(SENTRY_CONFIG_HEADER sentry-config.h.in)
	SET(SENTRY_DISTRIBUTION_FILES "${SENTRY_INSTALL_PATH}/bin/sentry.dll" "${SENTRY_INSTALL_PATH}/bin/crashpad_handler.exe")
ENDIF()

IF(SPARKLE_UPDATES)
	message("Sparkle and WinSparkle updates are turned on, the executable will be linked against WinSparkle")
	juce_add_binary_data(CodeSigning SOURCES "${CMAKE_CURRENT_LIST_DIR}/../codesigning/dsa_pub.pem") 
	SET(WINSPARKLE_DISTRIBUTION_FILES "${WINSPARKLE_LIBDIR}/WinSparkle.dll")
ENDIF()

set(SOURCES
	AdaptationView.cpp AdaptationView.h
	AutoCategorizeWindow.cpp AutoCategorizeWindow.h
	AutoDetectProgressWindow.cpp AutoDetectProgressWindow.h
	AutoThumbnailingDialog.cpp AutoThumbnailingDialog.h
	BCR2000_Component.cpp BCR2000_Component.h
	BulkRenameDialog.cpp BulkRenameDialog.h
	CreateListDialog.cpp CreateListDialog.h
	CurrentPatchDisplay.cpp CurrentPatchDisplay.h
	EditCategoryDialog.cpp EditCategoryDialog.h
	ElectraOneRouter.cpp ElectraOneRouter.h
	ExportDialog.cpp ExportDialog.h
	ImportFromSynthDialog.cpp ImportFromSynthDialog.h
	KeyboardMacroView.cpp KeyboardMacroView.h
	LibrarianProgressWindow.h
	MacroConfig.cpp MacroConfig.h
	MainComponent.h MainComponent.cpp	
	Main.cpp
	MidiLogPanel.cpp MidiLogPanel.h
	OrmLookAndFeel.cpp OrmLookAndFeel.h
	PatchButtonPanel.cpp PatchButtonPanel.h
	PatchDiff.cpp PatchDiff.h
	PatchHistoryPanel.cpp PatchHistoryPanel.h
	PatchHolderButton.cpp PatchHolderButton.h
	PatchListTree.cpp PatchListTree.h
	PatchPerSynthList.cpp PatchPerSynthList.h
	PatchSearchComponent.cpp PatchSearchComponent.h
	PatchTextBox.cpp PatchTextBox.h
	PatchView.cpp PatchView.h
	ReceiveManualDumpWindow.cpp ReceiveManualDumpWindow.h
	RecordingView.cpp RecordingView.h
	RotaryWithLabel.cpp RotaryWithLabel.h
	ScriptedQuery.cpp ScriptedQuery.h
	SecondaryWindow.cpp SecondaryWindow.h
	SettingsView.cpp SettingsView.h
	SetupView.cpp SetupView.h
	SimplePatchGrid.cpp SimplePatchGrid.h
	SynthBankPanel.cpp SynthBankPanel.h
	UIModel.cpp UIModel.h
	UserBankFactory.cpp UserBankFactory.h
	VerticalPatchButtonList.cpp VerticalPatchButtonList.h
	win_resources.rc
	version.cpp.in
	setup.iss
	ideas.txt
	${SENTRY_CONFIG_HEADER}
	redist/agpl-3.0.txt
)

# Mac Icon Magic
if(APPLE)
set(KnobKraftOrm_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.icns)
set_source_files_properties(${KnobKraftOrm_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

ENDIF()

set(MIDIKRAFT_LIBRARIES
		juce-utils 
		juce-widgets 
		midikraft-base 
		midikraft-librarian 
		midikraft-database 
		midikraft-access-virus
		midikraft-behringer-bcr2000
		midikraft-kawai-k3
		midikraft-oberheim-matrix1000
		midikraft-roland-mks50 
		midikraft-roland-mks80 
		midikraft-sequential-rev2   
		midikraft-sequential-ob6  
		knobkraft-generic-adaptation
		pytschirp_embedded
)

add_executable(KnobKraftOrm MACOSX_BUNDLE WIN32 ${KnobKraftOrm_ICON} ${SOURCES})
target_include_directories(KnobKraftOrm INTERFACE ${CMAKE_CURRENT_LIST_DIR})
if (CRASH_REPORTING)
  target_include_directories(KnobKraftOrm SYSTEM PRIVATE "${SENTRY_INSTALL_PATH}/include")
  if (WIN32)
    target_link_directories(KnobKraftOrm PRIVATE "${SENTRY_INSTALL_PATH}/lib")
  endif()
endif()
if (SPARKLE_UPDATES AND WIN32)
  target_include_directories(KnobKraftOrm SYSTEM PRIVATE "${WINSPARKLE_INCLUDE}")
  target_link_directories(KnobKraftOrm PRIVATE "${WINSPARKLE_LIBDIR}")
endif()
target_include_directories(KnobKraftOrm SYSTEM
	PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../third_party/dtl" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(KnobKraftOrm PRIVATE JUCE_MODULE_AVAILABLE_gin_gui JUCE_MODULE_AVAILABLE_gin)
get_target_property(gin_include_dirs gin INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(KnobKraftOrm SYSTEM PRIVATE ${gin_include_dirs})
IF(WIN32)	
	IF(CRASH_REPORTING)
		IF(SENTRY_LOGGING)
			set(D_LOG_SENTRY "-DLOG_SENTRY")
		ENDIF()
		target_compile_options(KnobKraftOrm PRIVATE -DUSE_SENTRY ${D_LOG_SENTRY})
		set(SENTRY_LIB sentry)
	ENDIF()
	IF(SPARKLE_UPDATES)
		target_compile_options(KnobKraftOrm PRIVATE -DUSE_SPARKLE)
		set(SPARKLE_DEPENDENCY CodeSigning)
	ENDIF()
	target_link_directories(KnobKraftOrm PRIVATE "${SENTRY_INSTALL_PATH}/lib" "${WINSPARKLE_LIBDIR}")
	if(ASAN)
		set(ASAN_LINK_LIB clang_rt.asan_dbg_dynamic-x86_64)
		target_link_options(KnobKraftOrm PRIVATE  /wholearchive:clang_rt.asan_dbg_dynamic-x86_64.lib)
	endif()
	target_link_libraries(KnobKraftOrm PRIVATE ${JUCE_LIBRARIES} ${MIDIKRAFT_LIBRARIES}
		icuuc
		SQLiteCpp
		sqlite3
		gin
		spdlog::spdlog
		${SENTRY_LIB}
		${SPARKLE_DEPENDENCY}
		)
ELSEIF(APPLE)
	target_link_libraries(KnobKraftOrm PRIVATE
		${JUCE_LIBRARIES}
		ICU::data ICU::uc
		${MIDIKRAFT_LIBRARIES}
		SQLiteCpp
		sqlite3
		gin
		spdlog::spdlog
		pybind11::embed
		)
	SET_TARGET_PROPERTIES(KnobKraftOrm PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KnobKraft Orm ${KnobKraftOrm_VERSION}"
			MACOSX_BUNDLE_ICON_FILE icon_orm.icns
			MACOSX_BUNDLE_BUNDLE_VERSION ${KnobKraftOrm_VERSION}
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.knobkraft.orm"
			MACOSX_BUNDLE_IDENTIFIER "com.knobkraft.orm"
                )
ELSEIF(UNIX)
	target_link_libraries(KnobKraftOrm PRIVATE 
		${JUCE_LIBRARIES} 
		${LINUX_JUCE_LINK_LIBRARIES}
		ICU::data ICU::uc
		${MIDIKRAFT_LIBRARIES}
		SQLiteCpp
		sqlite3
		gin
		spdlog::spdlog
		)
        target_include_directories(KnobKraftOrm PRIVATE
            /usr/include/webkitgtk-4.0
            /usr/include/at-spi2-atk/2.0
            /usr/include/at-spi-2.0
            /usr/include/dbus-1.0
            /usr/lib/x86_64-linux-gnu/dbus-1.0/include
            /usr/include/gtk-3.0
            /usr/include/gio-unix-2.0/
            /usr/include/cairo
            /usr/include/pango-1.0
            /usr/include/harfbuzz
            /usr/include/atk-1.0
            /usr/include/pixman-1
            /usr/include/freetype2
            /usr/include/libpng16
            /usr/include/gdk-pixbuf-2.0
            /usr/include/libsoup-2.4
            /usr/include/libxml2
            /usr/include/glib-2.0
            /usr/lib/x86_64-linux-gnu/glib-2.0/include)
	target_compile_options(KnobKraftOrm PRIVATE -pthread)
ENDIF()

# Use all cores
IF (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
	# Turn off 4068 unknown pragmas
    target_compile_options(KnobKraftOrm PRIVATE /W4 /WX /we4388 /we4389 /wd4068)
else()    
	if(APPLE)
		# lots of warnings and all warnings as errors    
		target_compile_options(KnobKraftOrm PRIVATE -Wall -Wextra -pedantic -Werror -Wno-unknown-pragmas -Wno-missing-field-initializers)
	else()
		# only lots of warnings now
		target_compile_options(KnobKraftOrm PRIVATE -Wall -Wextra -pedantic -Wno-unknown-pragmas -Wno-missing-field-initializers)
	endif()
endif()

IF(WIN32)
	# Post build commands - copy the DLLs (Windows only I guess)
	add_custom_command(
			TARGET KnobKraftOrm POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					"${icu_SOURCE_DIR}/bin64/icuuc74.dll" "${icu_SOURCE_DIR}/bin64/icudt74.dll"
					${SENTRY_DISTRIBUTION_FILES} ${WINSPARKLE_DISTRIBUTION_FILES}
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
	add_custom_command(
			TARGET KnobKraftOrm POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					"${PYTHON_EMBEDDED_PATH}/python${PYTHON_VERSION_TO_COPY}.dll"
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
ENDIF()

# Installation - we want to copy the sequential module directory as a subdirectory to the binary output dir. Same with roland and knobkraft python modules
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-DNO_PYCACHE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/adaptations/sequential
		-DNO_PYCACHE_DEST_DIR=$<TARGET_FILE_DIR:KnobKraftOrm>/sequential
		-P ${CMAKE_SOURCE_DIR}/cmake/copy_without_pycache.cmake
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-DNO_PYCACHE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/adaptations/roland
		-DNO_PYCACHE_DEST_DIR=$<TARGET_FILE_DIR:KnobKraftOrm>/roland
		-P ${CMAKE_SOURCE_DIR}/cmake/copy_without_pycache.cmake
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-DNO_PYCACHE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/adaptations/knobkraft
		-DNO_PYCACHE_DEST_DIR=$<TARGET_FILE_DIR:KnobKraftOrm>/knobkraft
		-P ${CMAKE_SOURCE_DIR}/cmake/copy_without_pycache.cmake
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-DNO_PYCACHE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/adaptations/testing
		-DNO_PYCACHE_DEST_DIR=$<TARGET_FILE_DIR:KnobKraftOrm>/testing
		-P ${CMAKE_SOURCE_DIR}/cmake/copy_without_pycache.cmake
		)
file(GLOB ADAPTATION_PYTHON_FILES "${CMAKE_SOURCE_DIR}/adaptations/*.py")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX "conftest.py")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX "test_.*py")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX ".*__pycache__/.*")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX "DSI_OB6.py")
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory 
		$<TARGET_FILE_DIR:KnobKraftOrm>/adaptations
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${ADAPTATION_PYTHON_FILES}
		$<TARGET_FILE_DIR:KnobKraftOrm>/adaptations
		)

IF(WIN32)
    # We need to copy the runtime of the embedded Python used, else we get weird errors starting from Python 3.12, probably because it finds
    # other versions on the drive.
    add_custom_command(TARGET KnobKraftOrm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${pythonembedded_SOURCE_DIR}"
                "$<TARGET_FILE_DIR:KnobKraftOrm>"
        COMMENT "Copying embedded Python runtime"
    )

    # Windows only: Build InnoSetup installer
	find_program(INNOSETUP NAMES iscc ISCC HINTS "${CMAKE_CURRENT_LIST_DIR}/../third_party/Tools.InnoSetup.6.0.5/tools" "c:/program files (x86)/Inno Setup 6")
	if (INNOSETUP)
	else()
		message(WARNING "Didn't find InnoSetup 6 installation, skipping creation of Client installer '")
	endif()
ENDIF()

IF(WIN32)
	 # Parse Inno Setup config file
	configure_file("${CMAKE_CURRENT_LIST_DIR}/setup.iss"
				   "${CMAKE_CURRENT_BINARY_DIR}/setup.iss")
	set(no_copy $<NOT:$<CONFIG:RelWithDebInfo>>)
	add_custom_command(
		TARGET KnobKraftOrm POST_BUILD
		COMMAND  "${CMAKE_COMMAND}" -E $<${no_copy}:echo> $<${no_copy}:"Not building Installer with "> chdir . ${INNOSETUP} "${CMAKE_CURRENT_BINARY_DIR}/setup.iss"
	)
ENDIF()

IF(APPLE)
	# This section is supposed to build a relocatable macOS DMG installer when you specify the
	# --target package

	# For local unsigned builds, still embed Python so the app can find its runtime.
	IF(NOT CODESIGN_CERTIFICATE_NAME)
		add_custom_command(
				TARGET KnobKraftOrm POST_BUILD
				COMMAND "${CMAKE_COMMAND}"
					-DPYTHON_EXECUTABLE="${PYTHON_EXECUTABLE}"
					-DPYTHON_VERSION="${PYTHON_VERSION_TO_EMBED}"
					-DAPP_BUNDLE="$<TARGET_BUNDLE_DIR:KnobKraftOrm>"
					-P "${CMAKE_SOURCE_DIR}/cmake/embed_python_macos.cmake"
		)
	ENDIF()

	IF(CODESIGN_CERTIFICATE_NAME)
		# We need to sign the files as a post build step. Doing this as install step doesn't help because
		# CPack will already have created a copy and ignore our
		get_filename_component(PYTHON_SOURCE_DIR ${PYTHON_EXECUTABLE} DIRECTORY)
		add_custom_command(
				TARGET KnobKraftOrm POST_BUILD
				COMMAND "${CMAKE_COMMAND}"
					-DPYTHON_SOURCE="${PYTHON_SOURCE_DIR}/../lib"
					-DPYTHON_VERSION="${PYTHON_VERSION_TO_EMBED}"
					-DSIGN_DIRECTORY=$<TARGET_BUNDLE_DIR:KnobKraftOrm>
					-DENTITLEMENTS_FILE="${CMAKE_CURRENT_LIST_DIR}/Codesign.entitlements"
					-DCODESIGN_CERTIFICATE_NAME="${CODESIGN_CERTIFICATE_NAME}"
					-P "${CMAKE_SOURCE_DIR}/cmake/codesign.cmake"
		)
	ENDIF()

	# This is the one and most important INSTALL directive, without it there is no Orm
	INSTALL(TARGETS KnobKraftOrm BUNDLE DESTINATION .)

    # Setup CPack variables
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.png")
    set(CPACK_PACKAGE_VERSION ${KnobKraftOrm_VERSION})
    set(CPACK_PACKAGE_VENDOR "Christof Ruch")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/redist/agpl-3.0.txt")
	#set(CPACK_RESOURCE_FILE_README "${CMAKE_PROJECT_DIR}/readme.md")
	set(CPACK_COMMAND_HDIUTIL "${CMAKE_CURRENT_LIST_DIR}/hdiutil_repeat.sh")
    
    # Select only the dmg generator for now
    SET(CPACK_BINARY_DRAGNDROP ON)
    SET(CPACK_BINARY_TGZ OFF)
    SET(CPACK_BINARY_STGZ OFF)
    INCLUDE(CPack)
ELSE()
	INSTALL(TARGETS KnobKraftOrm knobkraft-generic-adaptation
			BUNDLE DESTINATION .
			)
	IF(UNIX)
		SET(CPACK_BINARY_TGZ ON)
		set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.png")
		set(CPACK_PACKAGE_VERSION ${KnobKraftOrm_VERSION})
		set(CPACK_PACKAGE_VENDOR "Christof Ruch Beratungs UG (haftungsbeschraenkt)")
		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/redist/agpl-3.0.txt")
		INCLUDE(CPack)
	ENDIF()
ENDIF()
