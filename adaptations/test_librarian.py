from threading import Event

from testing.librarian import Librarian, UploadStatus
from testing.mock_midi import MockMidiController, ScriptedMockDevice


def program_dump(slot: int, sound: int):
    return [0xF0, 0x01, slot, sound, 0xF7]


class ProgramStreamBankAdaptation:
    @staticmethod
    def createProgramDumpRequest(channel, patch_no):
        return []

    @staticmethod
    def isSingleProgramDump(message):
        return len(message) == 5 and message[1] == 0x01

    @staticmethod
    def isPartOfBankDump(message):
        return ProgramStreamBankAdaptation.isSingleProgramDump(message)

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 3

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return messages

    @staticmethod
    def calculateFingerprint(message):
        return str(message[3])


def test_load_sysex_deduplicates_bank_stream_of_program_dumps():
    messages = [program_dump(0, 10), program_dump(1, 11), program_dump(2, 12)]

    patches = Librarian().load_sysex(ProgramStreamBankAdaptation, messages)

    assert patches == messages


class PartiallyOverlappingBankAdaptation(ProgramStreamBankAdaptation):
    @staticmethod
    def isPartOfBankDump(message):
        return True

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 2

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return [program_dump(0, 10), program_dump(1, 10)]


def test_load_sysex_deduplicates_bank_patches_by_occurrence():
    messages = [program_dump(0, 10), [0xF0, 0x02, 0xF7]]

    patches = Librarian().load_sysex(PartiallyOverlappingBankAdaptation, messages)

    assert patches == [program_dump(0, 10), program_dump(1, 10)]


class MutatingFingerprintProgramAdaptation:
    @staticmethod
    def createProgramDumpRequest(channel, patch_no):
        return []

    @staticmethod
    def isSingleProgramDump(message):
        return len(message) == 5 and message[1] == 0x01

    @staticmethod
    def calculateFingerprint(message):
        message[2] = 99
        return str(message)


def test_load_sysex_preserves_program_patch_when_fingerprint_mutates_input():
    patches = Librarian().load_sysex(MutatingFingerprintProgramAdaptation, [program_dump(0, 10)])

    assert patches == [program_dump(0, 10)]


class MutatingFingerprintEditBufferAdaptation:
    @staticmethod
    def createEditBufferRequest(channel):
        return []

    @staticmethod
    def isEditBufferDump(message):
        return len(message) == 5 and message[1] == 0x02

    @staticmethod
    def calculateFingerprint(message):
        message[2] = 99
        return str(message)


def test_load_sysex_preserves_edit_buffer_when_fingerprint_mutates_input():
    edit_buffer = [0xF0, 0x02, 0, 10, 0xF7]
    expected_edit_buffer = edit_buffer.copy()

    patches = Librarian().load_sysex(MutatingFingerprintEditBufferAdaptation, [edit_buffer])

    assert patches == [expected_edit_buffer]
    assert edit_buffer == expected_edit_buffer


class MutatingFingerprintBankAdaptation:
    @staticmethod
    def isPartOfBankDump(message):
        return True

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 1

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return [program_dump(0, 10)]

    @staticmethod
    def calculateFingerprint(message):
        message[2] = 99
        return str(message)


def test_load_sysex_preserves_bank_patch_when_fingerprint_mutates_input():
    patches = Librarian().load_sysex(MutatingFingerprintBankAdaptation, [[0xF0, 0x02, 0xF7]])

    assert patches == [program_dump(0, 10)]


class UploadHandshakeAdaptation:
    @staticmethod
    def expectsUploadReply(sent_message):
        return sent_message[0] == 0xF0

    @staticmethod
    def isPartOfUploadReply(message, sent_message):
        if sent_message != [0xF0, 0x01, 0xF7]:
            return None
        if message == [0xF0, 0x10, 0xF7]:
            return {"status": "continue", "messages": [0xF0, 0x55, 0xF7]}
        if message == [0xF0, 0x11, 0xF7]:
            return {"status": "accepted"}
        return None

    @staticmethod
    def messageTimings():
        return {"uploadReplyTimeoutMs": 1234}


class ErrorWithMessagesUploadAdaptation(UploadHandshakeAdaptation):
    @staticmethod
    def isPartOfUploadReply(message, sent_message):
        return {
            "status": "error",
            "code": "write_failed",
            "message": "Write failed",
            "messages": [0xF0, 0x55, 0xF7],
        }


class ShortTimeoutUploadAdaptation(UploadHandshakeAdaptation):
    @staticmethod
    def messageTimings():
        return {"uploadReplyTimeoutMs": 10}


class RaisingUploadHandshakeAdaptation(UploadHandshakeAdaptation):
    @staticmethod
    def expectsUploadReply(sent_message):
        raise RuntimeError("broken upload predicate")


def test_upload_handshake_waits_orders_responses_and_skips_unacknowledged_messages():
    upload = [0xF0, 0x01, 0xF7]
    unrelated = [0xF0, 0x09, 0xF7]
    progress = [0xF0, 0x10, 0xF7]
    accepted = [0xF0, 0x11, 0xF7]
    response = [0xF0, 0x55, 0xF7]
    program_change = [0xC0, 0x07]
    device = ScriptedMockDevice(
        {tuple(upload): [unrelated, progress, accepted]},
        ignore_unmatched=True,
    )
    controller = MockMidiController(device)
    librarian = Librarian()
    results = []

    librarian.send_block_of_messages_to_synth(
        controller,
        UploadHandshakeAdaptation,
        [upload, program_change],
        lambda result: results.append(result),
    )

    assert controller.sent_messages == [upload]
    controller.drain()

    assert controller.sent_messages == [upload, response, program_change]
    assert len(results) == 1
    assert results[0].status == UploadStatus.ACKNOWLEDGED
    assert results[0].completed_messages == 2
    assert controller.handlers == []
    assert Librarian.upload_reply_timeout(UploadHandshakeAdaptation) == 1234


def test_upload_handshake_timeout_is_terminal_and_uncertain():
    upload = [0xF0, 0x01, 0xF7]
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))
    librarian = Librarian()
    results = []

    librarian.send_block_of_messages_to_synth(
        controller,
        UploadHandshakeAdaptation,
        [upload],
        lambda result: results.append(result),
    )
    librarian.timeout_upload()

    assert controller.sent_messages == [upload]
    assert len(results) == 1
    assert results[0].status == UploadStatus.TIMEOUT
    assert results[0].outcome_uncertain
    assert controller.handlers == []


def test_upload_handshake_rejects_error_replies_with_response_messages():
    upload = [0xF0, 0x01, 0xF7]
    reply = [0xF0, 0x12, 0xF7]
    controller = MockMidiController(ScriptedMockDevice({tuple(upload): [reply]}))
    librarian = Librarian()
    results = []

    librarian.send_block_of_messages_to_synth(
        controller,
        ErrorWithMessagesUploadAdaptation,
        [upload],
        lambda result: results.append(result),
    )
    controller.drain()

    assert controller.sent_messages == [upload]
    assert len(results) == 1
    assert results[0].status == UploadStatus.ADAPTATION_ERROR
    assert results[0].code == "invalid_upload_reply"
    assert results[0].outcome_uncertain


def test_upload_handshake_timeout_is_scheduled_and_releases_the_librarian():
    upload = [0xF0, 0x01, 0xF7]
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))
    librarian = Librarian()
    results = []
    finished = Event()

    def upload_finished(result):
        results.append(result)
        finished.set()

    librarian.send_block_of_messages_to_synth(
        controller,
        ShortTimeoutUploadAdaptation,
        [upload],
        upload_finished,
    )

    assert finished.wait(1)
    assert len(results) == 1
    assert results[0].status == UploadStatus.TIMEOUT
    assert results[0].outcome_uncertain
    assert controller.handlers == []

    retry_controller = MockMidiController(ScriptedMockDevice({
        tuple(upload): [[0xF0, 0x11, 0xF7]],
    }))
    retry_results = []
    librarian.send_block_of_messages_to_synth(
        retry_controller,
        UploadHandshakeAdaptation,
        [upload],
        lambda result: retry_results.append(result),
    )
    retry_controller.drain()
    assert retry_results[0].status == UploadStatus.ACKNOWLEDGED


def test_upload_handshake_predicate_exception_finishes_and_releases_the_librarian():
    upload = [0xF0, 0x01, 0xF7]
    controller = MockMidiController(ScriptedMockDevice({}, ignore_unmatched=True))
    librarian = Librarian()
    results = []

    librarian.send_block_of_messages_to_synth(
        controller,
        RaisingUploadHandshakeAdaptation,
        [upload],
        lambda result: results.append(result),
    )

    assert controller.sent_messages == []
    assert len(results) == 1
    assert results[0].status == UploadStatus.ADAPTATION_ERROR
    assert results[0].code == "invalid_upload_handshake"
    assert results[0].message == "broken upload predicate"
    assert controller.handlers == []
    assert librarian._active_upload is None
