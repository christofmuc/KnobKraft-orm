#
#  Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(KnobKraft-Generic-Adaptation)

set(adaptation_files
	"AlesisAndromedaA6.py"
	"BC_Kijimi.py"
	"Behringer Deepmind 12.py" 
	"DSI Pro 2.py" "DSI Prophet 08.py" "DSI_Evolver.py" "DSI_Mopho.py" "DSI_Mopho_X4.py" "DSI_Tetra.py" "DSI Prophet 12.py" 
	"ElectraOne.py" 
	"Ensoniqesq1.py" 
	"GrooveSynthesis_3rdWave.py" 
	"JB_Solaris.py"
	"KawaiK1.py" 
	"Korg_03RW.py" "KorgDW6000.py" "KorgMinilogue_XD.py" "KorgMS2000.py" 
	"Matrix 6.py" 
	"Novation_Summit.py" "Novation_UltraNova.py" 
	"OberheimOB8.py" "OberheimOBX.py" "OberheimOBXa.py" "OberheimOBX8.py"
	"PioneerToraiz-AS1.py" 
	"QuasimidiCyber6.py" 
	"Roland JX-8P.py" "Roland_JV80.py" "Roland_JV1080.py" "Roland_XV3080.py" "RolandD50.py" "Roland_MKS70V4.py"
	"Sequential Pro 3.py" "Sequential Prophet 5 Rev4.py" "Sequential Prophet 6.py" "Sequential Prophet X.py" "Sequential_Take_5.py" 
	"Studiologic Sledge.py" 
	"Waldorf Blofeld.py" "Waldorf_Kyra.py"
	"YamahaDX7.py" "YamahaDX7II.py"
	"Zoom_MS.py"
)

set(adaptation_files_test_only
	"KawaiK3.py" 
	"KorgDW8000.py" 
	"Matrix1000.py" 
	"YamahaRefaceDX.py"
)

set(adaptation_support_files
	"roland/__init__.py" "roland/GenericRoland.py" "sequential/__init__.py" "sequential/GenericSequential.py"
	"knobkraft/__init__.py" "knobkraft/adaptation_module.py" "knobkraft/sysex.py" "knobkraft/test_helper.py"
)

# Define the sources for the static library
set(Sources
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
	${adaptation_files_test_only}
	${adaptation_support_files}
)

# Setup library
add_library(knobkraft-generic-adaptation ${Sources})
target_include_directories(knobkraft-generic-adaptation PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${JUCE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} "${icu_SOURCE_DIR}/include")
if(WIN32)
	target_link_directories(knobkraft-generic-adaptation PUBLIC "${icu_SOURCE_DIR}/lib64")
endif()
target_link_libraries(knobkraft-generic-adaptation pybind11::embed juce-utils midikraft-base spdlog::spdlog)

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
	# Turn off 4068 unknown pragmas
    target_compile_options(knobkraft-generic-adaptation PRIVATE /W4 /WX /utf-8 /wd4068)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(knobkraft-generic-adaptation PRIVATE -Wall -Wextra -pedantic)
endif()

if (MSVC)
set(INSTALLER_FILE_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/../The-Orm/adaptations.iss")
file(WRITE ${INSTALLER_FILE_LIST_FILE} "; Auto generated, don't edit'\n")
foreach(ADAPTATION ${adaptation_files})
file(APPEND ${INSTALLER_FILE_LIST_FILE} "Source: \"${CMAKE_CURRENT_LIST_DIR}/${ADAPTATION}\"; DestDir: \"{app}\\adaptations\"\n")
endforeach()

endif()

# Define additional install files - in our case, all adaptation files and the python modules
install(DIRECTORY sequential/ DESTINATION bin/sequential)
install(DIRECTORY roland/ DESTINATION bin/roland)
install(DIRECTORY knobkraft/ DESTINATION bin/knobkraft)
install(FILES ${adaptation_files} DESTINATION bin)

