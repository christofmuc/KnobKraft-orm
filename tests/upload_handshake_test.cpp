#include "doctest/doctest.h"

#include "Capability.h"
#include "GenericAdaptation.h"
#include "UploadHandshakeCapability.h"
#include "UploadSequence.h"

#include <pybind11/embed.h>

#include <vector>

namespace {

class TestHandshake : public midikraft::UploadHandshakeCapability {
public:
	bool expectsUploadReply(const MidiMessage& message) const override {
		return message.isSysEx();
	}

	midikraft::UploadHandshakeReply isMessagePartOfUploadReply(const MidiMessage& message, const MidiMessage&) const override {
		if (!message.isSysEx() || message.getSysExDataSize() == 0) return {};
		auto code = message.getSysExData()[0];
		if (code == 0x01) {
			return { midikraft::UploadHandshakeReply::Status::ACCEPTED, { MidiMessage::programChange(1, 9) } };
		}
		if (code == 0x02) {
			return { midikraft::UploadHandshakeReply::Status::DEVICE_ERROR, {}, "rejected", "The device rejected the write" };
		}
		return {};
	}
};

MidiMessage sysex(uint8 value) {
	return MidiMessage::createSysExMessage(&value, 1);
}

} // namespace

TEST_CASE("upload sequence acknowledges selected messages and preserves response order") {
	TestHandshake handshake;
	std::vector<MidiMessage> sent;
	std::vector<midikraft::UploadResult> results;
	midikraft::UploadSequence sequence(
		&handshake,
		{ sysex(0x20), MidiMessage::programChange(1, 7) },
		[&](const std::vector<MidiMessage>& messages) { sent.insert(sent.end(), messages.begin(), messages.end()); },
		[&](const midikraft::UploadResult& result) { results.push_back(result); });

	sequence.start();
	REQUIRE(sent.size() == 1);
	CHECK(sequence.waitingForReply());
	sequence.handleIncomingMessage(MidiMessage::programChange(1, 1));
	CHECK(sent.size() == 1);
	sequence.handleIncomingMessage(sysex(0x01));

	REQUIRE(sent.size() == 3);
	CHECK(sent[1].isProgramChange());
	CHECK(sent[1].getProgramChangeNumber() == 9);
	CHECK(sent[2].isProgramChange());
	CHECK(sent[2].getProgramChangeNumber() == 7);
	REQUIRE(results.size() == 1);
	CHECK(results[0].status == midikraft::UploadResult::Status::ACKNOWLEDGED);
	CHECK(results[0].completedMessages == 2);
}

TEST_CASE("upload sequence stops on device errors and timeout") {
	TestHandshake handshake;
	for (bool timeout : { false, true }) {
		int sends = 0;
		std::vector<midikraft::UploadResult> results;
		midikraft::UploadSequence sequence(
			&handshake,
			{ sysex(0x20), sysex(0x21) },
			[&](const std::vector<MidiMessage>&) { ++sends; },
			[&](const midikraft::UploadResult& result) { results.push_back(result); });
		sequence.start();
		if (timeout) sequence.timeout();
		else sequence.handleIncomingMessage(sysex(0x02));

		CHECK(sends == 1);
		REQUIRE(results.size() == 1);
		CHECK(results[0].status == (timeout
			? midikraft::UploadResult::Status::TIMEOUT
			: midikraft::UploadResult::Status::DEVICE_ERROR));
		CHECK(results[0].outcomeUncertain == timeout);
	}
}

TEST_CASE("generic adaptation exposes and validates upload handshake hooks") {
	pybind11::scoped_interpreter python;
	auto adaptation = knobkraft::GenericAdaptation::fromBinaryCode(
		"upload_handshake_bridge_test",
		R"(
def name():
    return "Upload handshake bridge test"

def createDeviceDetectMessage(channel):
    return []

def channelIfValidDeviceResponse(message):
    return -1

def expectsUploadReply(sent_message):
    return sent_message[1] == 0x01

def isPartOfUploadReply(message, sent_message):
    if message[1] == 0x10:
        return {"status": "continue", "messages": [0xf0, 0x7d, 0xf7]}
    if message[1] == 0x11:
        return {"status": "accepted"}
    if message[1] == 0x12:
        return {"status": "error", "code": "write_failed", "message": "Write failed"}
    if message[1] == 0x13:
        return {"status": "unknown"}
    return None

def messageTimings():
    return {"uploadReplyTimeoutMs": 1234}
)");

	REQUIRE(adaptation);
	auto capability = midikraft::Capability::hasCapability<midikraft::UploadHandshakeCapability>(adaptation);
	REQUIRE(capability);
	CHECK(capability->expectsUploadReply(sysex(0x01)));
	CHECK_FALSE(capability->expectsUploadReply(sysex(0x02)));
	CHECK(capability->uploadReplyTimeoutMs() == 1234);

	auto progress = capability->isMessagePartOfUploadReply(sysex(0x10), sysex(0x01));
	CHECK(progress.status == midikraft::UploadHandshakeReply::Status::CONTINUE);
	REQUIRE(progress.response.size() == 1);
	CHECK(knobkraft::GenericAdaptation::messageToVector(progress.response[0]) == std::vector<int>{ 0xf0, 0x7d, 0xf7 });

	auto accepted = capability->isMessagePartOfUploadReply(sysex(0x11), sysex(0x01));
	CHECK(accepted.status == midikraft::UploadHandshakeReply::Status::ACCEPTED);
	auto error = capability->isMessagePartOfUploadReply(sysex(0x12), sysex(0x01));
	CHECK(error.status == midikraft::UploadHandshakeReply::Status::DEVICE_ERROR);
	CHECK(error.code == "write_failed");
	auto malformed = capability->isMessagePartOfUploadReply(sysex(0x13), sysex(0x01));
	CHECK(malformed.status == midikraft::UploadHandshakeReply::Status::ADAPTATION_ERROR);
}
