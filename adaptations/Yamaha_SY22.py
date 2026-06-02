"""
# Yamaha SY22 adaptations.

Adaptation created by github.com/hmmbug.

## 1 Voice SysEx Format

    01      0xf0 Start SysEx
    02      0x43 Yamaha ID
    03      0x00 Device ID (zero indexed 0-15 -> 1-16)
    04      0x7e Synth ID
    05      0xnn Byte Count MSB
    06      0xnn Byte Count LSB
    07-16   ---- "PK  2203AE"
    17-19   0x?? Unknown
    20-27   Voice name
    ...

"""
from enum import IntEnum
import logging
import sys
from typing import List

import testing
import knobkraft.sysex

logging.basicConfig(level=logging.DEBUG)

from yamaha.Yamaha_SY_TG_common import (
    YamahaSYTGBase,
    bytes2str,
    SYSEX_START,
    YAMAHA_ID,
    OFFSET_DEVICE_ID,
    OFFSET_CHECKSUM,
    SYSEX_END,
    str2bytes
)

this_module = sys.modules[__name__]


HELP_STRING = \
"""SY22 Systems Settings:
- Set 'Program Change' mode to 'direct'
- Set 'Bulk Protect' to 'off'
- Set 'SysEx Device ID' appropriately. If in doubt, set it the same as the
  MIDI channel or 'omni'.

Only Voices are supported, no Multi, sequencer or other capabilities.
"""

class MemoryType(IntEnum):
    INTERNAL = 0x00
    PRESET   = 0x02
    EDIT     = 0x7F

BANKS = [
    { "bank": 0, "name": "Internal", "size": 64, "type": "Voice", "isROM": False, },
    { "bank": 1, "name": "Preset",   "size": 64, "type": "Voice", "isROM": True,  },
]

class YamahaSY22(YamahaSYTGBase):

    def createBankDumpRequest(self, channel:int, bank:int) -> List[int]:
        if self.msg_id_all_voice_dump is None:
            raise ValueError("msg_id_all_voice_dump is undefined")
        return [
            SYSEX_START, YAMAHA_ID, (channel & 0x0f), self.synth_id,
            *str2bytes(self.msg_id_all_voice_dump), SYSEX_END
        ]

    def extractPatchesFromBank(self, bank: List[int]) -> List[List[int]]:
        # check for "PK  2203VM" to ensure we've a bank message to process
        if self.msg_id_all_voice_dump is None:
            raise ValueError("Undefined: msg_id_all_voice_dump")
        # check correct msg id
        if bank[6:16] != str2bytes(self.msg_id_all_voice_dump):
            raise ValueError("Not a all voice bank dump.")

        prefix = [SYSEX_START, YAMAHA_ID, 0, 0x7e, 0x04, 0x3e] + str2bytes(self.msg_id_voice_dump)
        suffix = [0, SYSEX_END]

        patches = []
        offset = 16
        patch_len = 574  # each voice is 574 bytes
        chunk_start = 1 << 16
        chunk_end = 0
        while len(patches) < 64:
            # 16-589; 590-1163; 1164-1737; 1738-2312
            for x in range(4):
                chunk_start = min(chunk_start, offset)
                chunk_end = max(chunk_end, offset+patch_len-1)
                patch = prefix + bank[offset:offset+patch_len-1] + suffix
                patch[OFFSET_CHECKSUM] = self._calculateChecksum(patch)
                patches.append(patch)
                offset += patch_len
            if len(patches) == 4:
                chunk = str2bytes(self.msg_id_all_voice_dump) + bank[chunk_start:chunk_end]
            else:
                chunk = bank[chunk_start:chunk_end]
            if bank[offset+1] != self._calculateRawChecksum(chunk):
                raise ValueError(f"Invalid checksum at offset {offset+1}.")
            offset += 3  # skip checksum + 2 length bytes
        logging.debug(f"extractPatchesFromBank(): {len(patches)} patches extracted")
        return patches

    # def _validateBulkVoiceMessage(self, buf: List[int]) -> bool:
    #     rtn = self._validateMessage(buf) and self._getMessageType(buf) == str2bytes(self.msg_id_voice_dump)
    #     logging.debug(f"validate_voice_message: rtn:{rtn}")
    #     return rtn

    # def convertPatchesToBankDump(self, patches: List[List[int]]) -> List[int]:
    #     # SY22 patch -> bank conversion
    #     device_id = (self.detected_device_id if self.detected_device_id else 0) & 0x0f
    #     rtn = []
    #     for idx, patch in enumerate(patches):
    #         buf = patch.copy()
    #         buf[OFFSET_DEVICE_ID] = device_id
    #         buf[self.offset_memory_type] = MemoryType.INTERNAL
    #         buf[self.offset_memory_number] = idx
    #         buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
    #         rtn.append(buf)
    #     return rtn

    # def createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]:
    #     memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
    #     if memtype == MemoryType.INTERNAL:
    #         bank_pc = 119
    #     elif memtype == MemoryType.PRESET:
    #         bank_pc = 121
    #     else:
    #         raise ValueError(f"Invalid memory type ({memtype}).")
    #     rtn = [
    #         0xc0 | (channel & 0x0f), bank_pc,   # 'direct mode' program change for bank selection (manual ref 2.1.3)
    #         0xc0 | (channel & 0x0f), memnum     # select the voice in the bank, based on patchNo MOD 64
    #     ]
    #     logging.debug(f"createCustomProgramChange({channel},{patchNo}) -> {bytes2str(rtn)}")
    #     return rtn

    def friendlyProgramName(self, patchNo):
        # SY22/55/etc with 1x INTERNAL bank and 1x PRESET bank:
        #   I01 (internal, slot 1), P10 (preset, slot 10)
        # same format as displayed on synth screen
        memtype, memnum = self._mapPatchNumToSynthMemory(patchNo)
        memname = ["I", None, "P"][memtype]
        bankslot = memnum % 16 + 1
        rtn = f"{memname}{bankslot:02d}"
        logging.debug(f"friendlyProgramName: MemType/Num: {memtype}/{memnum} -> rtn:{rtn}")
        return rtn

    def bankSlotToPatchNo(self, memory_type: int, memory_number: int) -> int:
        self._memoryTypeNumberChecks(memory_type, memory_number)
        if   memory_type == self.memory_types["EDIT"]:      rtn = -1
        elif memory_type == self.memory_types["INTERNAL"]:  rtn = memory_number
        elif memory_type == self.memory_types["PRESET"]:    rtn = memory_number + 64
        else:
            raise ValueError("Invalid memory type/number combination")
        return rtn

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        rtn = None
        if   patchno <=  63: rtn = (int(self.memory_types["INTERNAL"]), patchno)
        elif patchno <= 127: rtn = (int(self.memory_types["PRESET"]),  patchno -  64)
        if rtn is None:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module):
        super().install(module)
        # setattr(module, 'createBankDumpRequest', self.createBankDumpRequest)
        # setattr(module, 'extractPatchesFromBank', self.extractPatchesFromBank)
        # setattr(module, 'convertPatchesToBankDump', self.convertPatchesToBankDump)
        # setattr(module, 'createCustomProgramChange', self.createCustomProgramChange)
        setattr(module, 'friendlyProgramName', self.friendlyProgramName)
        setattr(module, 'bankSlotToPatchNo', self.bankSlotToPatchNo)


synth = YamahaSY22(
    synth_name="Yamaha SY22",
    synth_id=0x7e,

    # voice
    first_preset_name="Genesis",
    msg_id_voice_dump="PK  2203AE",
    msg_id_all_voice_dump="PK  2203VM",
    voice_default_name="Initial ",
    voice_name_length=8,
    offset_voice_name=19,

    # banks
    memory_types=MemoryType,
    banks=BANKS,

    # data offsets in voice bulk dumps
    offset_memory_type=30,          # location of memory type (eg. INT, Preset)
    offset_memory_number=31,        # location of memory number (eg. patch num.)
    offset_req_memory_type=28,      # location of memory type in request
    offset_req_memory_number=29,    # location of memory num. in request

    # misc
    help_string=HELP_STRING,
)
synth.install(this_module)


def make_test_data():
    def programs(data: testing.TestData):
        return [
            testing.ProgramTestData(
                message=data.all_messages[0], number=0, name="PowerBass!", friendly_number="I01",
                rename_name="New Patch"),
            testing.ProgramTestData(
                message=data.all_messages[2], number=2, name="VCF String", friendly_number="I03",
                rename_name="New Patch"),
            testing.ProgramTestData(
                message=data.all_messages[5], number=5, name="Ocean*View", friendly_number="I06",
                rename_name="New Patch"),
        ]

    def edit_buffers(data: testing.TestData):
        edit_buffer = synth.convertToEditBuffer(0, data.all_messages[16])
        return [
            testing.ProgramTestData(
                message=edit_buffer, name="ArticChoir", number=0, rename_name="0123456789")
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    return testing.TestData(
        sysex="testData/Yamaha_SY55/sy55ptch.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,
        program_dump_request=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4c, 0x4d, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x33, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4c, 0x4d, 0x20, 0x20,
            0x38, 0x31, 0x30, 0x33, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, SYSEX_END
        ],
        device_detect_reply=([
            # Preset Voice 1
            SYSEX_START, YAMAHA_ID, 0x00, synth.synth_id, 0x01, 0x38, 0x4c, 0x4d,
            0x20, 0x20, 0x38, 0x31, 0x30, 0x33, 0x56, 0x43,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
            0x05, 0x50, 0x69, 0x61, 0x6e, 0x6f, 0x20, 0x20,
            0x20, 0x20, 0x20, 0x02, 0x64, 0x0c, 0x09, 0x14,
            0x02, 0x00, 0x01, 0x05, 0x0c, 0x00, 0x01, 0x00,
            0x0c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0e, 0x00,
            0x00, 0x00, 0x7f, 0x00, 0x00, 0x7f, 0x00, 0x40,
            0x00, 0x7f, 0x01, 0x7f, 0x20, 0x0d, 0x00, 0x00,
            0x00, 0x00, 0x45, 0x40, 0x03, 0x00, 0x00, 0x00,
            0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 0x01, 0x03,
            0x01, 0x3d, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x65, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x3f, 0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x03, 0x24, 0x40, 0x51, 0x67, 0x01, 0x07,
            0x01, 0x03, 0x00, 0x7f, 0x00, 0x79, 0x00, 0x7f,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x03, 0x00,
            0x2a, 0x54, 0x7f, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x00, 0x07, 0x03, 0x00, 0x3d,
            0x08, 0x08, 0x05, 0x19, 0x3a, 0x2d, 0x06, 0x00,
            0x2a, 0x54, 0x7f, 0x01, 0x01, 0x00, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x04, 0x00, 0x03, 0x2a, 0xf7,
            ], 0
        ),
        expected_patch_count=64,
        friendly_bank_name=(0, "Internal"),
    )
