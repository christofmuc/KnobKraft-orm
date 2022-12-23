#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(MidiKraft-Oberheim-Matrix1000)

# Define the sources for the static library
set(Sources
	Matrix1000.cpp Matrix1000.h
	Matrix1000_GlobalSettings.cpp Matrix1000_GlobalSettings.h
	#Matrix1000BCR.cpp Matrix1000BCR.h
	Matrix1000ParamDefinition.cpp Matrix1000ParamDefinition.h
	Matrix1000Patch.cpp Matrix1000Patch.h
	README.md
	LICENSE.md
)

# Setup library
add_library(midikraft-oberheim-matrix1000 ${Sources})
target_include_directories(midikraft-oberheim-matrix1000 PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(midikraft-oberheim-matrix1000 PUBLIC "${icu_SOURCE_DIR}/lib64")
	target_link_libraries(midikraft-oberheim-matrix1000 juce-utils midikraft-base spdlog::spdlog)
ELSEIF(APPLE)
	target_link_libraries(midikraft-oberheim-matrix1000 juce-utils midikraft-base spdlog::spdlog ICU::data ICU::uc ${APPLE_BOOST})
ELSE()
	target_link_libraries(midikraft-oberheim-matrix1000 juce-utils midikraft-base spdlog::spdlog icudata icuuc)
ENDIF()

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(midikraft-oberheim-matrix1000 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(midikraft-oberheim-matrix1000 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
