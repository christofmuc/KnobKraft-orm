/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericCustomProgramChangeCapability.h"

#include "GenericAdaptation.h"
#include "Sysex.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace py = pybind11;

namespace knobkraft {

	std::vector<juce::MidiMessage> GenericCustomProgramChangeCapability::createCustomProgramChangeMessages(MidiProgramNumber program) const
	{
		py::gil_scoped_acquire acquire;
		try {
			int c = me_->channel().toZeroBasedInt();
			if (c < 0) {
				spdlog::warn("unknown channel in createCustomProgramChangeMessages, defaulting to MIDI channel 1");
				c = 0;
			}
			int patchNo = program.toZeroBasedWithBank();
			py::object result = me_->callMethod(kCreateCustomProgramChange, c, patchNo);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kCreateCustomProgramChange, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kCreateCustomProgramChange, ex);
		}
		return {};
	}

}

