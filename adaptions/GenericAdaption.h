/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "JuceHeader.h"

#include "Synth.h"

namespace knobkraft {

	class GenericAdaption : public midikraft::Synth {
	public:
		GenericAdaption(std::string const &pythonModuleFilePath);

		std::string getName() const override;

		int numberOfBanks() const override;
		int numberOfPatches() const override;
		std::string friendlyBankName(MidiBankNumber bankNo) const override;

		std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const override;
		bool isOwnSysex(MidiMessage const &message) const override;

		MidiMessage deviceDetect(int channel) override;
		int deviceDetectSleepMS() override;
		MidiChannel channelIfValidDeviceResponse(const MidiMessage &message) override;
		bool needsChannelSpecificDetection() override;

		// Internal workings of the Generic Adaption module
		static void startupGenericAdaption();

	private:
		class AdaptionImpl;
		std::unique_ptr<AdaptionImpl> impl;
	};

}
