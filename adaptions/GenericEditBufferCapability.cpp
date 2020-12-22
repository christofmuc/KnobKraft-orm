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


	juce::MidiMessage GenericEditBufferCapability::requestEditBufferDump()
	{
		try {
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod("createEditBufferRequest", c);
			// These should be only one midi message...
			return { GenericAdaptation::vectorToMessage(result.cast<std::vector<int>>()) };
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling createEditBufferRequest: %s") % ex.what()).str());
			ex.restore();
			return {};
		}
	}

	bool GenericEditBufferCapability::isEditBufferDump(const MidiMessage& message) const
	{
		try {
			auto vectorForm = me_->messageToVector(message);
			py::object result = me_->callMethod("isEditBufferDump", vectorForm);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling isEditBufferDump: %s") % ex.what()).str());
			ex.restore();
			return false;
		}
	}

	std::shared_ptr<midikraft::DataFile> GenericEditBufferCapability::patchFromSysex(const MidiMessage& message) const
	{
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::EDIT_BUFFER);
	}

	std::vector<juce::MidiMessage> GenericEditBufferCapability::patchToSysex(std::shared_ptr<midikraft::DataFile> patch) const
	{
		try {
			auto data = patch->data();
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod("convertToEditBuffer", c, data);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling convertToEditBuffer: %s") % ex.what()).str());
			return {};
		}
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage(s)
		//return { MidiMessage(patch.data().data(), (int)patch.data().size()) };
	}

	juce::MidiMessage GenericEditBufferCapability::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		return MidiMessage();
	}


}



