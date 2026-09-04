#include "doctest/doctest.h"

#include "BankDumpCapability.h"
#include "GenericEditBufferCapability.h"
#include "GenericProgramDumpCapability.h"
#include "Librarian.h"
#include "MidiLocationCapability.h"
#include "ProgramDumpCapability.h"
#include "test_helpers.h"

namespace {

// Exercise the real librarian without opening a MIDI device.
class SendTestSynth : public test_helpers::DummySynth,
	public midikraft::MidiLocationCapability, public midikraft::ProgramDumpCabability {
public:
	SendTestSynth() : DummySynth("SendTestSynth", 2) {}

	MidiChannel channel() const override { return MidiChannel::fromZeroBase(0); }
	juce::MidiDeviceInfo midiInput() const override { return {}; }
	juce::MidiDeviceInfo midiOutput() const override { return {}; }
	std::vector<MidiMessage> requestPatch(int) const override { return {}; }
	bool isSingleProgramDump(const std::vector<MidiMessage>&) const override { return true; }
	MidiProgramNumber getProgramNumber(const std::vector<MidiMessage>&) const override {
		return MidiProgramNumber::fromZeroBase(0);
	}
	std::shared_ptr<midikraft::DataFile> patchFromProgramDumpSysex(const std::vector<MidiMessage>&) const override {
		return nullptr;
	}
	std::vector<MidiMessage> patchToProgramDumpSysex(std::shared_ptr<midikraft::DataFile> patch, MidiProgramNumber number) const override {
		++conversions;
		CHECK(patch != nullptr);
		programs.push_back(number.toZeroBasedWithBank());
		return { MidiMessage::programChange(1, number.toZeroBasedWithBank()) };
	}
	void sendBlockOfMessagesToSynth(juce::MidiDeviceInfo const&, std::vector<MidiMessage> const& messages) override {
		sentMessages += messages.size();
	}

	mutable int conversions = 0;
	mutable std::vector<int> programs;
	size_t sentMessages = 0;
};

class BankSendTestSynth : public SendTestSynth, public midikraft::BankSendCapability {
public:
	std::vector<MidiMessage> createBankMessages(std::vector<std::vector<MidiMessage>> patches) override {
		++bankConversions;
		std::vector<MidiMessage> result;
		for (auto const& patch : patches) {
			result.insert(result.end(), patch.begin(), patch.end());
		}
		return result;
	}
	int bankConversions = 0;
};

} // namespace

TEST_CASE("generic converters reject null patches without entering Python") {
	knobkraft::GenericProgramDumpCapability programConverter(nullptr);
	CHECK(programConverter.patchToProgramDumpSysex(nullptr, MidiProgramNumber::fromZeroBase(0)).empty());
	knobkraft::GenericEditBufferCapability editBufferConverter(nullptr);
	CHECK(editBufferConverter.patchToSysex(nullptr).empty());
}

TEST_CASE("incomplete banks are rejected before any conversion or MIDI output") {
	std::shared_ptr<SendTestSynth> synth;
	SUBCASE("individual program dumps") { synth = std::make_shared<SendTestSynth>(); }
	SUBCASE("whole bank messages") { synth = std::make_shared<BankSendTestSynth>(); }
	REQUIRE(synth);
	midikraft::UserBank bank("test-bank", "Test bank", synth, MidiBankNumber::fromZeroBase(0, 2));
	auto patch = test_helpers::makePatchHolder(synth, "Valid first patch", { 1, 2 });
	// The first slot is valid: validation must catch the later empty slot before sending it.
	bank.setPatches({});
	bank.updatePatchAtPosition(MidiProgramNumber::fromZeroBaseWithBank(bank.bankNumber(), 0), patch);
	REQUIRE(bank.isDirty());
	midikraft::Librarian librarian({});
	for (bool fullBank : { true, false }) {
		int callbacks = 0;
		librarian.sendBankToSynth(bank, fullBank, nullptr, [&](bool completed) {
			++callbacks;
			CHECK_FALSE(completed);
		});
		CHECK(callbacks == 1);
		CHECK(synth->conversions == 0);
		CHECK(synth->sentMessages == 0);
		CHECK(bank.isDirty());
	}
	if (auto bankSynth = std::dynamic_pointer_cast<BankSendTestSynth>(synth)) {
		CHECK(bankSynth->bankConversions == 0);
	}
	// No callback is also a supported caller.
	librarian.sendBankToSynth(bank, true, nullptr, {});
}

TEST_CASE("populated banks still send successfully") {
	std::shared_ptr<SendTestSynth> synth;
	SUBCASE("individual program dumps") { synth = std::make_shared<SendTestSynth>(); }
	SUBCASE("whole bank messages") { synth = std::make_shared<BankSendTestSynth>(); }
	REQUIRE(synth);
	midikraft::UserBank bank("test-bank", "Test bank", synth, MidiBankNumber::fromZeroBase(0, 2));
	auto patch = test_helpers::makePatchHolder(synth, "Patch", { 1, 2 });
	bank.setPatches({ patch, patch });
	midikraft::Librarian librarian({});
	int callbacks = 0;
	librarian.sendBankToSynth(bank, true, nullptr, [&](bool completed) {
		++callbacks;
		CHECK(completed);
	});
	CHECK(callbacks == 1);
	CHECK(synth->conversions == 2);
	CHECK(synth->sentMessages == 2);
	CHECK(synth->programs == std::vector<int>{ 0, 1 });
}
