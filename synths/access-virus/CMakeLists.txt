#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Access-Virus)

# Define the sources for the static library
set(Sources
	Virus.cpp Virus.h 
	VirusPatch.cpp VirusPatch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-access-virus ${Sources})
target_include_directories(midikraft-access-virus PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(midikraft-access-virus PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(midikraft-access-virus juce-utils midikraft-base)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-access-virus PRIVATE /W4 /WX /utf-8)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-access-virus PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
