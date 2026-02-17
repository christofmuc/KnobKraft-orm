/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "Capability.h"
#include "HasBanksCapability.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "LegacyLoaderCapability.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace knobkraft {

	//TODO Some forwards during refactoring
	class GenericEditBufferCapability;
	class GenericProgramDumpCapability;
	class GenericBankDumpRequestCapability;
	class GenericBankDumpCapability;
	class GenericHasBanksCapability;
	class GenericHasBankDescriptorsCapability;
	class GenericBankDumpSendCapability;
	class GenericLegacyLoaderCapability;
	void checkForPythonOutputAndLog();

	extern const char *kIsEditBufferDump, *kIsPartOfEditBufferDump, *kCreateEditBufferRequest, *kConvertToEditBuffer,
		*kNumberOfBanks, * kNumberOfPatchesPerBank, * kBankDescriptors, * kFriendlyBankName, *kBankSelect, 
		*kNameFromDump, *kRenamePatch, *kIsDefaultName,
		*kIsSingleProgramDump, *kIsPartOfSingleProgramDump, *kCreateProgramDumpRequest, *kConvertToProgramDump, *kNumberFromDump,
		*kCreateBankDumpRequest, *kIsPartOfBankDump, *kIsBankDumpFinished, *kExtractPatchesFromBank, *kExtractPatchesFromAllBankMessages,
		*kConvertPatchesToBankDump,
		*kLegacyLoadSupportedExtensions, *kLoadPatchesFromLegacyData,
		*kNumberOfLayers,
		*kLayerTitles,
		*kLayerName,
		*kSetLayerName,
		*kGetStoredTags,
		*kMessageTimings
		;

	extern std::vector<const char *> kAdaptationPythonFunctionNames;
	extern std::vector<const char *> kMinimalRequiredFunctionNames;

	class GenericAdaptation : public midikraft::Synth, public midikraft::SimpleDiscoverableDevice,
		public midikraft::RuntimeCapability<midikraft::HasBanksCapability>,
		public midikraft::RuntimeCapability<midikraft::HasBankDescriptorsCapability>,
		public midikraft::RuntimeCapability<midikraft::EditBufferCapability>, 
		public midikraft::RuntimeCapability<midikraft::ProgramDumpCabability>,
		public midikraft::RuntimeCapability<midikraft::BankDumpCapability>,
		public midikraft::RuntimeCapability<midikraft::BankDumpRequestCapability>,
		public midikraft::RuntimeCapability<midikraft::BankSendCapability>,
		public midikraft::RuntimeCapability<midikraft::LegacyLoaderCapability>,
		public midikraft::BankDownloadMethodIndicationCapability,
		public std::enable_shared_from_this<GenericAdaptation>
	{
	public:
		GenericAdaptation(std::string const &pythonModuleFilePath);
		GenericAdaptation(pybind11::module adaptation_module);
		virtual ~GenericAdaptation() override;
		static std::shared_ptr<GenericAdaptation> fromBinaryCode(std::string moduleName, std::string adaptationCode);

		// This needs to be implemented, and never changed, as the result is used as a primary key in the database to store the patches
		std::string getName() const override;

		// Allow the Adaptation to implement a different fingerprint logic
		virtual std::string calculateFingerprint(std::shared_ptr<midikraft::DataFile> patch) const override;

		// Implement the methods needed for device detection
		std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		int deviceDetectSleepMS() override;
		int defaultReplyTimeoutMs() const override;
		MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		bool needsChannelSpecificDetection() override;

		// Allow to override the default algorithm to determine how to query a bank
		virtual midikraft::BankDownloadMethod bankDownloadMethod() const override;

		// The following functions are implemented generically and current cannot be defined in Python
		std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		bool isOwnSysex(MidiMessage const &message) const override;

		// This generic synth method is overridden to allow throttling of messages for older synths like the Korg MS2000
		virtual void sendBlockOfMessagesToSynth(juce::MidiDeviceInfo const &midiOutput, std::vector<MidiMessage> const& buffer) override;
		virtual std::string friendlyProgramName(MidiProgramNumber programNo) const override;  //TODO this looks like a capability
		virtual std::string setupHelpText() const override;

		// Internal workings of the Generic Adaptation module
		bool pythonModuleHasFunction(std::string const &functionName) const;
		bool isFromFile() const;
		std::string getSourceFilePath() const;
		void reloadPython();

		// Call this once before using any other function
		static void startupGenericAdaptation();
		// Graceful shutdown with this please
		static void shutdownGenericAdaptation();
		// Check if the python runtime is available
		static bool hasPython();
		// Get the current adaptation directory, this is a configurable property with default
		static File getAdaptationDirectory();
		// Configure the adaptation directory
		static void setAdaptationDirectoy(std::string const &directory);
		
		static std::vector<std::shared_ptr<GenericAdaptation>> allAdaptationsInOneDirectory(std::string const& directory);
		static std::vector<std::shared_ptr<GenericAdaptation>> allAdaptations();
		static std::vector<std::string> getAllBuiltinSynthNames();
		static std::optional<std::string> breakOut(std::string synthName);

		static std::vector<int> messageToVector(MidiMessage const &message);
		static std::vector<int> midiMessagesToVector(std::vector<MidiMessage> const& message);
		static std::vector<uint8> intVectorToByteVector(std::vector<int> const &data);
		static MidiMessage vectorToMessage(std::vector<int> const &data);
		static std::vector<MidiMessage> vectorToMessages(std::vector<int> const &data);

		// Implement runtime capabilities		
		virtual bool hasCapability(std::shared_ptr<midikraft::EditBufferCapability> &outCapability) const override;
		virtual bool hasCapability(midikraft::EditBufferCapability **outCapability) const  override;
		virtual bool hasCapability(std::shared_ptr<midikraft::ProgramDumpCabability> &outCapability) const override;
		virtual bool hasCapability(midikraft::ProgramDumpCabability **outCapability) const  override;
		virtual bool hasCapability(std::shared_ptr<midikraft::BankDumpCapability> &outCapability) const override;
		virtual bool hasCapability(midikraft::BankDumpCapability **outCapability) const override;
		virtual bool hasCapability(std::shared_ptr<midikraft::BankDumpRequestCapability>& outCapability) const override;
		virtual bool hasCapability(midikraft::BankDumpRequestCapability** outCapability) const override;
		virtual bool hasCapability(std::shared_ptr<midikraft::HasBanksCapability>& outCapability) const override;
		virtual bool hasCapability(midikraft::HasBanksCapability** outCapability) const override;
		virtual bool hasCapability(std::shared_ptr<midikraft::HasBankDescriptorsCapability>& outCapability) const override;
		virtual bool hasCapability(midikraft::HasBankDescriptorsCapability** outCapability) const override;
		virtual bool hasCapability(std::shared_ptr<midikraft::BankSendCapability>& outCapability) const override;
		virtual bool hasCapability(midikraft::BankSendCapability** outCapability) const override;
		virtual bool hasCapability(std::shared_ptr<midikraft::LegacyLoaderCapability>& outCapability) const override;
		virtual bool hasCapability(midikraft::LegacyLoaderCapability** outCapability) const override;

		// Common error logging
		void logAdaptationError(const char *methodName, std::exception &e) const;

		// Name cache
		bool hasName(Synth::PatchData const& patchData, std::string& outFingerprint) const;
		void insertName(Synth::PatchData const& patchData, std::string const& inFingerprint) const;

		// Fingerprint cache
		bool hasFingerprint(Synth::PatchData const& patchData, std::string& outName) const;
		void insertFingerprint(Synth::PatchData const& patchData, std::string const& inName) const;

	private:
		friend class GenericEditBufferCapability;
		std::shared_ptr<GenericEditBufferCapability> editBufferCapabilityImpl_;

		friend class GenericProgramDumpCapability;
		std::shared_ptr<GenericProgramDumpCapability> programDumpCapabilityImpl_;

		friend class GenericBankDumpCapability;
		std::shared_ptr<GenericBankDumpCapability> bankDumpCapabilityImpl_;

		friend class GenericBankDumpRequestCapability;
		std::shared_ptr<GenericBankDumpRequestCapability> bankDumpRequestCapabilityImpl_;

		friend class GenericHasBanksCapability;
		std::shared_ptr<GenericHasBanksCapability> hasBanksCapabilityImpl_;

		friend class GenericHasBankDescriptorsCapability;
		std::shared_ptr<GenericHasBankDescriptorsCapability> hasBankDescriptorsCapabilityImpl_;

		friend class GenericBankDumpSendCapability;
		std::shared_ptr<GenericBankDumpSendCapability> hasBankDumpSendCapabilityImpl_;

		friend class GenericLegacyLoaderCapability;
		std::shared_ptr<GenericLegacyLoaderCapability> legacyLoaderCapabilityImpl_;

		template <typename ... Args> pybind11::object callMethod(std::string const &methodName, Args& ... args) const
		{
			if (!adaptation_module) {
				return pybind11::none();
			}
			pybind11::gil_scoped_acquire acquire;
			if (pybind11::hasattr(*adaptation_module, methodName.c_str())) {
				auto result = adaptation_module.attr(methodName.c_str())(args...);
				checkForPythonOutputAndLog();
				return result;
			}
			else {
				spdlog::error("Adaptation {}: method {} not found, fatal!", filepath_, methodName);
				return pybind11::none();
			}
		}

		// Helper function for adding the built-in adaptations
		static bool createCompiledAdaptationModule(std::string const &pythonModuleName, std::string const &adaptationCode, std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> &outAddToThis);
		void logNamespace();

		pybind11::module adaptation_module;
		std::string filepath_;

		mutable std::map<std::string, std::string> nameCache_;
		mutable std::map<std::string, std::string> fingerprintCache_;
	};

}
