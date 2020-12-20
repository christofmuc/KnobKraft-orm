/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericAdaptation.h"

#include "Patch.h"
#include "Logger.h"
#include "Sysex.h"

#include "PythonUtils.h"
#include "Settings.h"
#include "BundledAdaptation.h"

#include "GenericPatch.h"
#include "GenericEditBufferCapability.h"

#include <pybind11/stl.h>
//#include <pybind11/pybind11.h>
#include <memory>

#include "Python.h"

namespace py = pybind11;

#include <boost/format.hpp>

namespace knobkraft {

	const char *kUserAdaptationsFolderSettingsKey = "user_adaptations_folder";

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptationPythonEmbeddedGuard;
	std::unique_ptr<PyStdErrOutStreamRedirect> sGenericAdaptationPyOutputRedirect;
	CriticalSection GenericAdaptation::multiThreadGuard;

	void checkForPythonOutputAndLog() {
		sGenericAdaptationPyOutputRedirect->flushToLogger("Adaptation");
	}


	GenericAdaptation::GenericAdaptation(std::string const &pythonModuleFilePath) : filepath_(pythonModuleFilePath)
	{
		editBufferCapabilityImpl_ = std::make_shared<GenericEditBufferCapability>(shared_from_this());
		programDumpCapabilityImpl_ = std::make_shared<GenericProgramDumpCapability>(shared_from_this());
		try {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			adaptation_module = py::module::import(filepath_.c_str());
			checkForPythonOutputAndLog();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure loading python module: %s") % ex.what()).str());
			ex.restore();
		}
	}

	GenericAdaptation::GenericAdaptation(pybind11::module adaptationModule)
	{
		adaptation_module = adaptationModule;
	}

	std::shared_ptr<GenericAdaptation> GenericAdaptation::fromBinaryCode(std::string moduleName, std::string adaptationCode)
	{
		try {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			auto importlib = py::module::import("importlib.util");
			checkForPythonOutputAndLog();
			auto spec = importlib.attr("spec_from_loader")(moduleName, py::none()); // Create an empty module with the right name
			auto adaptation_module = importlib.attr("module_from_spec")(spec);
			auto builtins = py::module::import("builtins");
			adaptation_module.attr("__builtins__") = builtins; // That seems to be implementation depend... https://docs.python.org/3/library/builtins.html
			checkForPythonOutputAndLog();
			py::exec(adaptationCode, adaptation_module.attr("__dict__")); // Now run the define statements in the code, creating the defines within the right namespace
			checkForPythonOutputAndLog();
			auto newAdaptation = std::make_shared<GenericAdaptation>(py::cast<py::module>(adaptation_module));
			//if (newAdaptation) newAdaptation->logNamespace();
			return newAdaptation;
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure loading python module %s: %s") % moduleName % ex.what()).str());
			ex.restore();
		}
		return nullptr;
	}

	void GenericAdaptation::logNamespace() {
		try {
			auto name = py::cast<std::string>(adaptation_module.attr("__name__"));
			auto moduleDict = adaptation_module.attr("__dict__");
			for (auto a : moduleDict) {
				SimpleLogger::instance()->postMessage("Found in " + name + " attribute " + py::cast<std::string>(a));
			}
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure inspecting python module: %s") % ex.what()).str());
			ex.restore();
		}
	}

	void GenericAdaptation::startupGenericAdaptation()
	{
		if (juce::SystemStats::getEnvironmentVariable("ORM_NO_PYTHON", "NOTSET") != "NOTSET") {
			// This is the hard coded way to turn off python integration, just set the ORM_NO_PYTHON environment variable to anything (except NOTSET)
			return;
		}

#ifdef __APPLE__
		// The Apple might not have a Python 3.7 installed. We will check if we can find the appropriate Framework directory, and turn Python off in case we can't find it.
		// First, check the location where the Python 3.7 Mac installer will put it (taken from python.org/downloads)
		String python37_macHome = "/Library/Frameworks/Python.framework/Versions/3.7";
		File python37(python37_macHome);
		if (!python37.exists()) {
			// If that didn't work, check if the Homebrew brew install python3 command has installed it in the /usr/local/opt directory
			python37_macHome = "/usr/local/opt/python3/Frameworks/Python.framework/Versions/3.7";
			File python37_alternative(python37_macHome);
			if (!python37_alternative.exists()) {
				// No Python3.7 found, don't set path
				return;
			}
			else {
				Py_SetPythonHome(const_cast<wchar_t*>(python37_macHome.toWideCharPointer()));
			}
		}
		else {
			Py_SetPythonHome(const_cast<wchar_t*>(python37_macHome.toWideCharPointer()));
		}
#endif
		sGenericAdaptationPythonEmbeddedGuard = std::make_unique<py::scoped_interpreter>();
		sGenericAdaptationPyOutputRedirect = std::make_unique<PyStdErrOutStreamRedirect>();
		std::string command = "import sys\nsys.path.append(R\"" + getAdaptationDirectory().getFullPathName().toStdString() + "\")\n";
		py::exec(command);
		checkForPythonOutputAndLog();
	}

	bool GenericAdaptation::hasPython()
	{
		return sGenericAdaptationPythonEmbeddedGuard != nullptr;
	}

	juce::File GenericAdaptation::getAdaptationDirectory()
	{
		// Calculate default location - as Linux does not guarantee to provide a Documents folder, rather use the user's home directory
		File adaptationsDefault = File(File::getSpecialLocation(File::userHomeDirectory)).getChildFile("KnobKraft-Adaptations");
		auto adaptationsDirectory = Settings::instance().get(kUserAdaptationsFolderSettingsKey, adaptationsDefault.getFullPathName().toStdString());

		File adaptationsDir(adaptationsDirectory);
		if (!adaptationsDir.exists()) {
			adaptationsDir.createDirectory();
		}
		return  adaptationsDir;
	}

	void GenericAdaptation::setAdaptationDirectoy(std::string const &directory)
	{
		// This will only become active after a restart of the application, as I don't know how to properly clean the Python runtime.
		Settings::instance().set(kUserAdaptationsFolderSettingsKey, directory);
	}

	bool GenericAdaptation::createCompiledAdaptationModule(std::string const &pythonModuleName, std::string const &adaptationCode, std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> &outAddToThis) {
		auto newAdaptation = GenericAdaptation::fromBinaryCode(pythonModuleName, adaptationCode);
		if (newAdaptation) {
			// Now we need to check the name of the compiled adaptation just created, and if it is already present. If yes, don't add it but rather issue a warning
			auto newAdaptationName = newAdaptation->getName();
			if (newAdaptationName != "invalid") {
				for (auto existing : outAddToThis) {
					if (existing->getName() == newAdaptationName) {
						SimpleLogger::instance()->postMessage((boost::format("Overriding built-in adaptation %s (found in user directory %s)")
							% newAdaptationName % getAdaptationDirectory().getFullPathName().toStdString()).str());
						return false;
					}
				}
				outAddToThis.push_back(newAdaptation);
			}
			else {
				jassertfalse;
				SimpleLogger::instance()->postMessage("Program error: built-in adaptation " + std::string(pythonModuleName) + " failed to report name");
			}
		}
		return false;
	}

	std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> GenericAdaptation::allAdaptations()
	{
		std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> result;
		if (!hasPython()) {
#ifdef __APPLE__
			SimpleLogger::instance()->postMessage("Warning - couldn't find a Python 3.7 installation. Please install from https://www.python.org/ftp/python/3.7.9/python-3.7.9-macosx10.9.pkg. Turning off all adaptations.");
			//SimpleLogger::instance()->postMessage("Warning - couldn't find a Python 3.7 installation. Please install using 'brew install python3'. Turning off all adaptations.");
#else
			SimpleLogger::instance()->postMessage("Warning - couldn't find a Python 3.7 installation. Please install from https://www.python.org/downloads/release/python-378/. Turning off all adaptations.");
#endif
			return result;
		}

		// First, load user defined adaptations from the directory
		File adaptationDirectory = getAdaptationDirectory();
		if (adaptationDirectory.exists()) {
			for (auto f : adaptationDirectory.findChildFiles(File::findFiles, false, "*.py")) {
				result.push_back(std::make_shared<GenericAdaptation>(f.getFileNameWithoutExtension().toStdString()));
			}
		}

		// Then, iterate over the list of built-in adaptations and add those which are not present in the directory
		auto adaptations = gBundledAdaptations();
		for (auto const &b : adaptations) {
			createCompiledAdaptationModule(b.pythonModuleName, b.adaptationSourceCode, result);
		}
		return result;
	}

	bool GenericAdaptation::pythonModuleHasFunction(std::string const &functionName) const {
		ScopedLock lock(GenericAdaptation::multiThreadGuard);
		if (!adaptation_module) {
			return false;
		}
		return py::hasattr(*adaptation_module, functionName.c_str());
	}

	int GenericAdaptation::numberOfBanks() const
	{
		try {
			py::object result = callMethod("numberOfBanks");
			return result.cast<int>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling numberOfBanks: %s") % ex.what()).str());
			ex.restore();
			return 1;
		}
	}

	int GenericAdaptation::numberOfPatches() const
	{
		try {
			py::object result = callMethod("numberOfPatchesPerBank");
			return result.cast<int>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling numberOfPatchesPerBank: %s") % ex.what()).str());
			ex.restore();
			return 0;
		}
	}

	std::string GenericAdaptation::friendlyBankName(MidiBankNumber bankNo) const
	{
		if (!pythonModuleHasFunction("friendlyBankName")) {
			return (boost::format("Bank %d") % bankNo.toOneBased()).str();
		}
		try {
			int bankAsInt = bankNo.toZeroBased();
			py::object result = callMethod("friendlyBankName", bankAsInt);
			return result.cast<std::string>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling friendlyBankName: %s") % ex.what()).str());
			ex.restore();
			return "invalid name";
		}
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaptation::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		auto patch = std::make_shared<GenericPatch>(const_cast<py::module &>(adaptation_module), data, GenericPatch::PROGRAM_DUMP);
		patch->setPatchNumber(place);
		return patch;
	}

	bool GenericAdaptation::isOwnSysex(MidiMessage const &message) const
	{
		//TODO - if we delegate this to the python code, the "sniff synth" method of the Librarian can be used. But this is currently disabled anyway,
		// even if I forgot why
		ignoreUnused(message);
		return false;
	}

	void GenericAdaptation::sendBlockOfMessagesToSynth(std::string const& midiOutput, MidiBuffer const& buffer)
	{
		if (pythonModuleHasFunction("generalMessageDelay")) {
			try {
				auto result = callMethod("generalMessageDelay");
				int delay = py::cast<int>(result);
				// Be a bit careful with this device, do specify a delay when sending messages
				midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesThrottled(buffer, delay);
			}
			catch (py::error_already_set &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling friendlyBankName: %s") % ex.what()).str());
				ex.restore();
			}
		}
		else {
			// No special behavior - just send at full speed
			midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesFullSpeed(buffer);
		}
	}

	std::vector<juce::MidiMessage> GenericAdaptation::deviceDetect(int channel)
	{
		try {
			py::object result = callMethod("createDeviceDetectMessage", channel);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling createDeviceDetectMessage: %s") % ex.what()).str());
			ex.restore();
			return {};
		}
	}

	int GenericAdaptation::deviceDetectSleepMS()
	{
		if (!pythonModuleHasFunction("deviceDetectWaitMilliseconds")) {
			return 200;
		}
		try
		{
			py::object result = callMethod("deviceDetectWaitMilliseconds");
			return result.cast<int>();

		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling deviceDetectSleepMS: %s") % ex.what()).str());
			ex.restore();
			return 200;
		}
	}

	MidiChannel GenericAdaptation::channelIfValidDeviceResponse(const MidiMessage &message)
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
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling channelIfValidDeviceResponse: %s") % ex.what()).str());
			return MidiChannel::invalidChannel();
		}
	}

	bool GenericAdaptation::needsChannelSpecificDetection()
	{
		try
		{
			py::object result = callMethod("needsChannelSpecificDetection");
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling needsChannelSpecificDetection: %s") % ex.what()).str());
			return true;
		}
	}

	std::shared_ptr<midikraft::Patch> GenericAdaptation::GenericProgramDumpCapability::patchFromProgramDumpSysex(const MidiMessage& message) const
	{
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data(message.getRawData(), message.getRawData() + message.getRawDataSize());
		return std::make_shared<GenericPatch>(const_cast<py::module &>(me_->adaptation_module), data, GenericPatch::PROGRAM_DUMP);
	}

	std::vector<juce::MidiMessage> GenericAdaptation::GenericProgramDumpCapability::patchToProgramDumpSysex(const midikraft::Patch &patch) const
	{
		try
		{
			auto data = patch.data();
			int c = me_->channel().toZeroBasedInt();
			int programNo = patch.patchNumber()->midiProgramNumber().toZeroBased();
			py::object result = me_->callMethod("convertToProgramDump", c, data, programNo);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling convertToProgramDump: %s") % ex.what()).str());
			// Make it a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage(s)
			return { MidiMessage(patch.data().data(), (int)patch.data().size()) };
		}
	}

	std::string GenericAdaptation::getName() const
	{
		try {
			py::object result = callMethod("name");
			return result.cast<std::string>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling getName: %s") % ex.what()).str());
			return "Invalid";
		}
	}

	std::string GenericAdaptation::calculateFingerprint(std::shared_ptr<midikraft::DataFile> patch) const
	{
		// This is an optional function to allow ignoring bytes that do not define the identity of the patch
		if (!pythonModuleHasFunction("calculateFingerprint")) {
			return Synth::calculateFingerprint(patch);
		}

		try {
			std::vector<int> data(patch->data().data(), patch->data().data() + patch->data().size());
			py::object result = callMethod("calculateFingerprint", data);
			return result.cast<std::string>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling calculateFingerprint: %s") % ex.what()).str());
			return {};
		}
	}

	std::vector<juce::MidiMessage> GenericAdaptation::GenericProgramDumpCapability::requestPatch(int patchNo) const
	{
		try {
			int c = me_->channel().toZeroBasedInt();
			py::object result = me_->callMethod("createProgramDumpRequest", c, patchNo);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling createProgramDumpRequest: %s") % ex.what()).str());
			return {};
		}
	}

	bool GenericAdaptation::GenericProgramDumpCapability::isSingleProgramDump(const MidiMessage& message) const
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

	std::vector<int> GenericAdaptation::messageToVector(MidiMessage const &message) {
		return std::vector<int>(message.getRawData(), message.getRawData() + message.getRawDataSize());
	}

	std::vector<uint8> GenericAdaptation::intVectorToByteVector(std::vector<int> const &data) {
		std::vector<uint8> byteData;
		for (int byte : data) {
			if (byte >= 0 && byte < 256) {
				byteData.push_back((uint8)byte);
			}
			else {
				throw std::runtime_error("Adaptation: Value out of range in Midi Message");
			}
		}
		return byteData;
	}

	juce::MidiMessage GenericAdaptation::vectorToMessage(std::vector<int> const &data)
	{
		auto byteData = intVectorToByteVector(data);
		return MidiMessage(byteData.data(), (int)byteData.size());
	}

	bool GenericAdaptation::hasCapability(midikraft::EditBufferCapability **outCapability)
	{
		if (pythonModuleHasFunction("isEditBufferDump")
			&& pythonModuleHasFunction("createEditBufferRequest")
			&& pythonModuleHasFunction("convertToEditBuffer")) {
			*outCapability = dynamic_cast<midikraft::EditBufferCapability *>(editBufferCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::EditBufferCapability> &outCapability)
	{
		midikraft::EditBufferCapability *cap;
		if (hasCapability(&cap)) {
			outCapability = editBufferCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(midikraft::ProgramDumpCabability  **outCapability)
	{
		if (pythonModuleHasFunction("isSingleProgramDump")
			&& pythonModuleHasFunction("createProgramDumpRequest")
			&& pythonModuleHasFunction("convertToProgramDump")) {
			*outCapability = dynamic_cast<midikraft::ProgramDumpCabability *>(programDumpCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::ProgramDumpCabability> &outCapability)
	{
		midikraft::ProgramDumpCabability *cap;
		if (hasCapability(&cap)) {
			outCapability = programDumpCapabilityImpl_;
			return true;
		}
		return false;
	}

}
