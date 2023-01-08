#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Roland-MKS80)

# Define the sources for the static library
set(Sources
	MKS80.cpp MKS80.h
	MKS80_BCR2000.cpp MKS80_BCR2000.h
	MKS80_LegacyBankLoader.cpp MKS80_LegacyBankLoader.h
	MKS80_Parameter.cpp MKS80_Parameter.h
	MKS80_Patch.cpp MKS80_Patch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-roland-mks80 ${Sources})
target_include_directories(midikraft-roland-mks80 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES})
target_link_libraries(midikraft-roland-mks80 juce-utils midikraft-base midikraft-behringer-bcr2000 spdlog::spdlog)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-roland-mks80 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(midikraft-roland-mks80 PRIVATE -Wall -Wextra -Werror -Wno-switch-enum -Wno-switch -Wno-implicit-fallthrough)
endif()
