/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "DetailedParametersCapability.h"
#include "StoredPatchNameCapability.h"

namespace midikraft {

	enum class MKS50DataType {
		ALL = 0, 
	};

	class MKS50_Patch : public Patch, public DetailedParametersCapability, public StoredPatchNameCapability {
	public:
		MKS50_Patch(MidiProgramNumber programNumber, std::string const& patchName, Synth::PatchData const& patchData);

		virtual std::string name() const override;
		virtual bool changeNameStoredInPatch(std::string const& name) override;

		virtual MidiProgramNumber patchNumber() const override;
		//virtual void setPatchNumber(MidiProgramNumber patchNumber) override;
		
		// DetailedParametersCapability
		virtual std::vector<std::shared_ptr<SynthParameterDefinition>> allParameterDefinitions() const override;

		// MKS50 specific implementations
		static std::shared_ptr<MKS50_Patch> createFromToneBLD(MidiProgramNumber programNumber, std::vector<uint8> const& bldData);
		static std::shared_ptr<MKS50_Patch> createFromToneDAT(MidiProgramNumber programNumber, std::vector<uint8> const& datData);
		static std::shared_ptr<MKS50_Patch> createFromToneAPR(MidiMessage const& message);

		// Name encoding
		static const std::string kPatchNameChar;
		static std::string dataToString(std::vector<uint8> data);
		static std::vector<uint8> stringToData(std::string name);

	private:
		MidiProgramNumber programNumber_;
		std::string patchName_;
	};

}
