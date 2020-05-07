/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericAdaption.h"

#include "Patch.h"
#include "Logger.h"
#include "Sysex.h"

#include "PythonUtils.h"

#include <pybind11/stl.h>
//#include <pybind11/pybind11.h>
#include <memory>

namespace py = pybind11;

#include <boost/format.hpp>

namespace knobkraft {

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptionPythonEmbeddedGuard;
	std::unique_ptr<PyStdErrOutStreamRedirect> sGenericAdaptionPyOutputRedirect;

	void checkForPythonOutputAndLog() {
		sGenericAdaptionPyOutputRedirect->flushToLogger("Adaption");
	}

	class GenericPatchNumber : public midikraft::PatchNumber {
	public:
		GenericPatchNumber(MidiProgramNumber programNumber) : programNumber_(programNumber) {
		}

		std::string friendlyName() const override
		{
			return (boost::format("%d") % programNumber_.toOneBased()).str();
		}

	private:
		MidiProgramNumber programNumber_;
	};

	class GenericPatch : public midikraft::Patch {
	public:
		enum DataType {
			PROGRAM_DUMP = 0,
			EDIT_BUFFER
		};

		GenericPatch(pybind11::module &adaption_module, midikraft::Synth::PatchData const &data, DataType dataType) : midikraft::Patch(dataType, data), adaption_(adaption_module) {
			patchNumber_ = std::make_shared<GenericPatchNumber>(MidiProgramNumber::fromZeroBase(0));
		}

		template <typename ... Args>
		py::object callMethod(std::string const &methodName, Args& ... args) const {
			if (!adaption_) {
				return py::none();
			}
			if (py::hasattr(*adaption_, methodName.c_str())) {
				auto result = adaption_.attr(methodName.c_str())(args...);
				checkForPythonOutputAndLog();
				return result;
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Adaption: method %s not found, fatal!") % methodName).str());
				return py::none();
			}
		}


		std::string name() const override
		{
			try {
				auto message = data();
				auto result = callMethod("nameFromDump", message);
				return result.cast<std::string>();
			}
			catch (std::exception &ex) {
				std::string errorMessage = (boost::format("Error calling nameFromDump: %s") % ex.what()).str();
				SimpleLogger::instance()->postMessage(errorMessage);
			}
			return "invalid";
		}

		std::shared_ptr<midikraft::PatchNumber> patchNumber() const override
		{
			return patchNumber_;
		}


		void setPatchNumber(MidiProgramNumber patchNumber) override
		{
			patchNumber_ = std::make_shared<GenericPatchNumber>(patchNumber);
		}

	private:
		pybind11::module &adaption_;
		std::string name_;
		std::shared_ptr<midikraft::PatchNumber> patchNumber_;
	};

	GenericAdaption::GenericAdaption(std::string const &pythonModuleFilePath) : filepath_(pythonModuleFilePath)
	{
		try {
			adaption_module = py::module::import(filepath_.c_str());
			checkForPythonOutputAndLog();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Failure loading python module: %s") % ex.what()).str());
		}
	}

	void GenericAdaption::startupGenericAdaption()
	{
		sGenericAdaptionPythonEmbeddedGuard = std::make_unique<py::scoped_interpreter>();
		sGenericAdaptionPyOutputRedirect = std::make_unique<PyStdErrOutStreamRedirect>();
		std::string command = "import sys\nsys.path.append(R\"" + getAdaptionDirectory().getFullPathName().toStdString() + "\")\n";
		py::exec(command);
		checkForPythonOutputAndLog();
	}

	juce::File GenericAdaption::getAdaptionDirectory()
	{
		// Should I make this configurable?
		return File(File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName() + "/KnobKraft-Orm-adaptions");
	}

	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> GenericAdaption::allAdaptions()
	{
		std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> result;
		File adaptionDirectory = getAdaptionDirectory();
		if (adaptionDirectory.exists()) {
			for (auto f : adaptionDirectory.findChildFiles(File::findFiles, false, "*.py")) {
				result.push_back(std::make_shared<GenericAdaption>(f.getFileNameWithoutExtension().toStdString()));
			}
		}
		return result;
	}

	template <typename ... Args>
	py::object GenericAdaption::callMethod(std::string const &methodName, Args& ... args) const {
		if (!adaption_module) {
			return py::none();
		}
		if (py::hasattr(*adaption_module, methodName.c_str())) {
			auto result = adaption_module.attr(methodName.c_str())(args...);
			checkForPythonOutputAndLog();
			return result;
		}
		else {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: method %s not found, fatal!") % methodName).str());
			return py::none();
		}
	}

	juce::MidiMessage GenericAdaption::requestEditBufferDump()
	{
		try {
			int c = channel().toZeroBasedInt();
			py::object result = callMethod("createEditBufferRequest", c);
			// These should be only one midi message...
			return { vectorToMessage(result.cast<std::vector<int>>()) };
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling createEditBufferRequest: %s") % ex.what()).str());
			return {};
		}
	}

	bool GenericAdaption::isEditBufferDump(const MidiMessage& message) const
	{
		try {
			auto vectorForm = messageToVector(message);
			py::object result = callMethod("isEditBufferDump", vectorForm);
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling isEditBufferDump: %s") % ex.what()).str());
			return false;
		}
	}

	std::shared_ptr<midikraft::Patch> GenericAdaption::patchFromSysex(const MidiMessage& message) const
	{
		// For the Generic Adaption, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(const_cast<py::module &>(adaption_module), data, GenericPatch::EDIT_BUFFER);
	}

	std::vector<juce::MidiMessage> GenericAdaption::patchToSysex(const midikraft::Patch &patch) const
	{
		try {
			auto data = patch.data();
			int c = channel().toZeroBasedInt();
			py::object result = callMethod("convertToEditBuffer", c, data);
			return { vectorToMessage(py::cast<std::vector<int>>(result)) };
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling convertToEditBuffer: %s") % ex.what()).str());
			return {};
		}
		// For the Generic Adaption, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage(s)
		//return { MidiMessage(patch.data().data(), (int)patch.data().size()) };
	}

	juce::MidiMessage GenericAdaption::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		return MidiMessage();
	}

	int GenericAdaption::numberOfBanks() const
	{
		try {
			py::object result = callMethod("numberOfBanks");
			return result.cast<int>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling numberOfBanks: %s") % ex.what()).str());
			return 1;
		}
	}

	int GenericAdaption::numberOfPatches() const
	{
		try {
			py::object result = callMethod("numberOfPatchesPerBank");
			return result.cast<int>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling numberOfPatchesPerBank: %s") % ex.what()).str());
			return 0;
		}
	}

	std::string GenericAdaption::friendlyBankName(MidiBankNumber bankNo) const
	{
		//TODO could delegate this to the python code as well
		return (boost::format("Bank %d") % bankNo.toOneBased()).str();
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaption::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto patch = std::make_shared<GenericPatch>(const_cast<py::module &>(adaption_module), data, GenericPatch::PROGRAM_DUMP);
		patch->setPatchNumber(place);
		return patch;
	}

	bool GenericAdaption::isOwnSysex(MidiMessage const &message) const
	{
		//TODO - if we delegate this to the python code, the "sniff synth" method of the Librarian can be used. But this is currently disabled anyway,
		// even if I forgot why
		ignoreUnused(message);
		return false;
	}

	juce::MidiMessage GenericAdaption::deviceDetect(int channel)
	{
		try {
			py::object result = callMethod("createDeviceDetectMessage", channel);
			return vectorToMessage(result.cast<std::vector<int>>());
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling createDeviceDetectMessage: %s") % ex.what()).str());
			return MidiMessage();
		}
	}

	int GenericAdaption::deviceDetectSleepMS()
	{
		try
		{
			py::object result = callMethod("deviceDetectWaitMilliseconds");
			return result.cast<int>();

		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling deviceDetectSleepMS: %s") % ex.what()).str());
			return 100;
		}
	}

	MidiChannel GenericAdaption::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		try {
			auto vector = messageToVector(message);
			py::object result = callMethod("channelIfValidDeviceResponse", vector);
			int intResult = result.cast<int>();
			if (intResult >= 0 && intResult < 16) {
				return MidiChannel::fromZeroBase(intResult);
			}
			else {
				return MidiChannel::invalidChannel();
			}
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling channelIfValidDeviceResponse: %s") % ex.what()).str());
			return MidiChannel::invalidChannel();
		}
	}

	bool GenericAdaption::needsChannelSpecificDetection()
	{
		try
		{
			py::object result = callMethod("needsChannelSpecificDetection");
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling needsChannelSpecificDetection: %s") % ex.what()).str());
			return true;
		}
	}

	std::shared_ptr<midikraft::Patch> GenericAdaption::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		// For the Generic Adaption, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(const_cast<py::module &>(adaption_module), data, GenericPatch::PROGRAM_DUMP);
	}

	std::vector<juce::MidiMessage> GenericAdaption::patchToProgramDumpSysex(const midikraft::Patch &patch) const
	{
		// For the Generic Adaption, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage(s)
		return { MidiMessage(patch.data().data(), (int) patch.data().size()) };
	}

	std::string GenericAdaption::getName() const
	{
		try {
			py::object result = callMethod("name");
			return result.cast<std::string>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling getName: %s") % ex.what()).str());
			return "Invalid";
		}
	}

	std::vector<juce::MidiMessage> GenericAdaption::requestPatch(int patchNo)
	{
		try {
			int c = channel().toZeroBasedInt();
			py::object result = callMethod("createProgramDumpRequest", c, patchNo);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling createProgramDumpRequest: %s") % ex.what()).str());
			return {};
		}
	}

	bool GenericAdaption::isSingleProgramDump(const MidiMessage& message) const
	{
		try {
			auto vector = messageToVector(message);
			py::object result = callMethod("isSingleProgramDump", vector);
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaption: Error calling isSingleProgramDump: %s") % ex.what()).str());
			return false;
		}
	}

	std::vector<int> GenericAdaption::messageToVector(MidiMessage const &message) {
		return std::vector<int>(message.getRawData(), message.getRawData() + message.getRawDataSize());
	}

	std::vector<uint8> GenericAdaption::intVectorToByteVector(std::vector<int> const &data) {
		std::vector<uint8> byteData;
		for (int byte : data) {
			if (byte >= 0 && byte < 256) {
				byteData.push_back((uint8)byte);
			}
			else {
				throw std::runtime_error("Adaption: Value out of range in Midi Message");
			}
		}
		return byteData;
	}

	juce::MidiMessage GenericAdaption::vectorToMessage(std::vector<int> const &data)
	{
		auto byteData = intVectorToByteVector(data);
		return MidiMessage(byteData.data(), (int)byteData.size());
	}

}
