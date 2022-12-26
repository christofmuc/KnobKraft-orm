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

#include "GenericPatch.h"
#include "GenericEditBufferCapability.h"
#include "GenericProgramDumpCapability.h"
#include "GenericBankDumpCapability.h"
#include "GenericHasBanksCapability.h"
#include "GenericHasBankDescriptorsCapability.h"

#include <pybind11/stl.h>
#include <memory>
#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

namespace py = pybind11;
using namespace py::literals;

#include <fmt/format.h>

namespace knobkraft {

	const char
		*kName = "name",
		*kNumberOfBanks = "numberOfBanks",
		*kNumberOfPatchesPerBank = "numberOfPatchesPerBank",
		*kBankDescriptors = "bankDescriptors",
		*kCreateDeviceDetectMessage = "createDeviceDetectMessage",
		*kChannelIfValidDeviceResponse = "channelIfValidDeviceResponse",
		*kNeedsChannelSpecificDetection = "needsChannelSpecificDetection",
		*kDeviceDetectWaitMilliseconds = "deviceDetectWaitMilliseconds",
		*kNameFromDump = "nameFromDump",
		*kRenamePatch = "renamePatch",
		*kIsDefaultName = "isDefaultName",
		*kIsEditBufferDump = "isEditBufferDump",
		*kIsPartOfEditBufferDump = "isPartOfEditBufferDump",
		*kCreateEditBufferRequest = "createEditBufferRequest",
		*kConvertToEditBuffer = "convertToEditBuffer",
		*kIsSingleProgramDump = "isSingleProgramDump",
		*kIsPartOfSingleProgramDump = "isPartOfSingleProgramDump",
		*kCreateProgramDumpRequest = "createProgramDumpRequest",
		*kConvertToProgramDump = "convertToProgramDump",
		*kNumberFromDump = "numberFromDump",
		*kCreateBankDumpRequest = "createBankDumpRequest",
		*kIsPartOfBankDump = "isPartOfBankDump",
		*kIsBankDumpFinished = "isBankDumpFinished",
		*kExtractPatchesFromBank = "extractPatchesFromBank",
		*kNumberOfLayers = "numberOfLayers",
		*kLayerName = "layerName",
		*kSetLayerName = "setLayerName",
		*kGeneralMessageDelay = "generalMessageDelay",
		*kCalculateFingerprint = "calculateFingerprint",
		*kFriendlyBankName = "friendlyBankName",
		*kFriendlyProgramName = "friendlyProgramName",
		*kSetupHelp = "setupHelp",
		*kGetStoredTags = "storedTags";

	std::vector<const char *> kAdapatationPythonFunctionNames = {
		kName,
		kNumberOfBanks,
		kNumberOfPatchesPerBank,
		kBankDescriptors,
		kCreateDeviceDetectMessage,
		kChannelIfValidDeviceResponse,
		kNeedsChannelSpecificDetection,
		kDeviceDetectWaitMilliseconds,
		kNameFromDump,
		kIsDefaultName,
		kRenamePatch,
		kIsEditBufferDump,
		kIsPartOfEditBufferDump,
		kCreateEditBufferRequest,
		kConvertToEditBuffer,
		kIsSingleProgramDump,
		kIsPartOfSingleProgramDump,
		kCreateProgramDumpRequest,
		kConvertToProgramDump,
		kNumberFromDump,
		kCreateBankDumpRequest,
		kIsPartOfBankDump,
		kIsBankDumpFinished,
		kExtractPatchesFromBank,
		kNumberOfLayers,
		kLayerName,
		kSetLayerName,
		kGeneralMessageDelay,
		kCalculateFingerprint,
		kFriendlyBankName,
		kFriendlyProgramName,
		kSetupHelp,
		kGetStoredTags
	};

	std::vector<const char *> kMinimalRequiredFunctionNames = {
		kName,
		kCreateDeviceDetectMessage,
		kChannelIfValidDeviceResponse,
	};

	const char *kUserAdaptationsFolderSettingsKey = "user_adaptations_folder";

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptationPythonEmbeddedGuard;
	std::unique_ptr<py::gil_scoped_release> sGenericAdaptationDontLockGIL;
	std::unique_ptr<PyStdErrOutStreamRedirect> sGenericAdaptationPyOutputRedirect;

	void checkForPythonOutputAndLog() {
		sGenericAdaptationPyOutputRedirect->flushToLogger("Adaptation");
	}

	class FatalAdaptationException : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};

	GenericAdaptation::GenericAdaptation(std::string const &pythonModuleFilePath) : filepath_(pythonModuleFilePath)
	{
		py::gil_scoped_acquire acquire;
		editBufferCapabilityImpl_ = std::make_shared<GenericEditBufferCapability>(this);
		programDumpCapabilityImpl_ = std::make_shared<GenericProgramDumpCapability>(this);
		bankDumpCapabilityImpl_ = std::make_shared<GenericBankDumpCapability>(this);
		hasBanksCapabilityImpl_ = std::make_shared<GenericHasBanksCapability>(this);
		hasBankDescriptorsCapabilityImpl_= std::make_shared<GenericHasBankDescriptorsCapability>(this);
		try {
			// Validate that the filename is a good idea
			/*auto result = py::dict("filename"_a = pythonModuleFilePath);
			py::exec(R"(
				import re
				python_identifier = re.compile(r"^[^\d\W]\w*\Z")
				matches = re.match(python_identifier, filename) is not None
			)", py::globals(), result);
			if (!result["matches"].cast<bool>()) {
				SimpleLogger::instance()->postMessage(fmt::format("Adaptation: Warning: file name %s is not a valid module identifier in Python, please use only lower case letters and numbers") % pythonModuleFilePath).str());
			}*/
			adaptation_module = py::module::import(filepath_.c_str());
			checkForPythonOutputAndLog();
			adaptationName_ = getName(); //TODO - shouldn't call a virtual method here!
		}
		catch (py::error_already_set &ex) {
			spdlog::error("Adaptation: Failure loading python module {}: {}", pythonModuleFilePath, ex.what());
			ex.restore();
			throw FatalAdaptationException("Cannot initialize Adaptation");
		}
		catch (std::exception &ex) {
			spdlog::error("Adaptation: Failure loading python module {}: {}", pythonModuleFilePath, ex.what());
			throw FatalAdaptationException("Cannot initialize Adaptation");
		}
	}

	GenericAdaptation::GenericAdaptation(pybind11::module adaptationModule)
	{
		py::gil_scoped_acquire acquire;
		editBufferCapabilityImpl_ = std::make_shared<GenericEditBufferCapability>(this);
		programDumpCapabilityImpl_ = std::make_shared<GenericProgramDumpCapability>(this);
		bankDumpCapabilityImpl_ = std::make_shared<GenericBankDumpCapability>(this);
		adaptation_module = adaptationModule;		
	}

	std::shared_ptr<GenericAdaptation> GenericAdaptation::fromBinaryCode(std::string moduleName, std::string adaptationCode)
	{
		py::gil_scoped_acquire acquire;
		try {
			auto importlib = py::module::import("importlib.util");
			checkForPythonOutputAndLog();
			auto spec = importlib.attr("spec_from_loader")(moduleName, py::none()); // Create an empty module with the right name
			auto adaptation_module = importlib.attr("module_from_spec")(spec);
			auto builtins = py::module::import("builtins");
			adaptation_module.attr("__builtins__") = builtins; // That seems to be implementation depend... https://docs.python.org/3/library/builtins.html
			checkForPythonOutputAndLog();
			auto locals = py::dict("adaptation_name"_a = moduleName, "this_module"_a = adaptation_module);
			py::exec(R"(
				import sys
				sys.modules[adaptation_name] = this_module
			)", py::globals(), locals);
			checkForPythonOutputAndLog();
			py::exec(adaptationCode, adaptation_module.attr("__dict__")); // Now run the define statements in the code, creating the defines within the right namespace
			checkForPythonOutputAndLog();
			auto newAdaptation = std::make_shared<GenericAdaptation>(py::cast<py::module>(adaptation_module));
			//if (newAdaptation) newAdaptation->logNamespace();
			newAdaptation->adaptationName_ = newAdaptation->getName();
			return newAdaptation;
		}
		catch (py::error_already_set &ex) {
			spdlog::error("Adaptation: Failure loading python module {}: {}", moduleName, ex.what());
			ex.restore();
		}
		catch (std::exception &ex) {
			spdlog::error("Adaptation: Failure loading python module {}: {}", moduleName, ex.what());
		}
		return nullptr;
	}

	void GenericAdaptation::logNamespace() {
		py::gil_scoped_acquire acquire;
		try {
			auto name = py::cast<std::string>(adaptation_module.attr("__name__"));
			auto moduleDict = adaptation_module.attr("__dict__");
			for (auto a : moduleDict) {
				spdlog::debug("Found in {} attribute {}", name , py::cast<std::string>(a));
			}
		}
		catch (py::error_already_set &ex) {
			spdlog::error("Adaptation: Failure inspecting python module: {}", ex.what());
			ex.restore();
		}
		catch (std::exception &ex) {
			spdlog::error("Adaptation: Failure inspecting python module: {}", ex.what());
		}
	}

	void GenericAdaptation::startupGenericAdaptation()
	{
		if (juce::SystemStats::getEnvironmentVariable("ORM_NO_PYTHON", "NOTSET") != "NOTSET") {
			// This is the hard coded way to turn off python integration, just set the ORM_NO_PYTHON environment variable to anything (except NOTSET)
			return;
		}

#ifdef __APPLE__
		// macOS might not have Python 3.10 installed. We will check if we can find the appropriate Framework directory, and turn Python off in case we can't find it.
		std::list<String> pythonCandidatePaths = {
		    "/Library/Frameworks/Python.framework/Versions/3.10",               // Python Mac installer (python.org/downloads)
		    "/usr/local/opt/python3/Frameworks/Python.framework/Versions/3.10", // Homebrew: python3
		    "/opt/local/Library/Frameworks/Python.framework/Versions/3.10"      // MacPorts: python310
		};
		String userPythonPath = juce::SystemStats::getEnvironmentVariable("ORM_PYTHON", "");
		if (userPythonPath != "") {
			pythonCandidatePaths.emplace_front(userPythonPath);
		}
		bool pythonFound = false;

		for (auto candidate : pythonCandidatePaths) {
		    File pythonHome(candidate);
		    if (pythonHome.exists()) {
		        Py_SetPythonHome(const_cast<wchar_t*>(candidate.toWideCharPointer()));
		        pythonFound = true;
		        break;
		    }
		}

		if (!pythonFound) {
		    // No Python found, don't set path
		    return;
		}
#endif
		sGenericAdaptationPythonEmbeddedGuard = std::make_unique<py::scoped_interpreter>();
		sGenericAdaptationPyOutputRedirect = std::make_unique<PyStdErrOutStreamRedirect>();
        File pathToTheOrm = File::getSpecialLocation (File::SpecialLocationType::currentExecutableFile).getParentDirectory();
        std::cout << pathToTheOrm.getFullPathName().toStdString() << std::endl;
		std::string command = "import sys\nsys.path.append(R\"" + getAdaptationDirectory().getFullPathName().toStdString() + "\")\n"
				+ "sys.path.append(R\"" + pathToTheOrm.getFullPathName().toStdString() + "\")\n" // This is where Linux searches
				+ "sys.path.append(R\"" + pathToTheOrm.getChildFile("adaptations").getFullPathName().toStdString() + "\")\n" // This is where we place the adaptation modules
				+ "sys.path.append(R\"" + pathToTheOrm.getChildFile("python").getFullPathName().toStdString() + "\")\n"; // This is the path in the Mac DMG
		py::exec(command);
#ifdef __APPLE__
		// For Apple (probably for Linux as well?) we need to append the path "python" to the python sys path, so it will find 
		// python code we are installing, e.g. the generic sequential module which is used by all Sequential synths
		File pythonPath2 = File::getSpecialLocation(File::SpecialLocationType::currentExecutableFile).getParentDirectory().getChildFile("python");
		command = "import sys\nsys.path.append(R\"" + pythonPath2.getFullPathName().toStdString() + "\")\n";
		py::exec(command);
#endif
		checkForPythonOutputAndLog();
		sGenericAdaptationDontLockGIL = std::make_unique<py::gil_scoped_release>();
		// From this point on, whenever you want to call into python you need to acquire the GIL 
		// with:
		// 
		// py::gil_scoped_acquire acquire;
	}

	void GenericAdaptation::shutdownGenericAdaptation()
	{
		// Remove the global release on Python, else the destruction code will fail!
		{
			py::gil_scoped_acquire acquire;
			sGenericAdaptationPyOutputRedirect.reset();
		}
		sGenericAdaptationDontLockGIL.reset();
		sGenericAdaptationPythonEmbeddedGuard.reset();
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

	[[nodiscard]] bool GenericAdaptation::createCompiledAdaptationModule(std::string const &pythonModuleName, std::string const &adaptationCode, std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> &outAddToThis) {
		py::gil_scoped_acquire acquire;
		auto newAdaptation = GenericAdaptation::fromBinaryCode(pythonModuleName, adaptationCode);
		if (newAdaptation) {
// Now we need to check the name of the compiled adaptation just created, and if it is already present. If yes, don't add it but rather issue a warning
auto newAdaptationName = newAdaptation->getName();
if (newAdaptationName != "invalid") {
	for (auto existing : outAddToThis) {
		if (existing->getName() == newAdaptationName) {
			spdlog::warn("Overriding built-in adaptation {} (found in user directory {})",
				newAdaptationName, getAdaptationDirectory().getFullPathName().toStdString());
			return true; // Was created successfully, but still is ignored.
		}
	}
	outAddToThis.push_back(newAdaptation);
	return true;
}
else {
	jassertfalse;
	spdlog::error("Program error: built-in adaptation {} failed to report name", std::string(pythonModuleName));
}
		}
		return false;
	}

	std::vector<std::shared_ptr<GenericAdaptation>> GenericAdaptation::allAdaptationsInOneDirectory(std::string const& directory)
	{
		std::vector<std::shared_ptr<GenericAdaptation>> result;
		File adaptationDirectory(directory);
		if (adaptationDirectory.exists() && adaptationDirectory.isDirectory()) {
			for (auto f : adaptationDirectory.findChildFiles(File::findFiles, false, "*.py")) {
				try {
					if (!f.getFileName().startsWith("test_") && f.getFileName() != "conftest.py") {
						result.push_back(std::make_shared<GenericAdaptation>(f.getFileNameWithoutExtension().toStdString()));
					}
				}
				catch (FatalAdaptationException&) {
					spdlog::error("Unloading adaptation module {}", String(f.getFullPathName()));
				}
			}
		}
		else {
			spdlog::warn("Directory given '{}' does not exist or is not a directory", directory);
		}
		return result;
	}

	std::vector<std::shared_ptr<GenericAdaptation>> GenericAdaptation::allAdaptations()
	{
		std::vector<std::shared_ptr<GenericAdaptation>> result;
		if (!hasPython()) {
#ifdef __APPLE__
			spdlog::warn("Couldn't find a Python 3.10 installation. Please install using Homebrew (brew install python3), MacPorts (sudo port install python310) or from https://www.python.org/ftp/python/. Turning off all adaptations.");
#else
			spdlog::warn("Couldn't find a matching Python installation. Please install from https://www.python.org/downloads/. Turning off all adaptations.");
#endif
			return result;
		}

		// First, load user defined adaptations from the directory
		File adaptationDirectory = getAdaptationDirectory();
		if (adaptationDirectory.exists()) {
			//result = allAdaptationsInOneDirectory(adaptationDirectory.getFullPathName().toStdString());
		}

		// Then, load all adaptations in the directory of the current executable
		auto installDirectory = File::getSpecialLocation(File::SpecialLocationType::currentExecutableFile).getParentDirectory().getChildFile("adaptations");
		/*auto builtIns = allAdaptationsInOneDirectory(installDirectory.getFullPathName().toStdString());
		for (auto& builtin : builtIns)
		{
			if (std::none_of(result.begin(), result.end(), [&](std::shared_ptr<midikraft::SimpleDiscoverableDevice> device) { return device->getName() == builtin->getName(); }))
			{
				result.push_back(builtin);
			}
			else
			{
				SimpleLogger::instance()->postMessage(fmt::format("Overriding built-in adaptation %s (found in user directory %s)")
					% builtin->getName() % getAdaptationDirectory().getFullPathName().toStdString()).str());
			}
		}*/
		return result;
	}

	std::vector<std::string> GenericAdaptation::getAllBuiltinSynthNames()
	{
		std::vector<std::string> result;
		auto installDirectory = File::getSpecialLocation(File::SpecialLocationType::currentExecutableFile).getParentDirectory().getChildFile("adaptations");
		auto builtIns = allAdaptationsInOneDirectory(installDirectory.getFullPathName().toStdString());
		for (auto a : builtIns) {
			result.push_back(a->getName());
		}
		return result;
	}

	bool GenericAdaptation::breakOut(std::string synthName)
	{
		// Find it
		std::shared_ptr<GenericAdaptation> adaptation;
		auto installDirectory = File::getSpecialLocation(File::SpecialLocationType::currentExecutableFile).getParentDirectory().getChildFile("adaptations");
		auto builtIns = allAdaptationsInOneDirectory(installDirectory.getFullPathName().toStdString());
		for (auto a : builtIns) {
			if (a->getName() == synthName) {
				adaptation = a;
				break;
			}
		}
		if (!adaptation) {
			spdlog::error("Program error - could not find adaptation for synth {}", synthName);
			return false;
		}

		auto dir = GenericAdaptation::getAdaptationDirectory();

		// Copy out source code
		File sourceFile(adaptation->getSourceFilePath());
		if (!sourceFile.existsAsFile()) {
			spdlog::error("Program error - could not find source code for module to break out at {}", adaptation->getSourceFilePath());
			return false;
		}
		File target = dir.getChildFile(sourceFile.getFileName());
		if (target.exists()) {
			juce::AlertWindow::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "File exists", "There is already a file for this adaptation, which we will not overwrite.");
			return false;
		}

		if (!sourceFile.copyFileTo(target))
		{
			spdlog::error("Program error - could not copy {} to {}", adaptation->getSourceFilePath(), target.getFullPathName().toStdString());
			return false;
		}
		else 
		{
			return true;
		}
	}


	bool GenericAdaptation::pythonModuleHasFunction(std::string const &functionName) const {
		py::gil_scoped_acquire acquire;
		if (!adaptation_module) {
			return false;
		}
		return py::hasattr(*adaptation_module, functionName.c_str());
	}

	bool GenericAdaptation::isFromFile() const
	{
		return !filepath_.empty();
	}

	std::string GenericAdaptation::getSourceFilePath() const
	{
		py::gil_scoped_acquire acquire;
		return adaptation_module.attr("__file__").cast<std::string>();
	}

	void GenericAdaptation::reloadPython()
	{
		py::gil_scoped_acquire acquire;
		try {
			adaptation_module.reload();
			logNamespace();
		}
		catch (py::error_already_set &ex) {
			logAdaptationError("reload module", ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError("reload module", ex);
		}
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaptation::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		py::gil_scoped_acquire acquire;
		ignoreUnused(place);
		auto patch = std::make_shared<GenericPatch>(this, const_cast<py::module &>(adaptation_module), data, GenericPatch::PROGRAM_DUMP);
		return patch;
	}

	bool GenericAdaptation::isOwnSysex(MidiMessage const &message) const
	{
		py::gil_scoped_acquire acquire;
		//TODO - if we delegate this to the python code, the "sniff synth" method of the Librarian can be used. But this is currently disabled anyway,
		// even if I forgot why
		ignoreUnused(message);
		return false;
	}

	void GenericAdaptation::sendBlockOfMessagesToSynth(juce::MidiDeviceInfo const &midiOutput, std::vector<MidiMessage> const& buffer)
	{
		py::gil_scoped_acquire acquire;
		if (pythonModuleHasFunction(kGeneralMessageDelay)) {
			try {
				auto result = callMethod(kGeneralMessageDelay);
				int delay = py::cast<int>(result);
				// Be a bit careful with this device, do specify a delay when sending messages
				midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesThrottled(buffer, delay);
			}
			catch (py::error_already_set &ex) {
				logAdaptationError(kGeneralMessageDelay, ex);
				ex.restore();
			}
			catch (std::exception &ex) {
				logAdaptationError(kGeneralMessageDelay, ex);
			}
		}
		else {
			// No special behavior - just send at full speed
			midikraft::MidiController::instance()->getMidiOutput(midiOutput)->sendBlockOfMessagesFullSpeed(buffer);
		}
	}

	std::string GenericAdaptation::friendlyProgramName(MidiProgramNumber programNo) const
	{
		py::gil_scoped_acquire acquire;
		if (pythonModuleHasFunction(kFriendlyProgramName)) {
			try {
				int zerobased = programNo.toZeroBasedWithBank();
				auto result = callMethod(kFriendlyProgramName, zerobased);
				return py::cast<std::string>(result);
			}
			catch (py::error_already_set &ex) {
				logAdaptationError(kFriendlyProgramName, ex);
				ex.restore();
			}
			catch (std::exception &ex) {
				logAdaptationError(kFriendlyProgramName, ex);
			}
		}
		return Synth::friendlyProgramName(programNo);
	}

	std::string GenericAdaptation::setupHelpText() const
	{
		py::gil_scoped_acquire acquire;
		if (!pythonModuleHasFunction("setupHelp")) {
			return Synth::setupHelpText();
		}

		try {
			return py::cast<std::string>(callMethod(kSetupHelp));
		}
		catch (py::error_already_set &ex) {
			logAdaptationError(kSetupHelp, ex);
			ex.restore();
			return Synth::setupHelpText();
		}
		catch (std::exception &ex) {
			logAdaptationError(kSetupHelp, ex);
			return Synth::setupHelpText();
		}
	}

	std::vector<juce::MidiMessage> GenericAdaptation::deviceDetect(int channel)
	{
		py::gil_scoped_acquire acquire;
		try {
			py::object result = callMethod(kCreateDeviceDetectMessage, channel);
			std::vector<uint8> byteData = intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
		}
		catch (py::error_already_set &ex) {
			logAdaptationError(kCreateDeviceDetectMessage, ex);
			ex.restore();
			return {};
		}
		catch (std::exception &ex) {
			logAdaptationError(kCreateDeviceDetectMessage, ex);
			return {};
		}
	}

	int GenericAdaptation::deviceDetectSleepMS()
	{
		py::gil_scoped_acquire acquire;
		if (!pythonModuleHasFunction(kDeviceDetectWaitMilliseconds)) {
			return 200;
		}
		try
		{
			py::object result = callMethod(kDeviceDetectWaitMilliseconds);
			return result.cast<int>();

		}
		catch (py::error_already_set &ex) {
			logAdaptationError(kDeviceDetectWaitMilliseconds, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError(kDeviceDetectWaitMilliseconds, ex);
		}
		return 200;
	}

	MidiChannel GenericAdaptation::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		py::gil_scoped_acquire acquire;
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
		catch (py::error_already_set &ex) {
			logAdaptationError(kChannelIfValidDeviceResponse, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError(kChannelIfValidDeviceResponse, ex);
		}
		return MidiChannel::invalidChannel();
	}

	bool GenericAdaptation::needsChannelSpecificDetection()
	{
		py::gil_scoped_acquire acquire;
		if (!pythonModuleHasFunction(kNeedsChannelSpecificDetection)) {
			return true;
		}
		try
		{
			py::object result = callMethod(kNeedsChannelSpecificDetection);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			logAdaptationError(kNeedsChannelSpecificDetection, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError(kNeedsChannelSpecificDetection, ex);
		}
		return true;
	}

	std::string GenericAdaptation::getName() const
	{
		py::gil_scoped_acquire acquire;
		try {
			py::object result = callMethod(kName);
			return result.cast<std::string>();
		}
		catch (py::error_already_set &ex) {
			logAdaptationError(kName, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError(kName, ex);
		}
		return "Invalid";
	}

	std::string GenericAdaptation::calculateFingerprint(std::shared_ptr<midikraft::DataFile> patch) const
	{
		py::gil_scoped_acquire acquire;
		// This is an optional function to allow ignoring bytes that do not define the identity of the patch
		if (!pythonModuleHasFunction(kCalculateFingerprint)) {
			return Synth::calculateFingerprint(patch);
		}
		
		try {
			std::vector<int> data(patch->data().data(), patch->data().data() + patch->data().size());
			py::object result = callMethod(kCalculateFingerprint, data);
				return result.cast<std::string>();
			}			
		catch (py::error_already_set &ex) {
			logAdaptationError(kCalculateFingerprint, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			logAdaptationError(kCalculateFingerprint, ex);
		}
		return {};
	}

	std::vector<int> GenericAdaptation::messageToVector(MidiMessage const &message) {
		return std::vector<int>(message.getRawData(), message.getRawData() + message.getRawDataSize());
	}

	std::vector<int> GenericAdaptation::midiMessagesToVector(std::vector<MidiMessage> const& message)
	{
		std::vector<int> result;
		for (auto const& m : message) {
			std::copy(m.getRawData(), m.getRawData() + m.getRawDataSize(), std::back_inserter(result));
		}
		return result;
	}

	std::vector<uint8> GenericAdaptation::intVectorToByteVector(std::vector<int> const& data) {
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

	std::vector<juce::MidiMessage> GenericAdaptation::vectorToMessages(std::vector<int> const& data)
	{
		//TODO this could be accelerated
		auto byteData = intVectorToByteVector(data);
		return Sysex::vectorToMessages(byteData);
	}

	bool GenericAdaptation::hasCapability(midikraft::EditBufferCapability** outCapability) const
	{
		py::gil_scoped_acquire acquire;
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
		py::gil_scoped_acquire acquire;
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
		py::gil_scoped_acquire acquire;
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

	bool GenericAdaptation::hasCapability(midikraft::HasBanksCapability** outCapability) const
	{
		py::gil_scoped_acquire acquire;
		if (pythonModuleHasFunction(kNumberOfBanks)
			&& pythonModuleHasFunction(kNumberOfPatchesPerBank))
		{
			*outCapability = dynamic_cast<midikraft::HasBanksCapability*>(hasBanksCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::HasBanksCapability>& outCapability) const
	{
		midikraft::HasBanksCapability* cap;
		if (hasCapability(&cap)) {
			outCapability = hasBanksCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(midikraft::HasBankDescriptorsCapability** outCapability) const
	{
		py::gil_scoped_acquire acquire;
		if (pythonModuleHasFunction(kBankDescriptors))
		{
			*outCapability = dynamic_cast<midikraft::HasBankDescriptorsCapability*>(hasBankDescriptorsCapabilityImpl_.get());
			return true;
		}
		return false;
	}

	bool GenericAdaptation::hasCapability(std::shared_ptr<midikraft::HasBankDescriptorsCapability>& outCapability) const
	{
		midikraft::HasBankDescriptorsCapability* cap;
		if (hasCapability(&cap)) {
			outCapability = hasBankDescriptorsCapabilityImpl_;
			return true;
		}
		return false;
	}

	void GenericAdaptation::logAdaptationError(const char *methodName, std::exception &ex) const
	{
		// This hoop is required to properly process Python created exceptions
		std::string exceptionMessage = ex.what();
		MessageManager::callAsync([this, methodName, exceptionMessage]() {
			spdlog::error("Adaptation[{}]: Error calling {}: {}", adaptationName_, methodName, exceptionMessage);
		});
	}

}
