#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraft_Orm)

# On Windows, we need to download external dependencies
IF (WIN32)
	# Include useful scripts for CMake    
	include(FetchContent REQUIRED)

    FetchContent_Declare(
	  boost
	  URL "https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.zip"
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

	FetchContent_Declare(
		pythonembedded
		URL https://www.python.org/ftp/python/3.6.8/python-3.6.8-embed-amd64.zip
	)
	FetchContent_MakeAvailable(pythonembedded)
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

# We need a little bit support for the JUCE library, and also want to put our own CMake helpers on the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/juce-cmake/cmake" "${CMAKE_CURRENT_LIST_DIR}/cmake")

# RapidJson is an include-only library, so instead of bothering with their CMake files, just add the include path
set(MANUALLY_RAPID_JSON "${CMAKE_CURRENT_LIST_DIR}/third_party/rapidjson/include")

# Setup pybind11
IF(WIN32)
	IF(DEFINED ENV{APPVEYOR})
		# Fixed path in Appveyor Windows build machine
		set(PYTHON_EXECUTABLE "C:/Python36-x64/python.exe")
	ELSE()		
		#if (not(${PYTHON_VERSION_MAJOR} STREQUAL "3" and ${PYTHON_VERSION_MINOR} STREQUAL "6"))
		#	set(PYTHON_EXECUTABLE "c:/python/python36/python.exe")
		#ENDIF()
		IF(NOT DEFINED PYTHON_EXECUTABLE)
			set(PYTHON_EXECUTABLE "c:/python/python36/python.exe")
		ENDIF()
		message("USING PYTHON ${PYTHON_EXECUTABLE} at ${PYTHON_VERSION_MAJOR} and ${PYTHON_VERSION_MINOR}")		
	ENDIF()
ELSE()
	set(PYTHON_EXECUTABLE "python3")
ENDIF()
set(PYBIND11_PYTHON_VERSION 3.6)
add_subdirectory("third_party/pybind11")

# Define the list of link libraries required on Linux linking with JUCE, this must be used by any executable / module to run standalone
IF(UNIX)
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

# Include the SQLite wrapper for MidiKraft-database
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third_party/SQLiteCpp)

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
add_subdirectory(synths/MidiKraft-roland-mks80)
add_subdirectory(synths/MidiKraft-sequential-rev2)
add_subdirectory(synths/MidiKraft-sequential-ob6)
add_subdirectory(synths/MidiKraft-yamaha-refacedx)

# Add the generic adaptions module, that allows to define synths via Python
add_subdirectory(adaptions)

# Add the pytchirp module that exposes the synths to Python
add_subdirectory(pytschirp)

# Main module
add_subdirectory(The-Orm)



