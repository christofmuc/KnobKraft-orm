#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraft-Generic-Adaption)

# Find JUCE, and add the components we need
IF(NOT DEFINED JUCE_LIBRARIES)
	SET(JUCE_DIR_HINT "${CMAKE_CURRENT_LIST_DIR}/../third_party")
	find_package(JUCE REQUIRED 
		COMPONENTS 
			juce_core		
			juce_events
			juce_audio_basics
			juce_audio_devices
			juce_data_structures
	)
ENDIF()

# Define the shipped adaptions
set(Adaptions
	 "Matrix1000.py"
	 "PioneerToraiz-AS1.py"
	 "Roland JX-8P.py"
	 "DSI Prophet 08.py"
	 "DSI Prophet 12.py"
	 "DSI Prophet 6.py"
)

file(GLOB adaption_files "${CMAKE_CURRENT_LIST_DIR}/*.py")

# Define the sources for the static library
set(Sources
	GenericAdaption.cpp GenericAdaption.h
	PythonUtils.cpp PythonUtils.h
	${Adaptions}
)

# Setup library
add_library(knobkraft-generic-adaption ${Sources})
target_include_directories(knobkraft-generic-adaption PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(knobkraft-generic-adaption PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(knobkraft-generic-adaption pybind11::embed juce-utils midikraft-base)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(knobkraft-generic-adaption PRIVATE /W4 /WX /utf-8)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(knobkraft-generic-adaption PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

# Copy to documents directory, because this is where the exectuable after installation wants them to load from
if (WIN32)
	set(ADAPTION_DIRECTORY_PROD $ENV{USERPROFILE}/Documents/KnobKraft-orm-adaptions)
else()
	set(ADAPTION_DIRECTORY_PROD $ENV{HOME}/KnobKraft-orm-adaptions)
endif()
foreach (globalFile ${adaption_files})
	get_filename_component(file ${globalFile} NAME)	
	add_custom_command(
		OUTPUT ${ADAPTION_DIRECTORY_PROD}/${file}
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${ADAPTION_DIRECTORY_PROD}/${file}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()
