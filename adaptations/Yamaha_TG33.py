#
#   Yamaha TG33 adaptation, contributed to KnobKraft Orm under its dual licensing
#   (Affero GPL by default, an MIT license is available for purchase from Christof Ruch).
#
#   Voices, through the edit buffer. The SysEx follows Sean Luke's Edisyn TG33/SY22/SY35 editor
#   (https://github.com/eclab/edisyn, edisyn/synth/yamahatg33, Apache 2.0):
#   - the current voice is requested with F0 43 2n 7E "LM  0012VE" F7, n the device number;
#   - it comes back as one message, F0 43 0n 7E <count MSB> <count LSB> "LM  0012VE" <587 bytes> <checksum> F7,
#     605 bytes in all; the voice name is 8 ASCII characters at byte 28;
#   - the checksum covers "LM  0012VE" and the voice data;
#   - sent back in the same form, it replaces the voice being edited.
#   The TG33 has no request for a stored voice: Edisyn selects it with a program change first.
#   The SY22 and SY35 use another format ("PK  2203AE", 592 bytes) and are not covered here.
#
from typing import List

import testing

_header = [0x4C, 0x4D, 0x20, 0x20, 0x30, 0x30, 0x31, 0x32, 0x56, 0x45]  # "LM  0012VE"
_voice_length = 605
_name_start = 28
_name_length = 8


def name():
    return "Yamaha TG33"


def createDeviceDetectMessage(channel):
    return createEditBufferRequest(channel)


def needsChannelSpecificDetection():
    return True


def deviceDetectWaitMilliseconds():
    return 300


def channelIfValidDeviceResponse(message):
    if isEditBufferDump(message):
        return message[2] & 0x0F
    return -1


def createEditBufferRequest(channel):
    return [0xF0, 0x43, 0x20 | (channel & 0x0F), 0x7E] + _header + [0xF7]


def isEditBufferDump(message):
    return (len(message) == _voice_length
            and message[0] == 0xF0
            and message[1] == 0x43
            and message[2] & 0xF0 == 0x00
            and message[3] == 0x7E
            and message[6:16] == _header
            and message[-1] == 0xF7)


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message[0:2] + [channel & 0x0F] + message[3:]
    raise Exception("This is not a Yamaha TG33 voice")


def nameFromDump(message):
    if isEditBufferDump(message):
        return ''.join(chr(x) if 32 <= x < 127 else ' ' for x in message[_name_start:_name_start + _name_length])
    return "Invalid"


def renamePatch(message, new_name):
    if isEditBufferDump(message):
        name = [ord(c) if 32 <= ord(c) < 127 else 0x20 for c in new_name[:_name_length].ljust(_name_length)]
        result = message[:_name_start] + name + message[_name_start + _name_length:]
        result[-2] = checksum(result[6:-2])
        return result
    raise Exception("Can only rename Yamaha TG33 voices")


def checksum(data):
    return -sum(data) & 0x7F


def make_test_data():
    def edit_buffers(data: testing.TestData) -> List[testing.ProgramTestData]:
        # Edisyn's initial TG33 voice (edisyn/synth/yamahatg33/YamahaTG33.init, Apache 2.0)
        voice = data.all_messages[0]
        assert checksum(voice[6:-2]) == voice[-2]
        assert createEditBufferRequest(2) == [0xF0, 0x43, 0x22, 0x7E] + _header + [0xF7]
        assert channelIfValidDeviceResponse(convertToEditBuffer(5, voice)) == 5
        yield testing.ProgramTestData(message=voice, name="Untitled", rename_name="Vector 1")

    return testing.TestData(sysex=R"testData/Yamaha_TG33/edisyn-init-voice.syx", edit_buffer_generator=edit_buffers)
