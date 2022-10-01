#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraft-Generic-Adaptation)

set(adaptation_files
	"Behringer Deepmind 12.py" "DSI Pro 2.py" "DSI Prophet 08.py" "DSI_Evolver.py" "DSI_Mopho.py" "DSI_Mopho_X4.py" "DSI_Tetra.py" 
	"DSI Prophet 12.py" "ElectraOne.py" "Ensoniqesq1.py" "KawaiK1.py" "KawaiK3.py" "KorgDW6000.py" "KorgDW8000.py" "KorgMS2000.py" "Matrix 6.py"
	"Matrix1000.py" "Novation_UltraNova.py" "OberheimOB8.py" "OberheimOBX.py" "OberheimOBXa.py" "PioneerToraiz-AS1.py" "QuasimidiCyber6.py" "Roland JX-8P.py" "RolandD50.py" "Sequential Pro 3.py"
	"Sequential Prophet 5 Rev4.py" "Sequential Prophet 6.py" "Sequential Prophet X.py" "Sequential_Take_5.py" "Studiologic Sledge.py" "Waldorf Blofeld.py"
		"Zoom_MS.py"
)

set(adaptation_support_files
	"roland/__init__.py" "sequential/__init__.py" "sequential/GenericSequential.py"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CompiledAdaptations.h
	COMMAND ${CMAKE_COMMAND} -DOUTPUT="${CMAKE_CURRENT_BINARY_DIR}/CompiledAdaptations.h" -DDIR="${CMAKE_CURRENT_LIST_DIR}/*.py" -P createResources.cmake	
	DEPENDS ${adaptation_files}
	COMMENT "Compiling binary resources"
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

# Define the sources for the static library
set(Sources
	BundledAdaptation.cpp BundledAdaptation.h
	${CMAKE_CURRENT_BINARY_DIR}/CompiledAdaptations.h
	CreateNewAdaptationDialog.cpp CreateNewAdaptationDialog.h
	GenericAdaptation.cpp GenericAdaptation.h
	GenericBankDumpCapability.cpp GenericBankDumpCapability.h
	GenericEditBufferCapability.cpp GenericEditBufferCapability.h
	GenericHasBankDescriptorsCapability.cpp GenericHasBankDescriptorsCapability.h
	GenericHasBanksCapability.cpp GenericHasBanksCapability.h
	GenericPatch.cpp GenericPatch.h
	GenericProgramDumpCapability.cpp GenericProgramDumpCapability.h
	PythonUtils.cpp PythonUtils.h
	${adaptation_files}
	${adaptation_support_files}
)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/CompiledAdaptations.h	
	PROPERTIES GENERATED TRUE
)

# Setup library
add_library(knobkraft-generic-adaptation ${Sources})
target_include_directories(knobkraft-generic-adaptation PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} ${boost_SOURCE_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(knobkraft-generic-adaptation PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(knobkraft-generic-adaptation pybind11::embed juce-utils midikraft-base ${APPLE_BOOST})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(knobkraft-generic-adaptation PRIVATE /W4 /WX /utf-8)
else()
    # lots of warnings and all warnings as errors
    #target_compile_options(knobkraft-generic-adaptation PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

# Define additional install files - in our case, the generic python modules need to be copied on Linux and on Mac!
install(DIRECTORY sequential/ DESTINATION bin/sequential)
