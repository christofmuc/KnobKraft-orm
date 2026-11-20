#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


import hashlib


SYSEX_HEADER = [0xf0, 0x42, 0x30, 0x04]
DATA_DUMP_SIZE = 32
PROGRAM_COUNT = 64


def name():
    return "Korg DW-6000"


def createDeviceDetectMessage(channel):
    return [0xf0, 0x42, 0x40, 0xf7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if message == SYSEX_HEADER + [0xf7]:
        # The reply does not identify the receive channel. Keep the existing fallback.
        return 1
    return -1


def createEditBufferRequest(channel):
    return SYSEX_HEADER + [0x10, 0xf7]


def isEditBufferDump(message):
    # Older KnobKraft exports may carry a MIDI channel in the format byte.
    return (
        len(message) == DATA_DUMP_SIZE
        and message[:2] == SYSEX_HEADER[:2]
        and (message[2] & 0xf0) == 0x30
        and message[3:5] == [0x04, 0x40]
        and all(0 <= byte < 0x80 for byte in message[5:-1])
        and message[-1] == 0xf7
    )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return PROGRAM_COUNT


def friendlyProgramName(program):
    return f"{program // 8 + 1}{program % 8 + 1}"


def nameFromDump(message):
    # The DW-6000 has no patch name memory.
    return "DW-6000"


def convertToEditBuffer(channel, message):
    if not isSingleProgramDump(message):
        raise ValueError("Expected a complete DW-6000 patch dump")
    # The MIDI implementation specifies a fixed 0x30 format ID for SysEx.
    return SYSEX_HEADER + message[4:DATA_DUMP_SIZE]


def createProgramDumpRequest(channel, program_number):
    return [0xc0 | channel, program_number] + createEditBufferRequest(channel)


def createStoreToProgramMessage(channel, program_number):
    if not 0 <= program_number < PROGRAM_COUNT:
        raise ValueError("DW-6000 program number must be between 0 and 63")
    return SYSEX_HEADER + [0x11, program_number, 0xf7]


def isSingleProgramDump(message):
    # A requested program arrives as a sound dump. The write request is added on export.
    if len(message) == DATA_DUMP_SIZE:
        return isEditBufferDump(message)
    return isEditBufferDump(message[:DATA_DUMP_SIZE]) and expectsUploadReply(message[DATA_DUMP_SIZE:])


def numberFromDump(message):
    # Only an export with an appended write request carries a slot number.
    if len(message) > DATA_DUMP_SIZE and isSingleProgramDump(message):
        return message[-2]
    return -1


def calculateFingerprint(message):
    # Match the host's original MD5 of a sound dump, without an optional write request.
    return hashlib.md5(bytes(message[:DATA_DUMP_SIZE])).hexdigest()


def convertToProgramDump(channel, message, program_number):
    return convertToEditBuffer(channel, message) + createStoreToProgramMessage(channel, program_number)


def expectsUploadReply(sent_message):
    return (
        len(sent_message) == 7
        and sent_message[:5] == SYSEX_HEADER + [0x11]
        and 0 <= sent_message[5] < PROGRAM_COUNT
        and sent_message[6] == 0xf7
    )


def isPartOfUploadReply(message, sent_message):
    if not expectsUploadReply(sent_message):
        return None
    if message == SYSEX_HEADER + [0x21, 0xf7]:
        return {"status": "accepted"}
    if message == SYSEX_HEADER + [0x22, 0xf7]:
        program = friendlyProgramName(sent_message[5])
        return {
            "status": "error",
            "code": "write_error",
            "message": f"DW-6000 rejected the write to program {program}. Check the rear WRITE switch.",
        }
    return None


def setupHelp():
    return (
        "Set parameter 82 to 2 (ALL) and select the DW-6000 MIDI input and output in KnobKraft. "
        "Use the bank's Send to synth button to store patches in their assigned slots. "
        "Set the rear WRITE switch to ENABLE for program writes."
    )


def make_test_data():
    import testing

    patch = SYSEX_HEADER + [0x40] + [0] * 26 + [0xf7]
    write = SYSEX_HEADER + [0x11, 63, 0xf7]
    return testing.TestData(
        edit_buffer_generator=lambda _: [testing.ProgramTestData(message=patch, name="DW-6000", target_no=63)],
        program_generator=lambda _: [testing.ProgramTestData(message=patch + write, name="DW-6000", target_no=0, number=63, friendly_number="88")],
        device_detect_call="F0 42 40 F7",
        device_detect_reply=("F0 42 30 04 F7", 1),
        program_dump_request=(2, 63, [0xc2, 63, 0xf0, 0x42, 0x30, 0x04, 0x10, 0xf7]),
        upload_reply_cases=[
            testing.UploadReplyTestData(write, "F0 42 30 04 21 F7", {"status": "accepted"}),
            testing.UploadReplyTestData(write, "F0 42 30 04 22 F7", {
                "status": "error",
                "code": "write_error",
                "message": "DW-6000 rejected the write to program 88. Check the rear WRITE switch.",
            }),
            testing.UploadReplyTestData(patch, "F0 42 30 04 21 F7", None, expects_reply=False),
        ],
    )
