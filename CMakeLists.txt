#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

# Target a specific MacOS version (I have no idea which version I need, so let's try with Mavericks)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X version to target for deployment")

project(KnobKraft_Orm)

#set(USE_ASIO true)

# Since we also build MacOS, we need C++ 17. Which is not a bad thing.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

# To avoid dependency on WebKit. This also came with MacOS, but as webkit is heavyweight, it is probably a good idea to turn it off for all
option(JUCE_CONFIG_JUCE_WEB_BROWSER OFF) 

# On Windows, we need to download external dependencies
IF (WIN32)
	# Include useful scripts for CMake    
	include(FetchContent REQUIRED)

    FetchContent_Declare(
	  boost
	  URL "https://boostorg.jfrog.io/artifactory/main/release/1.71.0/source/boost_1_71_0.zip"
	)
	FetchContent_MakeAvailable(boost)

    FetchContent_Declare(
	  icu
	  URL https://github.com/unicode-org/icu/releases/download/release-67-1/icu4c-67_1-Win64-MSVC2017.zip
	)
	FetchContent_MakeAvailable(icu)

	FetchContent_Declare(
		vcredist
		URL https://aka.ms/vs/16/release/vc_redist.x64.exe
		DOWNLOAD_NO_EXTRACT true
	)
	FetchContent_MakeAvailable(vcredist)
	set(VCREDIST_PATH "${CMAKE_CURRENT_BINARY_DIR}/_deps/vcredist-subbuild/vcredist-populate-prefix/src")

	IF (USE_ASIO)
		add_definitions(-DJUCE_ASIO)
		FetchContent_Declare(
		  asiosdk
		  URL https://www.steinberg.net/asiosdk
		)
		FetchContent_MakeAvailable(asiosdk)
		include_directories("${asiosdk_SOURCE_DIR}/common")
	ENDIF()

	FetchContent_Declare(
		pythonembedded
		URL https://www.python.org/ftp/python/3.8.7/python-3.8.7-embed-amd64.zip
	)
	FetchContent_MakeAvailable(pythonembedded)

	FetchContent_Declare(
		winsparkle
		URL https://github.com/vslavik/winsparkle/releases/download/v0.7.0/WinSparkle-0.7.0.zip
	)
	FetchContent_MakeAvailable(winsparkle)
	#set(WINSPARKLE_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/_deps/winsparkle-src/include")
	#set(WINSPARKLE_LIBDIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/winsparkle-src/x64/Release")
	set(WINSPARKLE_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/third_party/winsparkle/include")
	set(WINSPARKLE_LIBDIR "${CMAKE_CURRENT_LIST_DIR}/third_party/winsparkle/x64/Debug")
ELSEIF(APPLE)
	find_package(Boost REQUIRED)

	list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/icu4c")	
	find_package(ICU REQUIRED data uc)
	SET(APPLE_BOOST Boost::boost)

	# Supress a warning from juce_TargetPlatform by letting it know the build type
	# NOTE that NDEBUG is set automatically in Release
	if (NOT CMAKE_BUILD_TYPE MATCHES Release)
		add_compile_definitions(DEBUG)
	endif()
 
    
    # The JUCE font rendering is really fat on macOS, let us try to disable this flag
    add_definitions(-DJUCE_DISABLE_COREGRAPHICS_FONT_SMOOTHING)
ELSEIF(UNIX)
	# Include useful scripts for CMake
	find_package(PkgConfig REQUIRED) 
	find_package(OpenGL)

	# These calls create special `PkgConfig::<MODULE>` variables
	pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
	pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
	pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.0)
	find_package(ICU REQUIRED data uc)
ENDIF()

# We need to put our own CMake helpers on the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# RapidJson is an include-only library, so instead of bothering with their CMake files, just add the include path
set(MANUALLY_RAPID_JSON "${CMAKE_CURRENT_LIST_DIR}/third_party/rapidjson/include")

# Setup pybind11
IF(WIN32)
	message("USING PYTHON ${PYTHON_EXECUTABLE} version ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}")		
ELSE()
	set(PYTHON_EXECUTABLE "python3")
	# Pybind11 has the tendency to enable link time optimization (LTO) when it runs on Linux, but sadly I have run into multiple compiler crashes
	# both on gcc 8.5.0 and gcc 9.2.1. The hard core way to disable this is LDFLAGS=--disable-lto, but after looking at the CMakefiles from pybind11 this does archive-iterators:
	set(PYBIND11_LTO_CXX_FLAGS "")
ENDIF()
set(PYBIND11_PYTHON_VERSION 3.8)
add_subdirectory("third_party/pybind11")

# Define the list of link libraries required on Linux linking with JUCE, this must be used by any executable / module to run standalone
if(UNIX AND NOT APPLE)
	set(LINUX_JUCE_LINK_LIBRARIES
		PkgConfig::WEBKIT 
		PkgConfig::GTK 
		PkgConfig::GLEW 
		Xext 
		X11 
		pthread 
		${CMAKE_DL_LIBS} 
		freetype 
		curl 
		asound)

	# Also, as we will be building a shared module for Python, make sure GCC generates relocatable code suited for inclusion in a shared library
	add_definitions(-fPIC)
ENDIF()

# Include the SQLite wrapper for MidiKraft-database. The EXCLUDE_FROM_ALL is to prevent it from adding to the
# CPack installer on macOS.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third_party/SQLiteCpp EXCLUDE_FROM_ALL)

# Adding JUCE 6.0
add_subdirectory("third_party/JUCE" EXCLUDE_FROM_ALL)

# Adding JUCE modules
add_subdirectory("third_party/")

# Build a static library from juce 
add_library(juce-static STATIC)
target_link_libraries(juce-static
	PRIVATE
	    juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
	    juce::juce_audio_processors juce::juce_audio_utils juce::juce_core
	    juce::juce_cryptography juce::juce_data_structures juce::juce_dsp
	    juce::juce_events juce::juce_graphics juce::juce_gui_basics
	    juce::juce_gui_extra juce::juce_opengl juce::juce_video
		${LINUX_JUCE_LINK_LIBRARIES}
	PUBLIC
		juce::juce_recommended_config_flags
		juce::juce_recommended_lto_flags
		juce::juce_recommended_warning_flags)

target_compile_definitions(juce-static
	PUBLIC
		JUCE_PLUGINHOST_VST=0
		JUCE_PLUGINHOST_AU=0
		DONT_SET_USING_JUCE_NAMESPACE=1
		JUCE_DISPLAY_SPLASH_SCREEN=0
		JUCE_REPORT_APP_USAGE=0
		JUCE_CHECK_MEMORY_LEAKS=0
		JUCE_QUICKTIME=0
		JUCE_USE_DIRECTWRITE=1
		#JUCE_DIRECT2D=1  # That doesn't work, and the JUCE forum is full of confusing threads about Direct2D and font rendering
		JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
		JUCE_COREGRAPHICS_DRAW_ASYNC=1
		JUCE_WIN_PER_MONITOR_DPI_AWARE=1
		JUCE_USE_FLAC=1
		# JUCE_USE_WINRT_MIDI=1 # So much doesn't work when activating this that this is really scary
	INTERFACE
		$<TARGET_PROPERTY:juce-static,COMPILE_DEFINITIONS>)

target_include_directories(juce-static
	INTERFACE
		$<TARGET_PROPERTY:juce-static,INCLUDE_DIRECTORIES>)

set_target_properties(juce-static PROPERTIES
	POSITION_INDEPENDENT_CODE TRUE
	VISIBILITY_INLINES_HIDDEN TRUE
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden)

# Add more third party libraries
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/json)
add_subdirectory(third_party/json-schema-validator)

# Import MidiKraft infrastructure 
add_subdirectory(juce-utils)
add_subdirectory(juce-widgets)
add_subdirectory(MidiKraft-base)
add_subdirectory(MidiKraft-librarian)
add_subdirectory(MidiKraft-database)

# Import the synths currently supported
add_subdirectory(synths/MidiKraft-access-virus)
add_subdirectory(synths/MidiKraft-BCR2000)
add_subdirectory(synths/MidiKraft-kawai-k3)
add_subdirectory(synths/MidiKraft-korg-dw8000)
add_subdirectory(synths/MidiKraft-oberheim-matrix1000)
add_subdirectory(synths/MidiKraft-roland-mks50)
add_subdirectory(synths/MidiKraft-roland-mks80)
add_subdirectory(synths/MidiKraft-sequential-rev2)
add_subdirectory(synths/MidiKraft-sequential-ob6)
add_subdirectory(synths/MidiKraft-yamaha-refacedx)

# Add the generic adaptations module, that allows to define synths via Python
add_subdirectory(adaptions)

# Add the pytchirp module that exposes the synths to Python
add_subdirectory(pytschirp)

# Main module
add_subdirectory(The-Orm)



