/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "Capability.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"

#include <pybind11/embed.h>
#include <boost/format.hpp>

namespace knobkraft {

	//TODO Some forwards during refactoring
	class GenericEditBufferCapability;
	class GenericProgramDumpCapability;
	class GenericBankDumpCapability;
	void checkForPythonOutputAndLog();

	extern const char *kIsEditBufferDump, *kCreateEditBufferRequest, *kConvertToEditBuffer,
		*kNameFromDump, *kRenamePatch,
		*kIsSingleProgramDump, *kCreateProgramDumpRequest, *kConvertToProgramDump, *kNumberFromDump,
		*kCreateBankDumpRequest, *kIsPartOfBankDump, *kIsBankDumpFinished, *kExtractPatchesFromBank;

	extern std::vector<const char *> kAdapatationPythonFunctionNames;


	class GenericAdaptation : public midikraft::Synth, public midikraft::SimpleDiscoverableDevice, 
		public midikraft::RuntimeCapability<midikraft::EditBufferCapability>, 
		public midikraft::RuntimeCapability<midikraft::ProgramDumpCabability>,
		public midikraft::RuntimeCapability<midikraft::BankDumpCapability>,
		public std::enable_shared_from_this<GenericAdaptation>
	{
	public:
		GenericAdaptation(std::string const &pythonModuleFilePath);
		GenericAdaptation(pybind11::module adaptation_module);
		static std::shared_ptr<GenericAdaptation> fromBinaryCode(std::string moduleName, std::string adaptationCode);

		// This needs to be implemented, and never changed, as the result is used as a primary key in the database to store the patches
		std::string getName() const override;

		// Allow the Adaptation to implement a different fingerprint logic
		virtual std::string calculateFingerprint(std::shared_ptr<midikraft::DataFile> patch) const override;

		// Implement hints for the UI of the Librarian
		int numberOfBanks() const override;
		int numberOfPatches() const override;
		std::string friendlyBankName(MidiBankNumber bankNo) const override;
	
		// Implement the methods needed for device detection
		std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		int deviceDetectSleepMS() override;
		MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		bool needsChannelSpecificDetection() override;

		// The following functions are implemented generically and current cannot be defined in Python
		std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		bool isOwnSysex(MidiMessage const &message) const override;

		// This generic synth method is overridden to allow throttling of messages for older synths like the Korg MS2000
		virtual void sendBlockOfMessagesToSynth(std::string const& midiOutput, MidiBuffer const& buffer) override;

		// Internal workings of the Generic Adaptation module
		bool pythonModuleHasFunction(std::string const &functionName) const;

		// Call this once before using any other function
		static void startupGenericAdaptation();
		// Check if the python runtime is available
		static bool hasPython();
		// Get the current adaptation directory, this is a configurable property with default
		static File getAdaptationDirectory();
		// Configure the adaptation directory
		static void setAdaptationDirectoy(std::string const &directory);
		
		static std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> allAdaptations();
		static CriticalSection multiThreadGuard;

		static std::vector<int> messageToVector(MidiMessage const &message);
		static std::vector<uint8> intVectorToByteVector(std::vector<int> const &data);
		static MidiMessage vectorToMessage(std::vector<int> const &data);

		// Implement runtime capabilities		
		virtual bool hasCapability(std::shared_ptr<midikraft::EditBufferCapability> &outCapability) const override;
		virtual bool hasCapability(midikraft::EditBufferCapability **outCapability) const  override;
		virtual bool hasCapability(std::shared_ptr<midikraft::ProgramDumpCabability> &outCapability) const override;
		virtual bool hasCapability(midikraft::ProgramDumpCabability **outCapability) const  override;
		virtual bool hasCapability(std::shared_ptr<midikraft::BankDumpCapability> &outCapability) const override;
		virtual bool hasCapability(midikraft::BankDumpCapability **outCapability) const override;

	private:
		friend class GenericEditBufferCapability;
		std::shared_ptr<GenericEditBufferCapability> editBufferCapabilityImpl_;

		friend class GenericProgramDumpCapability;
		std::shared_ptr<GenericProgramDumpCapability> programDumpCapabilityImpl_;

		friend class GenericBankDumpCapability;
		std::shared_ptr<GenericBankDumpCapability> bankDumpCapabilityImpl_;

		template <typename ... Args> pybind11::object callMethod(std::string const &methodName, Args& ... args) const
		{
			if (!adaptation_module) {
				return pybind11::none();
			}
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			if (pybind11::hasattr(*adaptation_module, methodName.c_str())) {
				auto result = adaptation_module.attr(methodName.c_str())(args...);
				//checkForPythonOutputAndLog();
				return result;
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: method %s not found, fatal!") % methodName).str());
				return pybind11::none();
			}
		}

		// Helper function for adding the built-in adaptations
		static bool createCompiledAdaptationModule(std::string const &pythonModuleName, std::string const &adaptationCode, std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> &outAddToThis);
		void logNamespace();

		pybind11::module adaptation_module;
		std::string filepath_;
	};

}
