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
		IF(NOT DEFINED PYTHON_EXECUTABLE)
			set(PYTHON_EXECUTABLE "	c:/python/python36/python.exe")
		ENDIF()
	ENDIF()
ELSE()
	set(PYTHON_EXECUTABLE "python3")
ENDIF()
set(PYBIND11_PYTHON_VERSION 3.6)
add_subdirectory("third_party/pybind11")

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
add_subdirectory(synths/MidiKraft-kawai-k3)
add_subdirectory(synths/MidiKraft-korg-dw8000)
add_subdirectory(synths/MidiKraft-oberheim-matrix1000)
add_subdirectory(synths/MidiKraft-sequential-rev2)
add_subdirectory(synths/MidiKraft-sequential-ob6)
add_subdirectory(synths/MidiKraft-yamaha-refacedx)

# Add the generic adaptions module, that allows to define synths via Python
add_subdirectory(adaptions)

# Main module
add_subdirectory(The-Orm)



