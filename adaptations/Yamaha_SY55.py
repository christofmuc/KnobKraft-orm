"""
# Yamaha SY55 / TG55 adaptation.

Adaptation created by github.com/hmmbug.

The SY55 and TG55 are nearly identical in terms of capabilities and MIDI spec.
There's no easy way to distinguish between them (1) so this adaptation is a
single combined one for both synths.

## NOTES

1. The only way I've found to distinguish these synths involved making use
   of a physical difference: the SY55 has a single stereo output, the TG55 has
   two stereo outputs. The MIDI spec has an "output select" parameter which
   could be used to identify the synths. Since that would require editing the
   loaded patch I've not implemented it as I don't think it's acceptable to
   modify patches without the user knowing.
"""

import logging
import sys
from enum import IntEnum
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


HELP_STRING = """SY55/TG55 Systems Settings:
- Set 'Program Change' mode to 'direct'
- Set 'Bulk Protect' to 'off'
- Set 'SysEx Device ID' appropriately. If in doubt, set it the same as the
  MIDI channel or 'omni'.

Only Voices are supported, no Multi, sequencer or other capabilities.
"""


class MemoryType(IntEnum):
    INTERNAL = 0x00
    PRESET = 0x02
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
        "name": "Preset",
        "size": 64,
        "type": "Voice",
        "isROM": True,
    },
]


class YamahaSY55(YamahaSYTGBase):
    def convertPatchesToBankDump(self, patches: List[List[int]]) -> List[int]:
        # SY55, TG55 patch -> bank conversion
        device_id = (self.detected_device_id if self.detected_device_id else 0) & 0x0F
        rtn = []
        for idx, patch in enumerate(patches):
            buf = patch.copy()
            buf[OFFSET_DEVICE_ID] = device_id
            buf[self.offset_memory_type] = MemoryType.INTERNAL
            buf[self.offset_memory_number] = idx
            buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
            rtn += buf
        return rtn

    def createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]:
        memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
        if memtype == MemoryType.INTERNAL:
            bank_pc = 119
        elif memtype == MemoryType.PRESET:
            bank_pc = 121
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

    short_memname = ["I", None, "P"]

    def friendlyProgramName(self, patchNo: int) -> str:
        # SY22/55/etc with 1x INTERNAL bank and 1x PRESET bank:
        #   I01 (internal, slot 1), P10 (preset, slot 10)
        # same format as displayed on synth screen
        memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
        memname = self.short_memname[memtype]
        bankslot = memnum + 1
        rtn = f"{memname}{bankslot:02d}"
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
        elif memory_type == self.memory_types["PRESET"]:
            rtn = memory_number + 64
        else:
            raise ValueError("Invalid memory type/number combination")
        return rtn

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        rtn = None
        if patchno <= 63:
            rtn = (int(self.memory_types["INTERNAL"]), patchno)
        elif patchno <= 127:
            rtn = (int(self.memory_types["PRESET"]), patchno - 64)
        else:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module):
        super().install(module)
        setattr(module, "convertPatchesToBankDump", self.convertPatchesToBankDump)
        setattr(module, "createCustomProgramChange", self.createCustomProgramChange)
        setattr(module, "friendlyProgramName", self.friendlyProgramName)
        setattr(module, "bankSlotToPatchNo", self.bankSlotToPatchNo)


synth = YamahaSY55(
    synth_name="Yamaha SY55/TG55",
    synth_id=0x7A,
    # voice
    first_preset_name="Piano     ",  # space-padded to 10 character
    msg_id_voice_dump="LM  8103VC",
    voice_default_name="INIT VOICE",
    voice_name_length=10,
    offset_voice_name=33,
    # banks
    memory_types=MemoryType,
    banks=BANKS,
    # data offsets in voice bulk dumps
    offset_memory_type=30,  # location of memory type (eg. INT, Preset)
    offset_memory_number=31,  # location of memory number (eg. patch num.)
    offset_req_memory_type=28,  # location of memory type in request
    offset_req_memory_number=29,  # location of memory num. in request
    # misc
    help_string=HELP_STRING,
)
synth.install(this_module)


def make_test_data():
    def programs(data: testing.TestData):
        return [
            testing.ProgramTestData(
                message=data.all_messages[0],
                number=0,
                name="PowerBass!",
                friendly_number="I01",
                rename_name="New Patch",
            ),
            testing.ProgramTestData(
                message=data.all_messages[2],
                number=2,
                name="VCF String",
                friendly_number="I03",
                rename_name="New Patch",
            ),
            testing.ProgramTestData(
                message=data.all_messages[5],
                number=5,
                name="Ocean*View",
                friendly_number="I06",
                rename_name="New Patch",
            ),
        ]

    def edit_buffers(data: testing.TestData):
        edit_buffer = synth.convertToEditBuffer(0, data.all_messages[16])
        return [
            testing.ProgramTestData(
                message=edit_buffer,
                name="ArticChoir",
                number=0,
                rename_name="0123456789",
            )
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    return testing.TestData(
        sysex="testData/Yamaha_SY55/sy55ptch.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,  # type: ignore
        program_dump_request=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x33, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END,
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4C, 0x4D, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x33, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, SYSEX_END,
        ],
        device_detect_reply=(
            [
                # Preset Voice 1
                SYSEX_START, YAMAHA_ID, 0x00, synth.synth_id, 0x01, 0x38, 0x4C, 0x4D,
                0x20, 0x20, 0x38, 0x31, 0x30, 0x33, 0x56, 0x43,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
                0x05, 0x50, 0x69, 0x61, 0x6E, 0x6F, 0x20, 0x20,
                0x20, 0x20, 0x20, 0x02, 0x64, 0x0C, 0x09, 0x14,
                0x02, 0x00, 0x01, 0x05, 0x0C, 0x00, 0x01, 0x00,
                0x0C, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0E, 0x00,
                0x00, 0x00, 0x7F, 0x00, 0x00, 0x7F, 0x00, 0x40,
                0x00, 0x7F, 0x01, 0x7F, 0x20, 0x0D, 0x00, 0x00,
                0x00, 0x00, 0x45, 0x40, 0x03, 0x00, 0x00, 0x00,
                0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x01, 0x03,
                0x01, 0x3D, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x01, 0x65, 0x01, 0x00, 0x00, 0x00, 0x00,
                0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
                0x40, 0x03, 0x24, 0x40, 0x51, 0x67, 0x01, 0x07,
                0x01, 0x03, 0x00, 0x7F, 0x00, 0x79, 0x00, 0x7F,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x40,
                0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x03, 0x00,
                0x2A, 0x54, 0x7F, 0x01, 0x00, 0x01, 0x00, 0x01,
                0x00, 0x01, 0x00, 0x00, 0x07, 0x03, 0x00, 0x3D,
                0x08, 0x08, 0x05, 0x19, 0x3A, 0x2D, 0x06, 0x00,
                0x2A, 0x54, 0x7F, 0x01, 0x01, 0x00, 0x00, 0x01,
                0x00, 0x01, 0x00, 0x04, 0x00, 0x03, 0x2A, 0xF7,
            ],
            0,
        ),
        expected_patch_count=64,
        friendly_bank_name=(0, "Internal"),
    )
