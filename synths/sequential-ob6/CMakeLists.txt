#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Sequential-OB6)

# Define the sources for the static library
set(Sources
	OB6.cpp OB6.h
	OB6Patch.cpp OB6Patch.h
	README.md
	LICENSE.md
	${PATCH_FILES}
)

# Setup library
add_library(midikraft-sequential-ob6 ${Sources})
target_include_directories(midikraft-sequential-ob6 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR})
target_link_libraries(midikraft-sequential-ob6 juce-utils midikraft-base midikraft-sequential-rev2 ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-sequential-ob6 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-sequential-ob6 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
