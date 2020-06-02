/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KawaiK3WaveParameter.h"

#include "Patch.h"
#include "KawaiK3.h"

#include "MidiHelpers.h"

#include <boost/format.hpp>

namespace midikraft {

	KawaiK3DrawbarParameters::KawaiK3DrawbarParameters(int harmonic) : drawbar_(Drawbar(DrawbarOrgan::hammondDrawbars()[0]))
	{
		// Let's see if we find it in the Hammond definition
		for (auto hammond : DrawbarOrgan::hammondDrawbars()) {
			if (hammond.harmonic_number_ == harmonic) {
				drawbar_ = hammond;
				return;
			}
		}
		jassertfalse;
	}

	midikraft::SynthParameterDefinition::ParamType KawaiK3DrawbarParameters::type() const
	{
		return midikraft::SynthParameterDefinition::ParamType::INT;
	}

	std::string KawaiK3DrawbarParameters::name() const
	{
		return drawbar_.name_;
	}

	std::string KawaiK3DrawbarParameters::description() const
	{
		return name();
	}

	std::string KawaiK3DrawbarParameters::valueInPatchToText(DataFile const& patch) const
	{
		int outValue;
		if (valueInPatch(patch, outValue)) {
			return (boost::format("Drawbar %s at %d") % drawbar_.name_ % outValue).str();
		}
		return "invalid";
	}

	int KawaiK3DrawbarParameters::maxValue() const
	{
		return 31;
	}

	int KawaiK3DrawbarParameters::minValue() const
	{
		return 0;
	}

	int KawaiK3DrawbarParameters::sysexIndex() const
	{
		jassertfalse;
		return drawbar_.harmonic_number_;
	}

	bool KawaiK3DrawbarParameters::valueInPatch(DataFile const& patch, int& outValue) const
	{
		switch (patch.dataTypeID()) {
		case KawaiK3::K3_PATCH: {
			if (patch.data().size() != 98) return false;
			for (int i = 34; i < 64 + 34; i += 2) {
				int harmonic = patch.data()[i];
				if (harmonic == drawbar_.harmonic_number_) {
					outValue = patch.data()[i + 1];
					return true;
				}
			}
			return false;
		}
		case KawaiK3::K3_WAVE:
			jassert(patch.data().size() == 64);
			for (int i = 0; i < 64; i += 2) {
				int harmonic = patch.data()[i];
				if (harmonic == drawbar_.harmonic_number_) {
					outValue = patch.data()[i + 1];
					return true;
				}
			}
			return false;
		default:
			jassertfalse;
			return false;
		}
	}

	void KawaiK3DrawbarParameters::setInPatch(DataFile& patch, int value) const
	{
		auto harmonics = KawaiK3HarmonicsParameters::toHarmonics(patch);
		harmonics.setHarmonic(drawbar_.harmonic_number_, value / 31.0f);
		KawaiK3HarmonicsParameters::fromHarmonics(harmonics, patch);
	}

	std::shared_ptr<TypedNamedValue> KawaiK3DrawbarParameters::makeTypedNamedValue()
	{
		return std::make_shared<TypedNamedValue>(name(), "KawaiK3", 0, 0, 31);
	}

	juce::MidiBuffer KawaiK3DrawbarParameters::setValueMessages(std::shared_ptr<DataFile> const patch, Synth const* synth) const
	{		
		auto k3 = dynamic_cast<KawaiK3 const *>(synth);
		if (k3) {
			auto message = k3->patchToSysex(patch->data(), static_cast<int>(KawaiK3::WaveType::USER_WAVE), true);
			return MidiHelpers::bufferFromMessages({ message });
		}
		else {
			jassertfalse;
			return {};
		}
	}

	midikraft::SynthParameterDefinition::ParamType KawaiK3HarmonicsParameters::type() const
	{
		return SynthParameterDefinition::ParamType::INT_ARRAY;
	}

	std::string KawaiK3HarmonicsParameters::name() const
	{
		return "Wave Harmonics";
	}

	std::string KawaiK3HarmonicsParameters::description() const
	{
		return "Wave Harmonics";
	}

	std::string KawaiK3HarmonicsParameters::valueInPatchToText(DataFile const& patch) const
	{
		auto harmonics = toHarmonics(patch);
		if (harmonics.harmonics().size() > 0) {
			std::string result;
			for (auto harmonic : harmonics.harmonics()) {
				result += (boost::format("#%d %d ") % harmonic.first % harmonic.second).str();
			}
			return result;
		}
		else {
			return "no user wave data";
		}
	}

	Additive::Harmonics KawaiK3HarmonicsParameters::toHarmonics(DataFile const& patch)
	{
		Additive::Harmonics result;
		int startIndex = 0;
		switch (patch.dataTypeID()) {
		case KawaiK3::K3_PATCH:
			if (patch.data().size() == 99) {
				startIndex = 34;
			}
			else {
				// This patch has no data
				return result;
			}
			// Fall through
		case KawaiK3::K3_WAVE:
			for (int i = startIndex; i < startIndex + 64; i += 2) {
				result.setHarmonic(patch.at(i), patch.at(i + 1) / 31.0f);
				if (patch.at(i) == 0) {
					// It seems a 0 entry stopped the series
					break;
				}
			}
			return result;
		default:
			jassertfalse;
		}
		return result;
	}

	void KawaiK3HarmonicsParameters::fromHarmonics(const Additive::Harmonics& harmonics, DataFile& patch) {
		size_t writeIndex = 0;
		Synth::PatchData data = patch.data();
		switch (patch.dataTypeID()) {
		case KawaiK3::K3_PATCH:
			writeIndex = 34;
			if (data.size() == 34) {
				// This was a patch without user wave data, it's data area needs to be enlarged
				//TODO there is better ways to do this
				while (data.size() < 98) data.push_back(0);
			}
			break;
		case KawaiK3::K3_WAVE:
			data = std::vector<uint8>(64);
		}

		// Fill the harmonic array appropriately
		for (auto harmonic : harmonics.harmonics()) {
			// Ignore zero harmonic definitions - that's the default, and would make the K3 stop looking at the following harmonics
			uint8 harmonicAmp = (uint8)roundToInt(harmonic.second * 31.0f);
			if (harmonicAmp > 0) {
				if (harmonic.first >= 1 && harmonic.first <= 128) {
					data[writeIndex] = (uint8)harmonic.first;
					data[writeIndex + 1] = harmonicAmp;
					writeIndex += 2;
				}
				else {
					jassertfalse;
				}
			}
		}
		patch.setData(data);
	}

}
