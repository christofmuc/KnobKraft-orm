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

	std::shared_ptr<midikraft::DataFile> GenericProgramDumpCapability::patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const
	{
		py::gil_scoped_acquire acquire;
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data;
		for (auto const& m : message) {
			std::copy(m.getRawData(), m.getRawData() + m.getRawDataSize(), std::back_inserter(data));
		}
		return std::make_shared<GenericPatch>(me_, const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::PROGRAM_DUMP);
	}

	std::vector<juce::MidiMessage> GenericProgramDumpCapability::requestPatch(int patchNo) const
	{
		py::gil_scoped_acquire acquire;
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

	bool GenericProgramDumpCapability::isSingleProgramDump(const std::vector<MidiMessage>& message) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto vector = GenericAdaptation::midiMessagesToVector(message);
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

	bool GenericProgramDumpCapability::isMessagePartOfProgramDump(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		// This is an optional function that can be implemented for multi message edit buffers like in the DSI Evolver
		if (me_->pythonModuleHasFunction(kIsPartOfSingleProgramDump)) {
			try {
				auto vectorForm = me_->messageToVector(message);
				py::object result = me_->callMethod(kIsPartOfSingleProgramDump, vectorForm);
				return result.cast<bool>();
			}
			catch (py::error_already_set& ex) {
				me_->logAdaptationError(kIsPartOfSingleProgramDump, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				me_->logAdaptationError(kIsPartOfSingleProgramDump, ex);
			}
			return false;
		}
		else {
			// Default implementation is to just to call isSingleProgramDump() with a single message vector
			return isSingleProgramDump({ message });
		}
	}

	MidiProgramNumber GenericProgramDumpCapability::getProgramNumber(const std::vector<MidiMessage>& message) const
	{
		py::gil_scoped_acquire acquire;
		if (me_->pythonModuleHasFunction("numberFromDump")) {
			try {
				auto vector = GenericAdaptation::midiMessagesToVector(message);
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
		py::gil_scoped_acquire acquire;
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
