#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Oberheim-Matrix1000)

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

# Define the sources for the static library
set(Sources
	Matrix1000.cpp Matrix1000.h
	#Matrix1000BCR.cpp Matrix1000BCR.h
	Matrix1000ParamDefinition.cpp Matrix1000ParamDefinition.h
	Matrix1000Patch.cpp Matrix1000Patch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-oberheim-matrix1000 ${Sources})
target_include_directories(midikraft-oberheim-matrix1000 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR})
target_link_libraries(midikraft-oberheim-matrix1000 juce-utils midikraft-base)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-oberheim-matrix1000 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-oberheim-matrix1000 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
