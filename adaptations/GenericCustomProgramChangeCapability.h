/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "CustomProgramChangeCapability.h"

namespace knobkraft {

	class GenericAdaptation;

	class GenericCustomProgramChangeCapability : public midikraft::CustomProgramChangeCapability
	{
	public:
		GenericCustomProgramChangeCapability(GenericAdaptation* me) : me_(me) {}
		virtual ~GenericCustomProgramChangeCapability() = default;
		virtual std::vector<juce::MidiMessage> createCustomProgramChangeMessages(MidiProgramNumber program) const override;

	private:
		GenericAdaptation* me_;
	};

}

