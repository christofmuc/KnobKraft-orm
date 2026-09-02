#include "doctest/doctest.h"

#include "BankDumpCapability.h"
#include "GenericAdaptation.h"
#include "MidiHelpers.h"
#include "Virus.h"

#include <pybind11/embed.h>

#include <memory>
#include <vector>

namespace {

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

}
