#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Korg-DW8000)

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
	KorgDW8000.cpp KorgDW8000.h
	#KorgDW8000_BCR2000.cpp KorgDW8000_BCR2000.h
	KorgDW8000Parameter.cpp KorgDW8000Parameter.h
	KorgDW8000Patch.cpp KorgDW8000Patch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-korg-dw8000 ${Sources})
target_include_directories(midikraft-korg-dw8000 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(midikraft-korg-dw8000 PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(midikraft-korg-dw8000 juce-utils midikraft-base)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-korg-dw8000 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-korg-dw8000 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
