import pytest

import KawaiK5000 as k5000


def program_dump(channel=0):
    return [0xF0, k5000.KawaiSysexID, channel, k5000.OneBlockDump,
            0x00, 0x0A, 0x00, 0x00, 0x00, 0xF7]


def upload_reply(code, channel=0):
    return [0xF0, k5000.KawaiSysexID, channel, code, 0x00, 0x0A, 0xF7]


def test_only_program_dumps_expect_upload_replies():
    assert k5000.expectsUploadReply(program_dump())
    assert not k5000.expectsUploadReply([0xC0, 0x7F])


def test_write_complete_accepts_the_current_upload():
    assert k5000.isPartOfUploadReply(
        upload_reply(k5000.WriteComplete), program_dump()) == {"status": "accepted"}


@pytest.mark.parametrize(
    "reply_code,error_code",
    [
        (k5000.WriteError, "write_error"),
        (k5000.WriteErrorByProtect, "write_protected"),
        (k5000.WriteErrorByMemoryFull, "memory_full"),
        (k5000.WriteErrorByNoExpandMemory, "expansion_missing"),
    ],
)
def test_write_errors_are_terminal(reply_code, error_code):
    result = k5000.isPartOfUploadReply(upload_reply(reply_code), program_dump())
    assert result["status"] == "error"
    assert result["code"] == error_code
    assert result["message"]


@pytest.mark.parametrize(
    "reply,sent",
    [
        (upload_reply(k5000.WriteComplete, channel=1), program_dump(channel=0)),
        ([0xF0, 0x41, 0x00, k5000.WriteComplete, 0x00, 0x0A, 0xF7], program_dump()),
        (upload_reply(0x43), program_dump()),
        (upload_reply(k5000.WriteComplete), [0xC0, 0x7F]),
    ],
)
def test_unrelated_messages_are_ignored(reply, sent):
    assert k5000.isPartOfUploadReply(reply, sent) is None


def test_upload_reply_timeout_is_configured_separately():
    assert k5000.messageTimings()["uploadReplyTimeoutMs"] == 5000
