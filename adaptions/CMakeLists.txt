#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraft-Generic-Adaptation)

file(GLOB adaptation_files "${CMAKE_CURRENT_LIST_DIR}/*.py")

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptations.h
	COMMAND ${CMAKE_COMMAND} -DOUTPUT="${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptations.h" -DDIR="${CMAKE_CURRENT_LIST_DIR}/*.py" -P createResources.cmake	
	DEPENDS ${adaptation_files}
	COMMENT "Compiling binary resources"
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

# Define the sources for the static library
set(Sources
	BundledAdaptation.cpp BundledAdaptation.h
	${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptations.h
	CreateNewAdaptationDialog.cpp CreateNewAdaptationDialog.h
	GenericAdaptation.cpp GenericAdaptation.h
	PythonUtils.cpp PythonUtils.h
	${adaptation_files}
)

set_source_files_properties(
	${CMAKE_CURRENT_LIST_DIR}/CompiledAdaptations.h
	DEPENDS ${adaptation_files}
	PROPERTIES GENERATED TRUE
)

# Setup library
add_library(knobkraft-generic-adaptation ${Sources})
target_include_directories(knobkraft-generic-adaptation PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(knobkraft-generic-adaptation PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(knobkraft-generic-adaptation pybind11::embed juce-utils midikraft-base ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(knobkraft-generic-adaptation PRIVATE /W4 /WX /utf-8)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(knobkraft-generic-adaptation PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
