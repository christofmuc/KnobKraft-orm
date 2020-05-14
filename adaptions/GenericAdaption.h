/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "JuceHeader.h"

#include "Synth.h"
#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"

#include <pybind11/embed.h>

namespace knobkraft {


	class GenericAdaption : public midikraft::Synth, public midikraft::SimpleDiscoverableDevice, public midikraft::EditBufferCapability, public midikraft::ProgramDumpCabability {
	public:
		GenericAdaption(std::string const &pythonModuleFilePath);

		// This needs to be implemented, and never changed, as the result is used as a primary key in the database to store the patches
		std::string getName() const override;

		// Implement hints for the UI of the Librarian
		int numberOfBanks() const override;
		int numberOfPatches() const override;
		std::string friendlyBankName(MidiBankNumber bankNo) const override;
	
		// Implement the methods needed for device detection
		std::vector<juce::MidiMessage> deviceDetect(int channel) override;
		int deviceDetectSleepMS() override;
		MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		bool needsChannelSpecificDetection() override;

		MidiMessage requestEditBufferDump() override;

		// EditBufferCapability
		bool isEditBufferDump(const MidiMessage& message) const override;
		std::shared_ptr<midikraft::Patch> patchFromSysex(const MidiMessage& message) const override;
		std::vector<MidiMessage> patchToSysex(const midikraft::Patch &patch) const override;
		MidiMessage saveEditBufferToProgram(int programNumber) override;

		// ProgramDumpCapability
		virtual std::vector<MidiMessage> requestPatch(int patchNo) const override;
		virtual bool isSingleProgramDump(const MidiMessage& message) const override;
		virtual std::shared_ptr<midikraft::Patch> patchFromProgramDumpSysex(const MidiMessage& message) const override;
		virtual std::vector<MidiMessage> patchToProgramDumpSysex(const midikraft::Patch &patch) const override;

		// The following functions are implemented generically and current cannot be defined in Python
		std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		bool isOwnSysex(MidiMessage const &message) const override;

		// Internal workings of the Generic Adaption module
		static void startupGenericAdaption();
		static File getAdaptionDirectory();
		static std::vector<std::shared_ptr<midikraft::SimpleDiscoverableDevice>> allAdaptions();
		static CriticalSection multiThreadGuard;

	private:
		template <typename ... Args> pybind11::object callMethod(std::string const &methodName, Args& ... args) const;

		static std::vector<int> messageToVector(MidiMessage const &message);
		static std::vector<uint8> intVectorToByteVector(std::vector<int> const &data);
		static MidiMessage vectorToMessage(std::vector<int> const &data);

		pybind11::module adaption_module;
		std::string filepath_;
	};

}
