# Copyright (c) 2026 Christof Ruch. All rights reserved.
# Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase.

cmake_minimum_required(VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(KnobKraftRecallPlugin LANGUAGES C CXX)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)

	set(KNOBKRAFT_JUCE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/JUCE" CACHE PATH "Path to the JUCE source checkout")
	set(KNOBKRAFT_JSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/json" CACHE PATH "Path to the nlohmann/json source checkout")
	add_subdirectory("${KNOBKRAFT_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
	set(JSON_BuildTests OFF CACHE INTERNAL "")
	add_subdirectory("${KNOBKRAFT_JSON_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/json" EXCLUDE_FROM_ALL)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../MidiKraft/session" "${CMAKE_CURRENT_BINARY_DIR}/MidiKraft-session" EXCLUDE_FROM_ALL)
	enable_testing()
endif()

set(KNOBKRAFT_RECALL_CORE_SOURCES
	src/PluginEditor.cpp
	src/PluginEditor.h
	src/PluginProcessor.cpp
	src/PluginProcessor.h
	src/PluginState.cpp
	src/PluginState.h
)

add_library(knobkraft-recall-core STATIC ${KNOBKRAFT_RECALL_CORE_SOURCES})
target_include_directories(knobkraft-recall-core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(knobkraft-recall-core
	PUBLIC
		midikraft::session
		juce::juce_audio_processors
		juce::juce_gui_extra
)
target_compile_features(knobkraft-recall-core PUBLIC cxx_std_20)
set_target_properties(knobkraft-recall-core PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_compile_definitions(knobkraft-recall-core PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# WP-00 currently names a local constant DOMAIN. MSVC's UCRT defines DOMAIN
# while including <cmath>, so keep the dependency source untouched and contain
# the Windows workaround in this consumer until WP-00 can rename the constant.
if(MSVC)
	target_compile_options(midikraft-session PRIVATE "/FI${CMAKE_CURRENT_LIST_DIR}/cmake/MsvcDomainCompatibility.h")
endif()

juce_add_plugin(KnobKraftRecall
	VERSION 0.1.0
	COMPANY_NAME "KnobKraft"
	BUNDLE_ID "com.knobkraft.recall"
	PLUGIN_MANUFACTURER_CODE KbKr
	PLUGIN_CODE KkRc
	FORMATS VST3
	PRODUCT_NAME "KnobKraft Recall"
	DESCRIPTION "Project recall for hardware synthesizer patches"
	IS_SYNTH FALSE
	NEEDS_MIDI_INPUT FALSE
	NEEDS_MIDI_OUTPUT FALSE
	IS_MIDI_EFFECT FALSE
	EDITOR_WANTS_KEYBOARD_FOCUS FALSE
	COPY_PLUGIN_AFTER_BUILD FALSE
	VST3_CATEGORIES Fx Utility
)

target_sources(KnobKraftRecall PRIVATE src/PluginEntry.cpp)
target_link_libraries(KnobKraftRecall
	PRIVATE
		knobkraft-recall-core
		juce::juce_recommended_config_flags
		juce::juce_recommended_lto_flags
		juce::juce_recommended_warning_flags
)
target_compile_definitions(KnobKraftRecall PUBLIC
	JUCE_VST3_CAN_REPLACE_VST2=0
)

option(BUILD_KNOBKRAFT_PLUGIN_TESTS "Build KnobKraft Recall processor and state tests" OFF)
if(BUILD_KNOBKRAFT_PLUGIN_TESTS)
	add_executable(knobkraft-recall-tests tests/PluginTests.cpp)
	target_link_libraries(knobkraft-recall-tests PRIVATE knobkraft-recall-core)
	target_compile_features(knobkraft-recall-tests PRIVATE cxx_std_20)
	add_test(NAME knobkraft-recall-tests COMMAND knobkraft-recall-tests)
endif()

if(MSVC)
	target_compile_options(knobkraft-recall-core PRIVATE /W4 /WX)
	if(BUILD_KNOBKRAFT_PLUGIN_TESTS)
		target_compile_options(knobkraft-recall-tests PRIVATE /W4 /WX)
	endif()
else()
	target_compile_options(knobkraft-recall-core PRIVATE -Wall -Wextra -pedantic -Werror)
	if(BUILD_KNOBKRAFT_PLUGIN_TESTS)
		target_compile_options(knobkraft-recall-tests PRIVATE -Wall -Wextra -pedantic -Werror)
	endif()
endif()
