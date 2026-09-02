#include "doctest/doctest.h"

#include "BankDumpCapability.h"
#include "GenericAdaptation.h"
#include "MidiHelpers.h"
#include "MidiController.h"
#include "Virus.h"
#include "test_helpers.h"

#include <pybind11/embed.h>

#include <memory>
#include <vector>

namespace {

class BankSequenceSynth : public test_helpers::DummySynth, public midikraft::BankDumpCapability {
public:
	BankSequenceSynth() : DummySynth("Bank sequence test") {}

	HandshakeReply isMessagePartOfBankDump(const juce::MidiMessage& message) const override {
		if (message.getRawDataSize() == 0) {
			resetCalls++;
			return { false, {} };
		}
		return { true, {} };
	}

	FinishedReply bankDumpFinishedWithReply(std::vector<juce::MidiMessage> const& bankDump) const override {
		if (bankDump.empty()) {
			return { false, {} };
		}
		auto code = bankDump.back().getRawData()[1];
		if (code == 0x01) {
			return { true, {}, false };
		}
		return { code == 0x02, {} };
	}

	midikraft::TPatchVector patchesFromSysexBank(std::vector<juce::MidiMessage> const& messages) const override {
		parsedBankSize = messages.size();
		auto patch = std::make_shared<test_helpers::DummyPatch>();
		patch->setData({ 0x02 });
		return { patch };
	}

	mutable int resetCalls = 0;
	mutable size_t parsedBankSize = 0;
};

TEST_CASE("generic bank dump bridge accepts nested messages and failed completion") {
	pybind11::scoped_interpreter python;
	auto adaptation = knobkraft::GenericAdaptation::fromBinaryCode(
		"bank_dump_bridge_test",
		R"(
def name():
    return "Bank dump bridge test"

def createDeviceDetectMessage(channel):
    return []

def channelIfValidDeviceResponse(message):
    return -1

def convertPatchesToBankDump(patches):
    return [[0xF0, 0x01, 0xF7], [0xF0, 0x02, 0xF7]]

def isPartOfBankDump(message):
    return False

def isBankDumpFinished(messages):
    return True, False, [[0xF0, 0x43, 0xF7]]

def extractPatchesFromAllBankMessages(messages):
    return []
)");

	REQUIRE(adaptation);
	std::shared_ptr<midikraft::BankSendCapability> sendCapability;
	REQUIRE(adaptation->hasCapability(sendCapability));
	auto bankMessages = sendCapability->createBankMessages({});
	REQUIRE(bankMessages.size() == 2);
	CHECK(knobkraft::GenericAdaptation::messageToVector(bankMessages[0]) == std::vector<int>{ 0xf0, 0x01, 0xf7 });
	CHECK(knobkraft::GenericAdaptation::messageToVector(bankMessages[1]) == std::vector<int>{ 0xf0, 0x02, 0xf7 });

	std::shared_ptr<midikraft::BankDumpCapability> dumpCapability;
	REQUIRE(adaptation->hasCapability(dumpCapability));
	auto finished = dumpCapability->bankDumpFinishedWithReply({});
	CHECK(finished.isFinished);
	CHECK_FALSE(finished.wasSuccessful);
	REQUIRE(finished.handshakeReply.size() == 1);
	CHECK(knobkraft::GenericAdaptation::messageToVector(finished.handshakeReply[0]) == std::vector<int>{ 0xf0, 0x43, 0xf7 });
}

TEST_CASE("Virus rejects truncated SysEx safely") {
	midikraft::Virus virus;
	auto headerOnly = MidiHelpers::sysexMessage({ 0x00, 0x20, 0x33, 0x01 });
	auto truncatedSingle = MidiHelpers::sysexMessage({ 0x00, 0x20, 0x33, 0x01, 0x10, 0x10, 0x01 });

	CHECK_FALSE(virus.isEditBufferDump({ headerOnly }));
	CHECK_FALSE(virus.isSingleProgramDump({ headerOnly }));
	CHECK_FALSE(virus.isEditBufferDump({ truncatedSingle }));
	CHECK_FALSE(virus.isSingleProgramDump({ truncatedSingle }));
	CHECK_FALSE(virus.bankDumpFinishedWithReply({ truncatedSingle }).isFinished);
	CHECK_FALSE(virus.channelIfValidDeviceResponse(headerOnly).isValid());
}

TEST_CASE("offline parsing clears a failed bank before a later valid bank") {
	BankSequenceSynth synth;
	auto failedBank = MidiHelpers::sysexMessage({ 0x01 });
	auto validBank = MidiHelpers::sysexMessage({ 0x02 });

	auto patches = synth.loadSysex({ failedBank, validBank });

	CHECK(midikraft::MidiController::isTimeoutMessage(midikraft::MidiController::makeTimeoutMessage()));
	REQUIRE(patches.size() == 1);
	CHECK(synth.parsedBankSize == 1);
	CHECK(synth.resetCalls == 2);
}

}
