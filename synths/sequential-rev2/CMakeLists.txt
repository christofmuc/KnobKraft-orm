#
#  Copyright (c) 2019 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Sequential-Prophet-Rev2)

set(PATCH_FILES
	resources/Rev2_InitPatch.syx
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/BinaryResources.h
	COMMAND ${CMAKE_COMMAND} -P createResources.cmake	
	DEPENDS ${PATCH_FILES}
	COMMENT "Compiling binary resources"
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

# Define the sources for the static library
set(Sources
	BinaryResources.h
	DSI.cpp DSI.h	
	Rev2.cpp Rev2.h
	#Rev2BCR2000.cpp Rev2BCR2000.h
	#Rev2ButtonStrip.cpp Rev2ButtonStrip.h	
	Rev2ParamDefinition.cpp Rev2ParamDefinition.h
	Rev2Patch.cpp Rev2Patch.h
	README.md
	LICENSE.md
	${PATCH_FILES}
)

set_source_files_properties(
	BinaryResources.h
	PROPERTIES GENERATED TRUE
)

# Setup library
add_library(midikraft-sequential-rev2 ${Sources})
target_include_directories(midikraft-sequential-rev2 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(midikraft-sequential-rev2 juce-utils midikraft-base spdlog::spdlog)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-sequential-rev2 PRIVATE /W4 /WX)
else()
	if(UNIX AND NOT APPLE)
		# lots of warnings and all warnings as errors
		target_compile_options(midikraft-sequential-rev2 PRIVATE -Wall -Wextra -pedantic -Werror -Wno-implicit-fallthrough)
	else()
		# lots of warnings and all warnings as errors
		target_compile_options(midikraft-sequential-rev2 PRIVATE -Wall -Wextra -pedantic -Werror)
	endif()
endif()
