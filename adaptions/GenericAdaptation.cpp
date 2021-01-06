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
#include "GenericProgramDumpCapability.h"
#include "GenericBankDumpCapability.h"

#include <pybind11/stl.h>
//#include <pybind11/pybind11.h>
#include <memory>

#include "Python.h"

namespace py = pybind11;

#include <boost/format.hpp>

namespace knobkraft {

	const char *pythonMethodNames;
	
	const char 
		*kName = "name",
		*kNumberOfBanks = "numberOfBanks",
		*kNumberOfPatchesPerBank = "numberOfPatchesPerBank",
		*kCreateDeviceDetectMessage = "createDeviceDetectMessage",
		*kChannelIfValidDeviceResponse = "channelIfValidDeviceResponse",
		*kNeedsChannelSpecificDetection = "needsChannelSpecificDetection",
		*kDeviceDetectWaitMilliseconds = "deviceDetectWaitMilliseconds",
		*kNameFromDump = "nameFromDump",
		*kRenamePatch = "renamePatch",
		*kIsDefaultName = "isDefaultName",
		*kIsEditBufferDump = "isEditBufferDump",
		*kCreateEditBufferRequest = "createEditBufferRequest",
		*kConvertToEditBuffer = "convertToEditBuffer",
		*kIsSingleProgramDump = "isSingleProgramDump",
		*kCreateProgramDumpRequest = "createProgramDumpRequest",
		*kConvertToProgramDump = "convertToProgramDump",
		*kNumberFromDump = "numberFromDump",
		*kCreateBankDumpRequest = "createBankDumpRequest",
		*kIsPartOfBankDump = "isPartOfBankDump",
		*kIsBankDumpFinished = "isBankDumpFinished",
		*kExtractPatchesFromBank = "extractPatchesFromBank",
		*kGeneralMessageDelay = "generalMessageDelay",
		*kCalculateFingerprint = "calculateFingerprint",
		*kFriendlyBankName = "friendlyBankName",
		*kFriendlyProgramName= "friendlyProgramName";

	std::vector<const char *> kAdapatationPythonFunctionNames = {
		kName,
		kNumberOfBanks,
		kNumberOfPatchesPerBank,
		kCreateDeviceDetectMessage,
		kChannelIfValidDeviceResponse,
		kNeedsChannelSpecificDetection,
		kDeviceDetectWaitMilliseconds,
		kNameFromDump,
		kIsDefaultName,
		kRenamePatch,
		kIsEditBufferDump,
		kCreateEditBufferRequest,
		kConvertToEditBuffer,
		kIsSingleProgramDump,
		kCreateProgramDumpRequest,
		kConvertToProgramDump,
		kNumberFromDump,
		kCreateBankDumpRequest,
		kIsPartOfBankDump,
		kIsBankDumpFinished,
		kExtractPatchesFromBank,
		kGeneralMessageDelay,
		kCalculateFingerprint,
		kFriendlyBankName,
		kFriendlyProgramName,
	};

	std::vector<const char *> kMinimalRequiredFunctionNames = {
		kName,
		kNumberOfBanks,
		kNumberOfPatchesPerBank,
		kCreateDeviceDetectMessage,
		kChannelIfValidDeviceResponse,
	};

	const char *kUserAdaptationsFolderSettingsKey = "user_adaptations_folder";

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptationPythonEmbeddedGuard;
	std::unique_ptr<PyStdErrOutStreamRedirect> sGenericAdaptationPyOutputRedirect;
	CriticalSection GenericAdaptation::multiThreadGuard;

	void checkForPythonOutputAndLog() {
		sGenericAdaptationPyOutputRedirect->flushToLogger("Adaptation");
	}


	GenericAdaptation::GenericAdaptation(std::string const &pythonModuleFilePath) : filepath_(pythonModuleFilePath)
	{
		editBufferCapabilityImpl_ = std::make_shared<GenericEditBufferCapability>(this);
		programDumpCapabilityImpl_ = std::make_shared<GenericProgramDumpCapability>(this);
		bankDumpCapabilityImpl_ = std::make_shared<GenericBankDumpCapability>(this);
		try {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			adaptation_module = py::module::import(filepath_.c_str());
			checkForPythonOutputAndLog();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure loading python module: %s") % ex.what()).str());
			ex.restore();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure loading python module: %s") % ex.what()).str());
		}
	}

	GenericAdaptation::GenericAdaptation(pybind11::module adaptationModule)
	{
		editBufferCapabilityImpl_ = std::make_shared<GenericEditBufferCapability>(this);
		programDumpCapabilityImpl_ = std::make_shared<GenericProgramDumpCapability>(this);
		bankDumpCapabilityImpl_ = std::make_shared<GenericBankDumpCapability>(this);
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
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure loading python module %s: %s") % moduleName % ex.what()).str());
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
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Failure inspecting python module: %s") % ex.what()).str());
		}
	}

	void GenericAdaptation::startupGenericAdaptation()
	{
		if (juce::SystemStats::getEnvironmentVariable("ORM_NO_PYTHON", "NOTSET") != "NOTSET") {
			// This is the hard coded way to turn off python integration, just set the ORM_NO_PYTHON environment variable to anything (except NOTSET)
			return;
		}

#ifdef __APPLE__
		// The Apple might not have a Python 3.8 installed. We will check if we can find the appropriate Framework directory, and turn Python off in case we can't find it.
		// First, check the location where the Python 3.8 Mac installer will put it (taken from python.org/downloads)
		String python38_macHome = "/Library/Frameworks/Python.framework/Versions/3.8";
		File python38(python38_macHome);
		if (!python38.exists()) {
			// If that didn't work, check if the Homebrew brew install python3 command has installed it in the /usr/local/opt directory
			python38_macHome = "/usr/local/opt/python3/Frameworks/Python.framework/Versions/3.8";
			File python38_alternative(python38_macHome);
			if (!python38_alternative.exists()) {
				// No Python3.8 found, don't set path
				return;
			}
			else {
				Py_SetPythonHome(const_cast<wchar_t*>(python38_macHome.toWideCharPointer()));
			}
		}
		else {
			Py_SetPythonHome(const_cast<wchar_t*>(python38_macHome.toWideCharPointer()));
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
			SimpleLogger::instance()->postMessage("Warning - couldn't find a Python 3.8 installation. Please install using 'brew install python3' or from https://www.python.org/ftp/python/. Turning off all adaptations.");
#else
			SimpleLogger::instance()->postMessage("Warning - couldn't find a Python 3.8 installation. Please install from https://www.python.org/downloads/release/python-387/. Turning off all adaptations.");
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
			py::object result = callMethod(kNumberOfBanks);
			return result.cast<int>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kNumberOfBanks % ex.what()).str());
			ex.restore();
			return 1;
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kNumberOfBanks % ex.what()).str());
			return 1;
		}
	}

	int GenericAdaptation::numberOfPatches() const
	{
		try {
			py::object result = callMethod(kNumberOfPatchesPerBank);
			return result.cast<int>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kNumberOfPatchesPerBank % ex.what()).str());
			ex.restore();
			return 0;
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kNumberOfPatchesPerBank % ex.what()).str());
			return 0;
		}
	}

	std::string GenericAdaptation::friendlyBankName(MidiBankNumber bankNo) const
	{
		if (!pythonModuleHasFunction(kFriendlyBankName)) {
			return (boost::format("Bank %d") % bankNo.toOneBased()).str();
		}
		try {
			int bankAsInt = bankNo.toZeroBased();
			py::object result = callMethod(kFriendlyBankName, bankAsInt);
			return result.cast<std::string>();
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kFriendlyBankName % ex.what()).str());
			ex.restore();
			return "invalid name";
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kFriendlyBankName % ex.what()).str());
			return "invalid name";
		}
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaptation::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		ignoreUnused(place);
		auto patch = std::make_shared<GenericPatch>(this, const_cast<py::module &>(adaptation_module), data, GenericPatch::PROGRAM_DUMP);
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
		if (pythonModuleHasFunction(kGeneralMessageDelay)) {
			try {
				auto result = callMethod(kGeneralMessageDelay);
				int delay = py::cast<int>(result);
				// Be a bit careful with this device, do specify a delay when sending messages
				midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesThrottled(buffer, delay);
			}
			catch (py::error_already_set &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kGeneralMessageDelay % ex.what()).str());
				ex.restore();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kGeneralMessageDelay % ex.what()).str());
			}
		}
		else {
			// No special behavior - just send at full speed
			midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesFullSpeed(buffer);
		}
	}

	std::string GenericAdaptation::friendlyProgramName(MidiProgramNumber programNo) const
	{
		if (pythonModuleHasFunction(kFriendlyProgramName)) {
			try {
				int zerobased = programNo.toZeroBased();
				auto result = callMethod(kFriendlyProgramName, zerobased);
				return py::cast<std::string>(result);
			}
			catch (py::error_already_set &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kFriendlyProgramName % ex.what()).str());
				ex.restore();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kFriendlyProgramName % ex.what()).str());
			}
		}
		return Synth::friendlyProgramName(programNo);
	}

	std::vector<juce::MidiMessage> GenericAdaptation::deviceDetect(int channel)
	{
		try {
			py::object result = callMethod(kCreateDeviceDetectMessage, channel);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kCreateDeviceDetectMessage % ex.what()).str());
			ex.restore();
			return {};
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kCreateDeviceDetectMessage % ex.what()).str());
			return {};
		}
	}

	int GenericAdaptation::deviceDetectSleepMS()
	{
		if (!pythonModuleHasFunction(kDeviceDetectWaitMilliseconds)) {
			return 200;
		}
		try
		{
			py::object result = callMethod(kDeviceDetectWaitMilliseconds);
			return result.cast<int>();

		}
		catch (py::error_already_set &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kDeviceDetectWaitMilliseconds % ex.what()).str());
			ex.restore();
			return 200;
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kDeviceDetectWaitMilliseconds % ex.what()).str());
			return 200;
		}
	}

	MidiChannel GenericAdaptation::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		try {
			auto vector = messageToVector(message);
			py::object result = callMethod(kChannelIfValidDeviceResponse, vector);
			int intResult = result.cast<int>();
			if (intResult >= 0 && intResult < 16) {
				return MidiChannel::fromZeroBase(intResult);
			}
			else {
				return MidiChannel::invalidChannel();
			}
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kChannelIfValidDeviceResponse % ex.what()).str());
			return MidiChannel::invalidChannel();
		}
	}

	bool GenericAdaptation::needsChannelSpecificDetection()
	{
		if (!pythonModuleHasFunction(kNeedsChannelSpecificDetection)) {
			return true;
		}
		try
		{
			py::object result = callMethod(kNeedsChannelSpecificDetection);
			return result.cast<bool>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kNeedsChannelSpecificDetection % ex.what()).str());
			return true;
		}
	}

	std::string GenericAdaptation::getName() const
	{
		try {
			py::object result = callMethod(kName);
			return result.cast<std::string>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kName % ex.what()).str());
			return "Invalid";
		}
	}

	std::string GenericAdaptation::calculateFingerprint(std::shared_ptr<midikraft::DataFile> patch) const
	{
		// This is an optional function to allow ignoring bytes that do not define the identity of the patch
		if (!pythonModuleHasFunction(kCalculateFingerprint)) {
			return Synth::calculateFingerprint(patch);
		}

		try {
			std::vector<int> data(patch->data().data(), patch->data().data() + patch->data().size());
			py::object result = callMethod(kCalculateFingerprint, data);
			return result.cast<std::string>();
		}
		catch (std::exception &ex) {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation: Error calling %s: %s") % kCalculateFingerprint % ex.what()).str());
			return {};
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

	bool GenericAdaptation::hasCapability(midikraft::EditBufferCapability **outCapability) const
	{
		if (pythonModuleHasFunction(kIsEditBufferDump)
			&& pythonModuleHasFunction(kCreateEditBufferRequest)
			&& pythonModuleHasFunction(kConvertToEditBuffer)) {
			*outCapability = dynamic_cast<midikraft::EditBufferCapability *>(editBufferCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::EditBufferCapability> &outCapability) const
	{
		midikraft::EditBufferCapability *cap;
		if (hasCapability(&cap)) {
			outCapability = editBufferCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(midikraft::ProgramDumpCabability  **outCapability) const
	{
		if (pythonModuleHasFunction(kIsSingleProgramDump)
			&& pythonModuleHasFunction(kCreateProgramDumpRequest)
			&& pythonModuleHasFunction(kConvertToProgramDump)) {
			*outCapability = dynamic_cast<midikraft::ProgramDumpCabability *>(programDumpCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::ProgramDumpCabability> &outCapability) const
	{
		midikraft::ProgramDumpCabability *cap;
		if (hasCapability(&cap)) {
			outCapability = programDumpCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(midikraft::BankDumpCapability  **outCapability) const
	{
		if (pythonModuleHasFunction(kCreateBankDumpRequest)
			&& pythonModuleHasFunction(kExtractPatchesFromBank)
			&& pythonModuleHasFunction(kIsPartOfBankDump)
			&& pythonModuleHasFunction(kIsBankDumpFinished)) {
			*outCapability = dynamic_cast<midikraft::BankDumpCapability *>(bankDumpCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::BankDumpCapability> &outCapability) const
	{
		midikraft::BankDumpCapability *cap;
		if (hasCapability(&cap)) {
			outCapability = bankDumpCapabilityImpl_;
			return true;
		}
		return false;
	}

	void GenericAdaptation::logAdaptationError(const char *methodName, std::exception &ex)
	{
		// This hoop is required to properly process Python created exceptions
		std::string exceptionMessage = ex.what();
		MessageManager::callAsync([this, methodName, exceptionMessage]() {
			SimpleLogger::instance()->postMessage((boost::format("Adaptation[%s]: Error calling %s: %s") % getName() % methodName % exceptionMessage).str());
		});
	}

}
