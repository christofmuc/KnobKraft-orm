#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Kawai-K3)

set(RESOURCE_FILES 
	"resources/R6P-09 27c256.BIN"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/BinaryResources.h
	COMMAND ${CMAKE_COMMAND} -P createResources.cmake	
	DEPENDS ${RESOURCE_FILES}
	COMMENT "Compiling binary resources"
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)


# Define the sources for the static library
set(Sources
	BinaryResources.h
	KawaiK3.cpp KawaiK3.h           
	KawaiK3_BCR2000.cpp KawaiK3_BCR2000.h    
	#KawaiK3_Reverse.cpp KawaiK3_Reverse.h     
	KawaiK3Parameter.cpp KawaiK3Parameter.h  
	KawaiK3Patch.cpp KawaiK3Patch.h                
	KawaiK3Wave.cpp KawaiK3Wave.h
	KawaiK3WaveParameter.cpp KawaiK3WaveParameter.h  
	${RESOURCE_FILES}
	README.md
	LICENSE.md
)

set_source_files_properties(
	BinaryResources.h
	PROPERTIES GENERATED TRUE
)

# Setup library
add_library(midikraft-kawai-k3 ${Sources})
target_include_directories(midikraft-kawai-k3 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(midikraft-kawai-k3 PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(midikraft-kawai-k3 juce-utils midikraft-base midikraft-behringer-bcr2000 ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-kawai-k3 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-kawai-k3 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
