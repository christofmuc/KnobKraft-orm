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
			juce_gui_basics
	)
ENDIF()

file(GLOB adaption_files "${CMAKE_CURRENT_LIST_DIR}/*.py")

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptions.h
	COMMAND ${CMAKE_COMMAND} -DOUTPUT="${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptions.h" -DDIR="${CMAKE_CURRENT_LIST_DIR}/*.py" -P createResources.cmake	
	DEPENDS ${adaption_files}
	COMMENT "Compiling binary resources"
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

# Define the sources for the static library
set(Sources
	BundledAdaption.cpp BundledAdaption.h
	${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptions.h
	CreateNewAdaptionDialog.cpp CreateNewAdaptionDialog.h
	GenericAdaption.cpp GenericAdaption.h
	PythonUtils.cpp PythonUtils.h
	${adaption_files}
)

set_source_files_properties(
	${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptions.h
	DEPENDS ${adaption_files}
	PROPERTIES GENERATED TRUE
)

# Setup library
add_library(knobkraft-generic-adaption ${Sources})
target_include_directories(knobkraft-generic-adaption PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(knobkraft-generic-adaption PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(knobkraft-generic-adaption pybind11::embed juce-utils midikraft-base ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(knobkraft-generic-adaption PRIVATE /W4 /WX /utf-8)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(knobkraft-generic-adaption PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
