/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"
#include "ProgressHandler.h"

#include <memory>
#include <string>
#include <vector>

namespace midikraft {

	enum class MidiLoopbackTestStatus {
		Passed,
		TimedOut,
		Incomplete,
		Mismatch,
		Cancelled
	};

	struct MidiLoopbackTestResult {
		std::string name;
		MidiLoopbackTestStatus status = MidiLoopbackTestStatus::TimedOut;
		std::vector<juce::uint8> expected;
		std::vector<juce::uint8> received;
		int elapsedMilliseconds = 0;
		int firstDifferentByte = -1;
	};

	struct MidiLoopbackTestReport {
		juce::MidiDeviceInfo midiOutput;
		juce::MidiDeviceInfo midiInput;
		std::vector<MidiLoopbackTestResult> results;
		std::string error;
		bool cancelled = false;

		bool allPassed() const;
	};

	class MidiLoopbackTest {
	public:
		// Runs synchronously. Call this from a worker thread, not the JUCE message thread.
		static MidiLoopbackTestReport run(juce::MidiDeviceInfo const& midiInput,
			juce::MidiDeviceInfo const& midiOutput,
			std::weak_ptr<ProgressHandler> progressHandler = {});
	};

}
