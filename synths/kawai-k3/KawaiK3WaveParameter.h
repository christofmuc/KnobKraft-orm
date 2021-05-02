/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

#include "Additive.h"
#include "DrawbarOrgan.h"

namespace midikraft {

	class KawaiK3HarmonicsParameters : public SynthParameterDefinition {
	public:
		// SynthParameterDefinition
		virtual ParamType type() const override;
		virtual std::string name() const override;
		virtual std::string description() const override;
		virtual std::string valueInPatchToText(DataFile const& patch) const override;

		// K3 specific
		static Additive::Harmonics toHarmonics(DataFile const& patch);
		static void fromHarmonics(const Additive::Harmonics& harmonics, DataFile& patch);

	};

	class KawaiK3DrawbarParameters : public SynthParameterDefinition, public SynthIntParameterCapability,
		public SynthParameterEditorCapability, public SynthParameterLiveEditCapability
	{
	public:
		KawaiK3DrawbarParameters(Drawbar& drawbar) : drawbar_(drawbar) {}
		KawaiK3DrawbarParameters(int harmonic);

		// SynthParameterDefinition
		ParamType type() const override;
		std::string name() const override;
		std::string description() const override;
		std::string valueInPatchToText(DataFile const& patch) const override;

		// SynthIntParameterCapability
		virtual int maxValue() const override;
		virtual int minValue() const override;
		virtual int sysexIndex() const override;
		virtual bool valueInPatch(DataFile const& patch, int& outValue) const override;
		virtual void setInPatch(DataFile& patch, int value) const override;

		// SynthParameterEditorCapability
		std::shared_ptr<TypedNamedValue> makeTypedNamedValue() override;

		// SynthParameterLiveEditCapability
		std::vector<MidiMessage> setValueMessages(std::shared_ptr<DataFile>, Synth const* synth) const override;

	private:
		Drawbar drawbar_;
	};

}
