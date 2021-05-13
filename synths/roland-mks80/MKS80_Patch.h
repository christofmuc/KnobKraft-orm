/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "MidiProgramNumber.h"
#include "DetailedParametersCapability.h"

#include "MKS80_Parameter.h"

namespace midikraft {

	class MKS80_Patch : public Patch, public DetailedParametersCapability {
	public:
		enum DataFileType {
			DF_MKS80_PATCH = 0
		};
		enum class APR_Section {
			PATCH_UPPER = (0b00110000 | 0b00000001),
			PATCH_LOWER = (0b00110000 | 0b00000010),
			TONE_UPPER = (0b00100000 | 0b00000001),
			TONE_LOWER = (0b00100000 | 0b00000010),
		};

		MKS80_Patch(MidiProgramNumber patchNumber, std::map<APR_Section, std::vector<uint8>> const &data);
		MKS80_Patch(MidiProgramNumber patchNumber, Synth::PatchData const &data);

		virtual std::string name() const override;

		virtual MidiProgramNumber patchNumber() const override;

		int value(SynthParameterDefinition const &param) const;
		SynthParameterDefinition const & paramBySysexIndex(int sysexIndex) const;

		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// Support APR format creation
		MidiMessage buildAPRMessage(APR_Section section, MidiChannel channel) const;

		//! For validating the enum class before casting
		static bool isValidAPRSection(int section);

		//! For the funky DAT format
		static std::vector<uint8> toneFromDat(std::vector<uint8> const &dat);
		static std::vector<uint8> patchesFromDat(std::vector<uint8> const &dat);

		// Helper functions to access the correct part of the sysex data
		uint8 *dataSection(APR_Section section);
		uint8 *dataSection(MKS80_Parameter::ParameterType type, MKS80_Parameter::SynthSection section);

	private:
		void copyDataSection(std::map<APR_Section, std::vector<uint8>> const &data, std::vector<uint8> &result, MKS80_Patch::APR_Section section, int expectedLength);

		MidiProgramNumber patchNumber_;
	};

}
