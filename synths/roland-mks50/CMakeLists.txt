#
#  Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Roland-MKS50)

# Define the sources for the static library
set(Sources
	MKS50.cpp MKS50.h
	MKS50_Parameter.cpp MKS50_Parameter.h
	MKS50_Patch.cpp MKS50_Patch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-roland-mks50 ${Sources})
target_include_directories(midikraft-roland-mks50 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES})
target_link_libraries(midikraft-roland-mks50 juce-utils midikraft-base spdlog::spdlog)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-roland-mks50 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(midikraft-roland-mks50 PRIVATE -Wall -Wextra -pedantic -Werror -Wno-unused-private-field -Wno-switch-enum -Wno-switch)
endif()
