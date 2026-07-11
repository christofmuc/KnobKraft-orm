"""
# Yamaha SY77 adaptation.

Adaptation created by github.com/hmmbug.

- Requires synth to be in program change "direct" mode (Utility -> MIDI ->
  Program Change = direct)
- Requires "Bulk Protect" set to "off" (Utility -> MIDI -> Channel Set -> Bulk
  Protect = Off)
- The SY77/TG77 has different settings for MIDI Channel and SysEx Device ID.

## Testing Notes:
- Tested on an SY77 (ROM version 1.5).

## Mapping SysEx Banks to Interface Banks
The SY77/TG77 maps voices (patches) to 3 main banks:

- Internal (editable), 64 voice slots
- Preset 1 (read-only), 64 voice slots
- Preset 2 (read-only), 64 voice slots

On the front panels, each of these 64-slot banks are split into 4 banks (A-D)
of 16 voices, so banks A-D represent voices 1-16, 17-32, 33-48, 49-64. In MIDI
these numbers are zero-indexed instead (0-63 voices).

This adaptation largely ignores the 16 slot A-D banks and represents banks as
64 slots. The `friendlyProgramName(patchNo)` function references voices in the
same way as on the synth screen, eg "IN-A01" for Internal, Bank A, voice 1.

## References:
- SY77 & TG77 MIDI Data Format Manuals:
    https://usa.yamaha.com/files/download/other_assets/1/317121/SY77E2.PDF
    https://usa.yamaha.com/files/download/other_assets/0/316980/TG77E2.PDF
"""

import logging
import sys
from enum import IntEnum
from types import ModuleType
from typing import List

import testing
from yamaha.Yamaha_SY_TG_common import (
    OFFSET_CHECKSUM,
    OFFSET_DEVICE_ID,
    SYSEX_END,
    SYSEX_START,
    YAMAHA_ID,
    YamahaSYTGBase,
    bytes2str,
)

this_module = sys.modules[__name__]


HELP_STRING = """SY77/TG77 Systems Settings:
- Set 'Program Change' mode to 'direct' (Utility -> MIDI -> Program Change)
- Set 'Bulk Protect' to 'off' (Utility -> MIDI -> Channel Set -> Bulk Protect)
- Set 'SysEx Device ID' appropriately. If in doubt, set it the same as the
  MIDI channel or 'omni'. (Utility -> MIDI -> Channel Set -> Device ID)

Only Voices are supported, no Multi, sequencer or other capabilities.
"""


class MemoryType(IntEnum):
    INTERNAL = 0x00
    PRESET1 = 0x02
    PRESET2 = 0x03
    EDIT = 0x7F


BANKS = [
    {
        "bank": 0,
        "name": "Internal",
        "size": 64,
        "type": "Voice",
        "isROM": False,
    },
    {
        "bank": 1,
        "name": "Preset 1",
        "size": 64,
        "type": "Voice",
        "isROM": True,
    },
    {
        "bank": 2,
        "name": "Preset 2",
        "size": 64,
        "type": "Voice",
        "isROM": True,
    },
]


class YamahaSY77(YamahaSYTGBase):
    def convertPatchesToBankDump(self, patches: List[List[int]]) -> List[int]:
        # SY77, TG77 patch -> bank conversion
        device_id = (self.detected_device_id if self.detected_device_id else 0) & 0x0F
        bank = []
        for idx, patch in enumerate(patches):
            buf = patch.copy()
            buf[OFFSET_DEVICE_ID] = device_id
            buf[self.offset_memory_type] = MemoryType.INTERNAL
            buf[self.offset_memory_number] = idx
            buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
            bank += buf
        return bank

    def createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]:
        memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
        if memtype == MemoryType.INTERNAL:
            bank_pc = 122
        elif memtype == MemoryType.PRESET1:
            bank_pc = 121
        elif memtype == MemoryType.PRESET2:
            bank_pc = 117
        else:
            raise ValueError(f"Invalid memory type ({memtype}).")
        rtn = [
            0xC0 | (channel & 0x0F),
            bank_pc,  # 'direct mode' program change for bank selection (manual ref 2.1.3)
            0xC0 | (channel & 0x0F),
            memnum,  # select the voice in the bank, based on patchNo MOD 64
        ]
        logging.debug(
            f"createCustomProgramChange({channel},{patchNo}) -> {bytes2str(rtn)}"
        )
        return rtn

    short_memnames = ["IN", None, "P1", "P2"]
    short_banknames = "ABCD"

    def friendlyProgramName(self, patchNo: int) -> str:
        # SY77: IN-A01 (internal, bank A, slot 1), P1-B03 (Preset 1, bank B, slot 3)
        # same format as displayed on synth screen
        memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
        memname = self.short_memnames[memtype]
        bankchar = self.short_banknames[memnum // 16]
        bankslot = memnum % 16 + 1
        rtn = f"{memname}-{bankchar}{bankslot:02d}"
        logging.debug(
            f"friendlyProgramName: MemType/Num: {memtype}/{memnum} -> rtn:{rtn}"
        )
        return rtn

    def bankSlotToPatchNo(self, memory_type: int, memory_number: int) -> int:
        self._memoryTypeNumberChecks(memory_type, memory_number)
        if memory_type == self.memory_types["EDIT"]:
            rtn = -1
        elif memory_type == self.memory_types["INTERNAL"]:
            rtn = memory_number
        elif memory_type == self.memory_types["PRESET1"]:
            rtn = memory_number + 64
        elif memory_type == self.memory_types["PRESET2"]:
            rtn = memory_number + 128
        else:
            raise ValueError("Invalid memory type/number combination")
        return rtn

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        rtn = None
        if 0 <= patchno <= 63:
            rtn = (int(self.memory_types["INTERNAL"]), patchno)
        elif patchno <= 127:
            rtn = (int(self.memory_types["PRESET1"]), patchno - 64)
        elif patchno <= 191:
            rtn = (int(self.memory_types["PRESET2"]), patchno - 128)
        else:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module: ModuleType):
        super().install(module)
        setattr(module, "convertPatchesToBankDump", self.convertPatchesToBankDump)
        setattr(module, "createCustomProgramChange", self.createCustomProgramChange)
        setattr(module, "friendlyProgramName", self.friendlyProgramName)
        setattr(module, "bankSlotToPatchNo", self.bankSlotToPatchNo)


synth = YamahaSY77(
    synth_name="Yamaha SY77",
    synth_id=0x7A,
    # voice
    first_preset_name="GrandPiano",
    msg_id_voice_dump="LM  8101VC",
    voice_default_name="INIT VOICE",
    voice_name_length=10,  # 10 characters in the voice name
    offset_voice_name=33,  # location of voice name
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
                number=0,
                name="GrandPiano",
                friendly_number="IN-A01",
                rename_name="New Patch",
            ),
            testing.ProgramTestData(
                message=data.all_messages[6],
                number=6,
                name="Triton    ",
                friendly_number="IN-A07",
                rename_name="New Patch",
            ),
            testing.ProgramTestData(
                message=data.all_messages[7],
                number=7,
                name="FrenchHorn",
                friendly_number="IN-A08",
                rename_name="New Patch",
            ),
        ]

    def edit_buffers(data: testing.TestData):
        edit_buffer = synth.convertToEditBuffer(0, data.all_messages[16])
        return [
            testing.ProgramTestData(
                message=edit_buffer,
                name="Dyna Grand",
                number=0,
                rename_name="0123456789",
            )
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    return testing.TestData(
        sysex="testData/Yamaha_SY77/bank.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,  # type: ignore
        program_dump_request=[
            # Preset 1, Voice 1 dump request
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x31, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END,
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x31, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, SYSEX_END,
        ],
        device_detect_reply=([
            # Preset 1 Voice 1 dump
            SYSEX_START, YAMAHA_ID, 0x00, synth.synth_id, 0x04, 0x43, 0x4C, 0x4D,
            0x20, 0x20, 0x38, 0x31, 0x30, 0x31, 0x56, 0x43,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00,
            0x08, 0x47, 0x72, 0x61, 0x6E, 0x64, 0x50, 0x69,
            0x61, 0x6E, 0x6F, 0x01, 0x00, 0x38, 0x64, 0x03,
            0x10, 0x01, 0x01, 0x00, 0x64, 0x64, 0x08, 0x10,
            0x10, 0x08, 0x06, 0x64, 0x17, 0x12, 0x05, 0x0A,
            0x10, 0x64, 0x1F, 0x0D, 0x05, 0x0B, 0x01, 0x01,
            0x02, 0x00, 0x01, 0x15, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x7F,
            0x00, 0x00, 0x7F, 0x00, 0x40, 0x00, 0x7F, 0x01,
            0x7F, 0x20, 0x06, 0x7F, 0x00, 0x40, 0x00, 0x7F,
            0x01, 0x7F, 0x20, 0x06, 0x3F, 0x18, 0x3F, 0x11,
            0x22, 0x3F, 0x3F, 0x3A, 0x3A, 0x00, 0x00, 0x00,
            0x03, 0x3F, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
            0x00, 0x05, 0x3F, 0x00, 0x00, 0x0E, 0x01, 0x00,
            0x00, 0x43, 0x00, 0x2A, 0x54, 0x7F, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x03,
            0x00, 0x3E, 0x1A, 0x3F, 0x0F, 0x1B, 0x3F, 0x3F,
            0x3A, 0x3A, 0x00, 0x00, 0x00, 0x03, 0x3F, 0x00,
            0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x12, 0x3F,
            0x00, 0x0C, 0x0E, 0x01, 0x00, 0x00, 0x38, 0x00,
            0x2A, 0x54, 0x7F, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x3D, 0x18,
            0x17, 0x11, 0x1D, 0x3F, 0x3F, 0x39, 0x37, 0x00,
            0x00, 0x00, 0x03, 0x3F, 0x00, 0x01, 0x00, 0x06,
            0x00, 0x00, 0x01, 0x04, 0x3F, 0x01, 0x00, 0x0E,
            0x01, 0x00, 0x00, 0x70, 0x00, 0x53, 0x5B, 0x60,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x7B, 0x00, 0x7B,
            0x01, 0x01, 0x00, 0x3F, 0x1F, 0x3F, 0x12, 0x22,
            0x3F, 0x3F, 0x38, 0x38, 0x00, 0x00, 0x00, 0x03,
            0x3F, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x13, 0x3F, 0x00, 0x02, 0x0E, 0x01, 0x12, 0x11,
            0x56, 0x4B, 0x4C, 0x4D, 0x58, 0x01, 0x00, 0x00,
            0x79, 0x00, 0x7A, 0x00, 0x7C, 0x00, 0x06, 0x00,
            0x3E, 0x12, 0x3F, 0x0F, 0x1B, 0x3F, 0x3F, 0x3A,
            0x3A, 0x00, 0x00, 0x00, 0x03, 0x3F, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x00, 0x01, 0x10, 0x3F, 0x00,
            0x0A, 0x0E, 0x01, 0x15, 0x12, 0x4E, 0x00, 0x2A,
            0x57, 0x58, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x03, 0x00, 0x01, 0x00, 0x2A, 0x17, 0x13,
            0x11, 0x21, 0x3F, 0x3F, 0x39, 0x37, 0x00, 0x00,
            0x00, 0x03, 0x34, 0x00, 0x02, 0x00, 0x07, 0x00,
            0x00, 0x01, 0x14, 0x3F, 0x01, 0x0A, 0x0E, 0x01,
            0x30, 0x00, 0x6F, 0x29, 0x4A, 0x5A, 0x5B, 0x00,
            0x01, 0x00, 0x71, 0x00, 0x6B, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x21, 0x3F, 0x3F, 0x3F, 0x3F, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x41,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x50, 0x00, 0x00, 0x00, 0x02, 0x28, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x2A, 0x54,
            0x7F, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x7F, 0x00, 0x3F, 0x04, 0x00, 0x00,
            0x00, 0x00, 0x40, 0x40, 0x22, 0x22, 0x22, 0x22,
            0x22, 0x04, 0x00, 0x4B, 0x4C, 0x4D, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x40, 0x03,
            0x3E, 0x3F, 0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x01, 0x00, 0x00, 0x36, 0x13, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x16,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x01, 0x00, 0x48, 0x54,
            0x7F, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x64, 0x00, 0x3F, 0x03, 0x00, 0x00,
            0x00, 0x00, 0x40, 0x40, 0x22, 0x22, 0x22, 0x22,
            0x22, 0x07, 0x00, 0x48, 0x54, 0x7F, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x06,
            0x00, 0x00, 0x3F, 0x0C, 0x0D, 0x0C, 0x1D, 0x3F,
            0x3A, 0x02, 0x4B, 0x4C, 0x50, 0x51, 0x01, 0x00,
            0x01, 0x09, 0x01, 0x08, 0x00, 0x7F, 0x06, 0x00,
            0x00, 0x08, SYSEX_END,
        ], 0, ),
        expected_patch_count=64,
        friendly_bank_name=(0, "Internal"),
    )
