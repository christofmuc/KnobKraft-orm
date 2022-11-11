#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.20)

project(KnobKraftOrm VERSION 1.17.1)

# Append the version number to the github action environment file to be used by subsequent steps
if(DEFINED ENV{GITHUB_ENV})
file(APPEND $ENV{GITHUB_ENV} "ORM_VERSION=${KnobKraftOrm_VERSION}")
endif()

# Export this variable to be use by the Azure pipeline to know how the installers are called
SET(KNOBKRAFT_ORM_VERSION ${KnobKraftOrm_VERSION} CACHE STRING "Version of KnobKraft Orm")
# Output the weird string Azure pipeline uses to parse a value into a variable. I hope.
MESSAGE("##vso[task.setvariable variable=ormVersionNumber]${KnobKraftOrm_VERSION}")

# Append the version number to the github action environment file to be used by subsequent steps 
if(DEFINED ENV{GITHUB_ENV})
file(APPEND $ENV{GITHUB_ENV} "ORM_VERSION=${KnobKraftOrm_VERSION}")
endif()

OPTION(CRASH_REPORTING "Turn on crash reporting via Internet/Sentry")
OPTION(SENTRY_LOGGING "Turn on logging of sentry events into the log window")
set(SENTRY_DSN "Sentry DSN URL" CACHE STRING "https://YYYYYYYYYYYYYYYYYY@ZZZZZ.ingest.sentry.io/XXXX")
set(D_LOG_SENTRY "")

configure_file("version.cpp.in" "version.cpp")

IF(CRASH_REPORTING)
	message("Internet-based crash reporting is turned on, using Sentry endpoint authorization from SENTRY_DSN CMake variable")
	configure_file("${CMAKE_CURRENT_LIST_DIR}/sentry-config.h.in" "${CMAKE_CURRENT_LIST_DIR}/sentry-config.h")
	SET(SENTRY_INSTALL_PATH "${CMAKE_CURRENT_LIST_DIR}/../third_party/sentry-native/install")
	SET(SENTRY_CONFIG_HEADER sentry-config.h.in)
	SET(SENTRY_DISTRIBUTION_FILES "${SENTRY_INSTALL_PATH}/bin/sentry.dll" "${SENTRY_INSTALL_PATH}/bin/crashpad_handler.exe")
ENDIF()

IF(SPARKLE_UPDATES)
if(WIN32)
	juce_add_binary_data(CodeSigning SOURCES "${CMAKE_CURRENT_LIST_DIR}/../codesigning/dsa_pub.pem")
	SET(WINSPARKLE_DISTRIBUTION_FILES "${WINSPARKLE_LIBDIR}/WinSparkle.dll")
else()
	set(SPARKLE_FRAMEWORK_PATH ${sparkleframework_SOURCE_DIR})
	message("Using Sparkle Framework from ${SPARKLE_FRAMEWORK_PATH}")
	set(SPARKLE_LINK "-F${SPARKLE_FRAMEWORK_PATH} -framework Sparkle")
endif()
	message("Sparkle and WinSparkle updates are turned on")
ENDIF()

set(SOURCES
	AdaptationView.cpp AdaptationView.h
	AutoCategorizeWindow.cpp AutoCategorizeWindow.h
	AutoDetectProgressWindow.cpp AutoDetectProgressWindow.h
	AutoThumbnailingDialog.cpp AutoThumbnailingDialog.h
	BCR2000_Component.cpp BCR2000_Component.h
	CreateListDialog.cpp CreateListDialog.h
	CurrentPatchDisplay.cpp CurrentPatchDisplay.h
	EditCategoryDialog.cpp EditCategoryDialog.h
	ElectraOneRouter.cpp ElectraOneRouter.h
	ExportDialog.cpp ExportDialog.h
	ImportFromSynthDialog.cpp ImportFromSynthDialog.h
	KeyboardMacroView.cpp KeyboardMacroView.h
	MacroConfig.cpp MacroConfig.h
	MainComponent.h MainComponent.cpp	
	Main.cpp
	OrmLookAndFeel.cpp OrmLookAndFeel.h
	PatchButtonPanel.cpp PatchButtonPanel.h
	PatchDiff.cpp PatchDiff.h
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
	SettingsView.cpp SettingsView.h
	SetupView.cpp SetupView.h
	UIModel.cpp UIModel.h
	win_resources.rc
	version.cpp.in
	setup.iss
	ideas.txt
	${SENTRY_CONFIG_HEADER}
	redist/agpl-3.0.txt
)

if(APPLE)
	IF(SPARKLE_UPDATES)
		# Sparkle framework
		message("Adding Mac Objective-C files for Sparkle")
		set(SPARKLE_SOURCES MacSparkle.mm MacSparkle.h)
	endif()

	# Mac Icon Magic
	set(KnobKraftOrm_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.icns)
	set_source_files_properties(resources/icon_orm.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
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
		midikraft-korg-dw8000
		midikraft-oberheim-matrix1000
		midikraft-roland-mks50 
		midikraft-roland-mks80 
		midikraft-sequential-rev2   
		midikraft-sequential-ob6  
		midikraft-yamaha-refacedx
		knobkraft-generic-adaptation
		pytschirp_embedded
)

set(CMAKE_SKIP_BUILD_RPATH TRUE)
add_executable(KnobKraftOrm MACOSX_BUNDLE WIN32 ${KnobKraftOrm_ICON} ${SOURCES} ${SPARKLE_SOURCES})
target_include_directories(KnobKraftOrm 
	INTERFACE ${CMAKE_CURRENT_LIST_DIR} 
	PRIVATE ${boost_SOURCE_DIR} "${SENTRY_INSTALL_PATH}/include" "${CMAKE_CURRENT_LIST_DIR}/../third_party/dtl" "${CMAKE_CURRENT_BINARY_DIR}" "${WINSPARKLE_INCLUDE}")
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
	target_link_libraries(KnobKraftOrm PRIVATE ${JUCE_LIBRARIES} ${MIDIKRAFT_LIBRARIES}
		icuuc
		SQLiteCpp
		sqlite3
		gin
		${SENTRY_LIB}
		${SPARKLE_DEPENDENCY}
		)
ELSEIF(APPLE)
	if(SPARKLE_UPDATES)
		target_compile_options(KnobKraftOrm PRIVATE -F${SPARKLE_FRAMEWORK_PATH} -DUSE_SPARKLE)
	endif()
	target_link_libraries(KnobKraftOrm PRIVATE ${JUCE_LIBRARIES} ${MIDIKRAFT_LIBRARIES}
		ICU::data ICU::uc
		SQLiteCpp
		sqlite3
		gin
		${APPLE_BOOST}
		${SPARKLE_LINK}
		)
	SET_TARGET_PROPERTIES(KnobKraftOrm PROPERTIES
			MACOSX_BUNDLE_BUNDLE_NAME "KnobKraft Orm ${KnobKraftOrm_VERSION}"
			MACOSX_BUNDLE_ICON_FILE icon_orm.icns
			MACOSX_BUNDLE_BUNDLE_VERSION ${KnobKraftOrm_VERSION}
			#MACOSX_BUNDLE_BUNDLE_VERSION ${KnobKraftOrm_VERSION}
			#MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
			#XCODE_EMBED_FRAMEWORKS Sparkle
			#XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY TRUE
			#XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
			)
	# https://stackoverflow.com/questions/68310342/how-to-get-cmake-to-embed-a-private-framework-into-a-macos-app-bundle
	set(APP_BUNDLE_CONTENTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.app/Contents")
	set(APP_BUNDLE_FRAMEWORKS_DIR "${APP_BUNDLE_CONTENTS_DIR}/Frameworks")
	add_custom_command(TARGET ${PROJECT_NAME}
			POST_BUILD COMMAND
			install_name_tool -add_rpath "@executable_path/../Frameworks/"
			"${APP_BUNDLE_CONTENTS_DIR}/MacOS/${PROJECT_NAME}")
ELSEIF(UNIX)
	target_link_libraries(KnobKraftOrm PRIVATE 
		${JUCE_LIBRARIES} 
		${LINUX_JUCE_LINK_LIBRARIES}
		ICU::data ICU::uc
		${MIDIKRAFT_LIBRARIES}
		SQLiteCpp
		sqlite3
		gin
		)
	target_compile_options(KnobKraftOrm PRIVATE -pthread -I/usr/include/webkitgtk-4.0 -I/usr/include/gtk-3.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/gtk-3.0 -I/usr/include/gio-unix-2.0/ -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/pango-1.0 -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libpng16 -I/usr/include/libsoup-2.4 -I/usr/include/libxml2 -I/usr/include/webkitgtk-4.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include)
ENDIF()

# Use all cores
IF (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(KnobKraftOrm PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(juce-widgets PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

IF(WIN32)
	# Post build commands - copy the DLLs (Windows only I guess)
	add_custom_command(
			TARGET KnobKraftOrm POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					"${icu_SOURCE_DIR}/bin64/icuuc67.dll" "${icu_SOURCE_DIR}/bin64/icudt67.dll" 
					${SENTRY_DISTRIBUTION_FILES} ${WINSPARKLE_DISTRIBUTION_FILES}
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
	add_custom_command(
			TARGET KnobKraftOrm POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					"${PYTHON_EMBEDDED_PATH}/python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}.dll"
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
ENDIF()

# Installation - we want to copy the sequential module directory as a subdirectory to the binary output dir. Same with roland and knobkraft python modules
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/adaptions/sequential
		$<TARGET_FILE_DIR:KnobKraftOrm>/sequential
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/adaptions/roland
		$<TARGET_FILE_DIR:KnobKraftOrm>/roland
		)
add_custom_command(TARGET KnobKraftOrm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/adaptions/knobkraft
		$<TARGET_FILE_DIR:KnobKraftOrm>/knobkraft
		)
file(GLOB ADAPTATION_PYTHON_FILES "${CMAKE_SOURCE_DIR}/adaptions/*.py")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX "conftest.py")
list(FILTER ADAPTATION_PYTHON_FILES EXCLUDE REGEX "test_.*py")
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
		DEPENDS setup.iss 
	)
ENDIF()

IF(APPLE)
	set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
	IF(SPARKLE_UPDATES)
		# Make sure the private frameworks are in out binary directory
		file(COPY ${SPARKLE_FRAMEWORK_PATH}/Sparkle.framework DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/KnobKraftOrm.app/Contents/Frameworks)
	ENDIF()

	# This is supposed to build a relocatable macOS DMG installer when you specify the
    # --target package
	SET(MY_RELEASE_DIR ${CMAKE_BINARY_DIR}/macInstaller)
    SET(APPS "\${CMAKE_INSTALL_PREFIX}/KnobKraftOrm.app")
    SET(DIRS ${CMAKE_BINARY_DIR})
    #SET(DIRS ${CMAKE_BINARY_DIR} ${SPARKLE_FRAMEWORK_PATH})
    INSTALL(TARGETS KnobKraftOrm
		BUNDLE DESTINATION . COMPONENT Runtime
		RUNTIME DESTINATION bin COMPONENT Runtime
		)
	INSTALL(CODE "include(BundleUtilities)
	fixup_bundle(\"${APPS}\"  \"\"  \"${DIRS}\" IGNORE_ITEM \"Python\")
	set(BU_CHMOD_BUNDLE_ITEMS TRUE)
    " COMPONENT Runtime)
    
    # Setup CPack variables
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.png")
    set(CPACK_PACKAGE_VERSION ${KnobKraftOrm_VERSION})
    set(CPACK_PACKAGE_VENDOR "Christof Ruch Beratungs UG (haftungsbeschraenkt)")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/redist/agpl-3.0.txt")
	#set(CPACK_RESOURCE_FILE_README "${CMAKE_PROJECT_DIR}/readme.md")
    
    # Select only the dmg generator for now
    SET(CPACK_BINARY_DRAGNDROP ON)
    SET(CPACK_BINARY_TGZ OFF)
    SET(CPACK_BINARY_STGZ OFF)
    INCLUDE(CPack)
ELSE()
	INSTALL(TARGETS KnobKraftOrm knobkraft-generic-adaptation
			BUNDLE DESTINATION . COMPONENT Runtime
			RUNTIME DESTINATION bin COMPONENT Runtime
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
