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
		return std::make_shared<GenericPatch>(const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::PROGRAM_DUMP);
	}

	std::vector<juce::MidiMessage> GenericProgramDumpCapability::requestPatch(int patchNo) const
	{
		try {
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod("createProgramDumpRequest", c, patchNo);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling createProgramDumpRequest: %s") % ex.what()).str());
			return {};
		}
	}

	bool GenericProgramDumpCapability::isSingleProgramDump(const MidiMessage& message) const
	{
		try {
			auto vector = me_->messageToVector(message);
			py::object result = me_->callMethod("isSingleProgramDump", vector);
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling isSingleProgramDump: %s") % ex.what()).str());
			return false;
		}
	}

	MidiProgramNumber GenericProgramDumpCapability::getProgramNumber(const MidiMessage &message) const
	{
		if (me_->pythonModuleHasFunction("numberFromDump")) {
			try {
				auto vector = me_->messageToVector(message);
				py::object result = me_->callMethod("numberFromDump", vector);
				return MidiProgramNumber::fromZeroBase(result.cast<int>());
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling numberFromDump: %s") % ex.what()).str());
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
			py::object result = me_->callMethod("convertToProgramDump", c, data, programNo);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling convertToProgramDump: %s") % ex.what()).str());
			// Make it a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage(s)
			return { MidiMessage(patch->data().data(), (int)patch->data().size()) };
		}
	}

}
