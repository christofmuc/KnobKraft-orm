/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "PatchHolder.h"
#include "HasBanksCapability.h"
#include "MidiProgramNumber.h"

#include <memory>
#include <string>
#include <vector>

namespace test_helpers {

class DummyPatch : public midikraft::Patch {
public:
	DummyPatch() : midikraft::Patch(0) {}

	MidiProgramNumber patchNumber() const override {
		return MidiProgramNumber::invalidProgram();
	}
};

class DummySynth : public midikraft::Synth, public midikraft::HasBanksCapability {
public:
	explicit DummySynth(std::string name, int bankSize = 2)
		: name_(std::move(name)), bankSize_(bankSize) {}

	std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber) const override {
		auto patch = std::make_shared<DummyPatch>();
		patch->setData(data);
		return patch;
	}

	bool isOwnSysex(juce::MidiMessage const&) const override { return true; }

	std::string getName() const override { return name_; }

	int numberOfBanks() const override { return 1; }
	int numberOfPatches() const override { return bankSize_; }
	std::string friendlyBankName(MidiBankNumber) const override { return "Dummy Bank"; }
	std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber) const override { return {}; }

private:
	std::string name_;
	int bankSize_;
};

inline midikraft::PatchHolder makePatchHolder(std::shared_ptr<DummySynth> synth, std::string const& name, std::vector<uint8> data) {
	auto patch = std::make_shared<DummyPatch>();
	patch->setData(data);
	auto sourceInfo = std::make_shared<midikraft::FromFileSource>(
		name + ".syx",
		"/tmp/" + name + ".syx",
		MidiProgramNumber::invalidProgram());
	midikraft::PatchHolder holder(synth, sourceInfo, patch);
	holder.setName(name);
	return holder;
}

} // namespace test_helpers
