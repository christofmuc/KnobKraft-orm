#
#  Copyright (c) 2019 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Behringer-BCR2000)

# Define the sources for the static library
set(Sources
	BCR2000.cpp BCR2000.h
	BCR2000Proxy.h
	BCRDefinition.cpp BCRDefinition.h
	CCBCRDefinition.cpp CCBCRDefinition.h
	SupportedByBCR2000.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-behringer-bcr2000 ${Sources})
target_include_directories(midikraft-behringer-bcr2000 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(midikraft-behringer-bcr2000 juce-utils midikraft-base)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-behringer-bcr2000 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-sequential-rev2 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
