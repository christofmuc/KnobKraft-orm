#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Yamaha-RefaceDX)

# Define the sources for the static library
set(Sources
	RefaceDX.cpp RefaceDX.h
	RefaceDXDiscovery.cpp RefaceDXDiscovery.h
	RefaceDXPatch.cpp RefaceDXPatch.h          	
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-yamaha-refacedx ${Sources})
target_include_directories(midikraft-yamaha-refacedx PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(midikraft-yamaha-refacedx PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(midikraft-yamaha-refacedx juce-utils midikraft-base ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-yamaha-refacedx PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-yamaha-refacedx PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
