"""
# Yamaha SY85 adaptation.

Adaptation created by github.com/hmmbug.

- The SY85 has different settings for MIDI Channel and SysEx Device ID.
  (UTILITY → SYNTH SETUP → MENU → 2:MIDI 1)
- Requires synth to be in program change "direct" mode
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Requires "Bulk Protect" set to "off"
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Bank transmission is not supported as Knobcraft currently doesn't provide a
  means to select which of the four internal banks to load the patches.
  Defaulting to override bank 0 (Internal1) was not considered a good idea.


## Mapping SysEx Banks to Interface Banks

The SY85 maps voices (patches) to 4 banks:

- Internal1 (editable), 64 voice slots
- Internal2 (editable), 64 voice slots
- Internal3 (editable), 64 voice slots
- Internal4 (editable), 64 voice slots

On the front panels, each of these 64-slot banks are split into 4 banks (I1,
I2, I3, I4) of 16 voices, split into groups (A-H) of 8 voices. In MIDI
the groups/voices are zero-indexed instead (0-63 voices).

This adaptation largely ignores the group and represents each bank as
64 slots. The `friendlyProgramName(patchNo)` function references voices in the
same way as on the synth screen, eg "I1-A1" for Internal1, group A, voice 1.

## References:
- SY85 Manuals:
  https://usa.yamaha.com/products/contents/music_production/downloads/manuals/index.html?l=en&c=music_production&k=SY85
"""

import logging
import sys
from enum import IntEnum
from types import ModuleType
from typing import List

import testing
from yamaha.Yamaha_SY_TG_common import (
    OFFSET_DEVICE_ID,
    SYSEX_END,
    SYSEX_START,
    YAMAHA_ID,
    YamahaSYTGBase,
    bytes2str,
    str2bytes,
)

this_module = sys.modules[__name__]


HELP_STRING = """SY85 Systems Settings:
- The SY85 has different settings for MIDI Channel and SysEx Device ID.
  (UTILITY → SYNTH SETUP → MENU → 2:MIDI 1)
- Requires synth to be in program change "direct" mode
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Requires "Bulk Protect" set to "off"
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)

Limitations:
- Only Voices are supported, no Multi, sequencer or other capabilities.
- Bank transmission is not supported.
"""


class MemoryType(IntEnum):
    INTERNAL1 = 0x00
    INTERNAL2 = 0x03
    INTERNAL3 = 0x06
    INTERNAL4 = 0x09
    EDIT = 0x7F


BANKS = [
    {
        "bank": 0,
        "name": "Internal1",
        "size": 64,
        "type": "Voice",
        "isROM": False,
    },
    {
        "bank": 1,
        "name": "Internal2",
        "size": 64,
        "type": "Voice",
        "isROM": False,
    },
    {
        "bank": 2,
        "name": "Internal3",
        "size": 64,
        "type": "Voice",
        "isROM": False,
    },
    {
        "bank": 3,
        "name": "Internal4",
        "size": 64,
        "type": "Voice",
        "isROM": False,
    },
]


class YamahaSY85(YamahaSYTGBase):
    msg_id_drum_dump = "LM  0065DR"

    # ##### DEVICE DETECTION CAPABILITIES

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0F)
        # Request INTERNAL4, voice 0. No other synth should respond to this request
        msg = (
            str2bytes(self.msg_id_voice_dump)
            + [0x00] * 14
            + [int(self.memory_types["INTERNAL4"]), 0]
        )
        buf = self._makeMessage(device_id, msg, add_checksum=False)
        logging.debug(
            f"createDeviceDetectMessage({channel}: {bytes2str(buf[:32])} ... {bytes2str(buf[-2:])}"
        )
        return buf

    def channelIfValidDeviceResponse(self, buf: List[int]) -> int:
        if len(buf) < 32:
            return -1  # filter out irrelevant messages like active sensing
        if self._validateVoiceMessage(buf):
            rtn = buf[OFFSET_DEVICE_ID]
            self.detected_device_id = rtn
        else:
            rtn = -1
        return rtn

    # ##### PROGRAM DUMP CAPABILITY

    def createProgramDumpRequest(self, channel: int, patchNo: int) -> List[int]:
        buf = super().createProgramDumpRequest(channel, patchNo)
        # On SY85/TG500, the last voice of each bank is drum voice and uses
        # the identifier of DR instead of VC
        if (patchNo & 0b00111111) == 63:
            buf[15:17] = [0x44, 0x52]  # DR for Drum Voice
        return buf

    def createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]:
        bankchange = [0, 3, 6, 9][(patchNo & 0b111000000) >> 6]
        memnum = patchNo & 0b111111
        rtn = [
            0xB0 | (channel & 0x0F),
            0x00,
            bankchange,  # requires SY85 set to Program Change "direct" mode
            0xC0 | (channel & 0x0F),
            memnum,
        ]
        logging.debug(
            f"createCustomProgramChange({channel},{patchNo}) -> {bytes2str(rtn)}"
        )
        return rtn

    # ##### BANK CAPABILITIES

    short_banks = ["I1", "I2", "I3", "I4"]
    short_groups = "ABCDEFGH"

    def friendlyProgramName(self, patchNo: int) -> str:
        # SY85: I[1-4]-[A-H][1-8], e.g. I2-D4
        bank = self.short_banks[(patchNo >> 6) & 0b111]
        group = self.short_groups[(patchNo >> 3) & 0b111]
        prog = (patchNo & 0b111) + 1
        rtn = f"{bank}-{group}{prog}"
        logging.debug(f"friendlyProgramName: patchNo:{patchNo} -> rtn:{rtn}")
        return rtn

    def friendlyBankName(self, bank_number: int) -> str:
        BANK_NUMBER_TO_INDEX = {0: 0, 3: 1, 6: 2, 9: 3}
        idx = BANK_NUMBER_TO_INDEX[bank_number]
        rtn = BANKS[idx]["name"]
        logging.debug(f"friendlyBankName(bank_number) -> {rtn}")
        return rtn

    def bankSlotToPatchNo(self, memory_type: int, memory_number: int) -> int:
        self._memoryTypeNumberChecks(memory_type, memory_number)
        if memory_type == self.memory_types["EDIT"]:
            rtn = -1
        elif memory_type == self.memory_types["INTERNAL1"]:
            rtn = memory_number
        elif memory_type == self.memory_types["INTERNAL2"]:
            rtn = memory_number + 64
        elif memory_type == self.memory_types["INTERNAL3"]:
            rtn = memory_number + 128
        elif memory_type == self.memory_types["INTERNAL4"]:
            rtn = memory_number + 192
        else:
            raise ValueError("Invalid memory type/number combination")
        return rtn

    def _validateVoiceMessage(self, buf: List[int]) -> bool:
        rtn = self._validateMessage(buf) and (
            self._getMessageType(buf) == str2bytes(self.msg_id_voice_dump)
            or self._getMessageType(buf) == str2bytes(self.msg_id_drum_dump)
        )
        logging.debug(f"validate_voice_message: rtn:{rtn}")
        return rtn

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        if patchno <= 63:
            rtn = (int(self.memory_types["INTERNAL1"]), patchno)
        elif patchno <= 127:
            rtn = (int(self.memory_types["INTERNAL2"]), patchno - 64)
        elif patchno <= 191:
            rtn = (int(self.memory_types["INTERNAL3"]), patchno - 128)
        elif patchno <= 255:
            rtn = (int(self.memory_types["INTERNAL4"]), patchno - 192)
        else:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module: ModuleType):
        super().install(module)
        setattr(module, "createProgramDumpRequest", self.createProgramDumpRequest)
        setattr(module, "createCustomProgramChange", self.createCustomProgramChange)
        setattr(module, "friendlyProgramName", self.friendlyProgramName)
        setattr(module, "friendlyBankName", self.friendlyBankName)
        setattr(module, "bankSlotToPatchNo", self.bankSlotToPatchNo)


synth = YamahaSY85(
    synth_name="Yamaha SY85",
    synth_id=0x7A,
    # voice
    first_preset_name="",  # No presets on SY85
    msg_id_voice_dump="LM  0065VC",
    voice_default_name="Init Vce",
    voice_name_length=8,  # 8 characters in the voice name
    offset_voice_name=105,  # location of voice name
    # banks
    memory_types=MemoryType,
    banks=BANKS,
    # data offsets in voice bulk dumps
    offset_memory_type=30,  # location of memory type (eg. INT, Preset)
    offset_memory_number=31,  # location of memory number (eg. patch num.)
    offset_req_memory_type=28,  # location of memory type in request
    offset_req_memory_number=29,  # location of memory num. in request
    help_string=HELP_STRING,
)
synth.install(this_module)


def make_test_data():
    def programs(data: testing.TestData):
        return [
            testing.ProgramTestData(
                message=data.all_messages[0],
                number=192 + 0,
                name="SP Makro",
                friendly_number="I4-A1",
                rename_name="NewName1",
            ),
            testing.ProgramTestData(
                message=data.all_messages[6],
                number=192 + 6,
                name="SP Abyss",
                friendly_number="I4-A7",
                rename_name="NewName2",
            ),
            testing.ProgramTestData(
                message=data.all_messages[8],
                number=192 + 8,
                name="AP Grand",
                friendly_number="I4-B1",
                rename_name="NewName3",
            ),
        ]

    def edit_buffers(data: testing.TestData):
        edit_buffer = synth.convertToEditBuffer(0, data.all_messages[16])
        return [
            testing.ProgramTestData(
                message=edit_buffer, name="BA Wood ", number=0, rename_name="01234567"
            )
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    return testing.TestData(
        sysex="testData/Yamaha_SY85/SY85_test_bank.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,  # type: ignore
        program_dump_request=[
            # Internal 1, Group A, Voice 1 dump request
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x30, 0x30, 0x36, 0x35, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END,
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x30, 0x30, 0x36, 0x35, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x09, 0x00, SYSEX_END,
        ],
        device_detect_reply=([
            # Internal 4 Group A Voice 1 dump
            0xF0, 0x43, 0x00, 0x7A, 0x01, 0x68, 0x4C, 0x4D,
            0x20, 0x20, 0x30, 0x30, 0x36, 0x35, 0x56, 0x43,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
            0x00, 0x01, 0x19, 0x01, 0x15, 0x00, 0x08, 0x00,
            0x00, 0x64, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00,
            0x00, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34,
            0x00, 0x0C, 0x64, 0x64, 0x00, 0x00, 0x00, 0x1B,
            0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0A,
            0x00, 0x00, 0x00, 0x04, 0x00, 0x37, 0x00, 0x07,
            0x00, 0x06, 0x00, 0x14, 0x64, 0x64, 0x32, 0x32,
            0x32, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x53, 0x50, 0x20, 0x4D, 0x61, 0x6B, 0x72,
            0x6F, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x40,
            0x28, 0x64, 0x1B, 0x00, 0x32, 0x7F, 0x7F, 0x00,
            0x01, 0x00, 0x00, 0x40, 0x40, 0x00, 0x19, 0x3F,
            0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x20,
            0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x3F,
            0x3F, 0x00, 0x18, 0x3F, 0x3F, 0x24, 0x37, 0x4C,
            0x60, 0x01, 0x00, 0x01, 0x00, 0x00, 0x78, 0x00,
            0x78, 0x00, 0x22, 0x00, 0x09, 0x00, 0x14, 0x00,
            0x00, 0x00, 0x10, 0x3E, 0x1D, 0x0F, 0x0E, 0x0F,
            0x00, 0x00, 0x6D, 0x61, 0x53, 0x49, 0x40, 0x40,
            0x40, 0x00, 0x24, 0x37, 0x4C, 0x60, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x27, 0xF7,
        ], 0, ),
        expected_patch_count=256,
        friendly_bank_name=(9, "Internal4"),
    )
