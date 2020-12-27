/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "EditBufferCapability.h"

#include "GenericAdaptation.h"

namespace knobkraft {

	// EditBufferCapability
	class GenericEditBufferCapability : public midikraft::EditBufferCapability {
	public:
		GenericEditBufferCapability(GenericAdaptation *me) : me_(me) {}
		MidiMessage requestEditBufferDump() const override;
		bool isEditBufferDump(const MidiMessage& message) const override;
		std::shared_ptr<midikraft::DataFile> patchFromSysex(const MidiMessage& message) const override;
		std::vector<MidiMessage> patchToSysex(std::shared_ptr<midikraft::DataFile> patch) const override;
		MidiMessage saveEditBufferToProgram(int programNumber) override;

	private:
		GenericAdaptation *me_;
	};


}
