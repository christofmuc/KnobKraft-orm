#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraftOrm VERSION 1.12.2)

# Export this variable to be use by the Azure pipeline to know how the installers are called
SET(KNOBKRAFT_ORM_VERSION ${KnobKraftOrm_VERSION} CACHE STRING "Version of KnobKraft Orm")

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
	CurrentPatchDisplay.cpp CurrentPatchDisplay.h
	EditCategoryDialog.cpp EditCategoryDialog.h
	ElectraOneRouter.cpp ElectraOneRouter.h
	ExportDialog.cpp ExportDialog.h
	ImportFromSynthDialog.cpp ImportFromSynthDialog.h
	KeyboardMacroView.cpp KeyboardMacroView.h
	MacroConfig.cpp MacroConfig.h
	MainComponent.h MainComponent.cpp	
	Main.cpp
	PatchButtonPanel.cpp PatchButtonPanel.h
	PatchDiff.cpp PatchDiff.h
	PatchHolderButton.cpp PatchHolderButton.h
	PatchListTree.cpp PatchListTree.h
	PatchNameDialog.cpp PatchNameDialog.h
	PatchPerSynthList.cpp PatchPerSynthList.h
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

# Mac Icon Magic
if(APPLE)
set(KnobKraftOrm_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.icns)
set_source_files_properties(resources/icon_orm.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
ENDIF()

set(MIDIKRAFT_LIBRARIES
		juce-utils 
		juce-widgets 
		midikraft-base 
		midikraft-librarian 
		midikraft-database 
		midikraft-electra-one
		midikraft-access-virus
		midikraft-behringer-bcr2000
		midikraft-kawai-k3
		midikraft-korg-dw8000
		midikraft-oberheim-matrix1000
		midikraft-roland-mks80 
		midikraft-sequential-rev2   
		midikraft-sequential-ob6  
		midikraft-yamaha-refacedx
		knobkraft-generic-adaptation
		pytschirp_embedded
)

add_executable(KnobKraftOrm MACOSX_BUNDLE WIN32 ${KnobKraftOrm_ICON} ${SOURCES})
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
	target_link_libraries(KnobKraftOrm PRIVATE ${JUCE_LIBRARIES} ${MIDIKRAFT_LIBRARIES}
		ICU::data ICU::uc
		SQLiteCpp
		sqlite3
		gin
		${APPLE_BOOST}
		)
	SET_TARGET_PROPERTIES(KnobKraftOrm PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KnobKraft Orm ${KnobKraftOrm_VERSION}"
		MACOSX_BUNDLE_ICON_FILE icon_orm.icns
		MACOSX_BUNDLE_BUNDLE_VERSION ${KnobKraftOrm_VERSION}
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
	get_filename_component(PYTHONPATH ${PYTHON_EXECUTABLE} DIRECTORY)
	add_custom_command(
			TARGET KnobKraftOrm POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					"${PYTHONPATH}/python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}.dll"
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
ENDIF()


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
    # This is supposed to build a relocatable macOS DMG installer when you specify the
    # --target package
    SET(MY_RELEASE_DIR ${CMAKE_BINARY_DIR}/macInstaller)
    SET(APPS "\${CMAKE_INSTALL_PREFIX}/KnobKraftOrm.app")
    SET(DIRS ${CMAKE_BINARY_DIR})
    INSTALL(TARGETS KnobKraftOrm
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        )
    
    INSTALL(CODE "include(BundleUtilities)
	set(BU_CHMOD_BUNDLE_ITEMS TRUE)
    fixup_bundle(\"${APPS}\"  \"\"  \"${DIRS}\" IGNORE_ITEM \"Python\")
    " COMPONENT Runtime)
    
    # Setup CPack variables
    set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_orm.png)
    set(CPACK_PACKAGE_VERSION ${KnobKraftOrm_VERSION})
    set(CPACK_PACKAGE_VENDOR "Christof Ruch Beratungs UG (haftungsbeschraenkt)")
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/redist/agpl-3.0.txt)
    
    # Select only the dmg generator for now
    SET(CPACK_BINARY_DRAGNDROP ON)
    SET(CPACK_BINARY_TGZ OFF)
    SET(CPACK_BINARY_STGZ OFF)
    INCLUDE(CPack)
ENDIF()
