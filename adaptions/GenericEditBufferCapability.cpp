/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericEditBufferCapability.h"

#include "PythonUtils.h"
#include "Sysex.h"

#include "GenericPatch.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <boost/format.hpp>

namespace py = pybind11;


namespace knobkraft {


	juce::MidiMessage GenericEditBufferCapability::requestEditBufferDump() const
	{
		py::gil_scoped_acquire acquire;
		try {
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod(kCreateEditBufferRequest, c);
			// These should be only one midi message...
			return { GenericAdaptation::vectorToMessage(result.cast<std::vector<int>>()) };
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kCreateEditBufferRequest, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kCreateEditBufferRequest, ex);
		}
		return {};
	}

	bool GenericEditBufferCapability::isEditBufferDump(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto vectorForm = me_->messageToVector(message);
			py::object result = me_->callMethod(kIsEditBufferDump, vectorForm);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsEditBufferDump, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsEditBufferDump, ex);
		}
		return false;
	}

	std::shared_ptr<midikraft::DataFile> GenericEditBufferCapability::patchFromSysex(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(me_, const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::EDIT_BUFFER);
	}

	std::vector<juce::MidiMessage> GenericEditBufferCapability::patchToSysex(std::shared_ptr<midikraft::DataFile> patch) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto data = patch->data();
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod(kConvertToEditBuffer, c, data);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kConvertToEditBuffer, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kConvertToEditBuffer, ex);
		}
		return {};
	}

	juce::MidiMessage GenericEditBufferCapability::saveEditBufferToProgram(int programNumber)
	{
		py::gil_scoped_acquire acquire;
		ignoreUnused(programNumber);
		return MidiMessage();
	}


}



