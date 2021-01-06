/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericProgramDumpCapability.h"

#include "GenericPatch.h"

#include "Sysex.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;


namespace knobkraft {

	std::shared_ptr<midikraft::DataFile> GenericProgramDumpCapability::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(me_, const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::PROGRAM_DUMP);
	}

	std::vector<juce::MidiMessage> GenericProgramDumpCapability::requestPatch(int patchNo) const
	{
		try {
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod(kCreateProgramDumpRequest, c, patchNo);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kCreateProgramDumpRequest, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kCreateProgramDumpRequest, ex);
		}
		return {};
	}

	bool GenericProgramDumpCapability::isSingleProgramDump(const MidiMessage& message) const
	{
		try {
			auto vector = me_->messageToVector(message);
			py::object result = me_->callMethod(kIsSingleProgramDump, vector);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsSingleProgramDump, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsSingleProgramDump, ex);
		}
		return false;
	}

	MidiProgramNumber GenericProgramDumpCapability::getProgramNumber(const MidiMessage &message) const
	{
		if (me_->pythonModuleHasFunction("numberFromDump")) {
			try {
				auto vector = me_->messageToVector(message);
				py::object result = me_->callMethod(kNumberFromDump, vector);
				return MidiProgramNumber::fromZeroBase(result.cast<int>());
			}
			catch (py::error_already_set &ex) {
				me_->logAdaptationError(kNumberFromDump, ex);
				ex.restore();
			}
			catch (std::exception &ex) {
				me_->logAdaptationError(kNumberFromDump, ex);
			}
		}
		return MidiProgramNumber::fromZeroBase(0);
	}

	std::vector<juce::MidiMessage> GenericProgramDumpCapability::patchToProgramDumpSysex(std::shared_ptr<midikraft::DataFile> patch, MidiProgramNumber programNumber) const
	{
		try
		{
			auto data = patch->data();
			int c = me_->channel().toZeroBasedInt();
			int programNo = programNumber.toZeroBased();
			py::object result = me_->callMethod(kConvertToProgramDump, c, data, programNo);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kConvertToProgramDump, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kConvertToProgramDump, ex);
		}
		return { MidiMessage(patch->data().data(), (int)patch->data().size()) };
	}

}
