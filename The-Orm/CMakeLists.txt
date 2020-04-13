#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraftOrm)

IF (WIN32)
	# All setup for UI software on Windows
ELSEIF(UNIX)
	# Include useful scripts for CMake
	find_package(PkgConfig REQUIRED) 
	find_package(OpenGL)

	# These calls create special `PkgConfig::<MODULE>` variables
	pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
	pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.0)
ENDIF()

find_package(JUCE REQUIRED 
	COMPONENTS 
		juce_core		
		juce_events
		juce_audio_basics
		juce_audio_devices
		juce_data_structures
		juce_audio_utils # AudioAppComponent
		juce_dsp
		juce_opengl
		juce_cryptography # MD5
)

set(SOURCES
	AutoCategorizeWindow.cpp AutoCategorizeWindow.h
	AutoDetectProgressWindow.cpp AutoDetectProgressWindow.h
	CurrentPatchDisplay.cpp CurrentPatchDisplay.h
	ImportFromSynthDialog.cpp ImportFromSynthDialog.h
	KeyboardMacroView.cpp KeyboardMacroView.h
	MacroConfig.cpp MacroConfig.h
	MainComponent.h MainComponent.cpp	
	Main.cpp
	PatchButtonPanel.cpp PatchButtonPanel.h
	PatchDiff.cpp PatchDiff.h
	PatchNameDialog.cpp PatchNameDialog.h
	PatchView.cpp PatchView.h
	SettingsView.cpp SettingsView.h
	UIModel.cpp UIModel.h
	setup.iss
	ideas.txt
	redist/agpl-3.0.txt
)

set(MIDIKRAFT_LIBRARIES
		juce-utils 
		juce-widgets 
		midikraft-base 
		midikraft-librarian 
		midikraft-database 
		midikraft-oberheim-matrix1000
		midikraft-sequential-rev2   
		midikraft-sequential-ob6  
)

add_executable(KnobKraftOrm WIN32 ${SOURCES})
target_include_directories(KnobKraftOrm INTERFACE ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${boost_SOURCE_DIR} "${CMAKE_CURRENT_LIST_DIR}/../third_party/dtl")
IF(WIN32)	
	target_link_libraries(KnobKraftOrm PRIVATE ${JUCE_LIBRARIES} ${MIDIKRAFT_LIBRARIES}
		icuuc
		SQLiteCpp
		sqlite3
		)
ELSEIF(UNIX)
	target_link_libraries(KnobKraftOrm PRIVATE 
		${JUCE_LIBRARIES} 
		PkgConfig::WEBKIT 
		PkgConfig::GTK 
		PkgConfig::ICU
		Xext 
		X11 
		pthread 
		${CMAKE_DL_LIBS} 
		freetype 
		curl 
		${MIDIKRAFT_LIBRARIES}
		SQLiteCpp
		sqlite3
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
					"${icu_SOURCE_DIR}/bin64/icuuc66.dll" "${icu_SOURCE_DIR}/bin64/icudt66.dll"
					${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
ENDIF()

IF(WIN32)
	# Windows only: Build InnoSetup installer
	find_program(INNOSETUP NAMES iscc ISCC HINTS "c:/program files (x86)/Inno Setup 6")
	if (INNOSETUP)
	else()
		message(WARNING "Didn't find InnoSetup 6 installation, skipping creation of Client installer '")
	endif()
ENDIF()

IF(WIN32)
	 # Parse Inno Setup config file
	configure_file("${CMAKE_CURRENT_LIST_DIR}/setup.iss"
				   "${CMAKE_CURRENT_BINARY_DIR}/setup.iss")
	set(no_copy $<NOT:$<CONFIG:Release>>)
	add_custom_command(
		TARGET KnobKraftOrm POST_BUILD
		COMMAND  "${CMAKE_COMMAND}" -E $<${no_copy}:echo> $<${no_copy}:"Not building Installer with "> chdir . ${INNOSETUP} "${CMAKE_CURRENT_BINARY_DIR}/setup.iss"
		DEPENDS setup.iss 
	)
ENDIF()
