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

import logging
import sys
from enum import IntEnum
from typing import List

import knobkraft.sysex
import testing
from yamaha.Yamaha_SY_TG_common import (
    OFFSET_CHECKSUM,
    OFFSET_DEVICE_ID,
    SYSEX_END,
    SYSEX_START,
    YAMAHA_ID,
    YamahaSYTGBase,
    bytes2str,
    str2bytes,
)

this_module = sys.modules[__name__]


HELP_STRING = """SY22 Systems Settings:

- Set 'Program Change' mode to 'direct'
- Set 'Bulk Protect' to 'off'
- Set 'SysEx Device ID' appropriately. If in doubt, set it the same as the
  MIDI channel or 'omni'.
- Bank downloads are supported but not uploads
- Only supports voice uploads to edit buffer

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


class YamahaSY22(YamahaSYTGBase):
    msg_id_alt_voice_dump = None  # used in subclasses
    msg_id_alt_all_voice_dump = None  # used in subclasses

    # ##### DEVICE DETECTION CAPABILITIES

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0F)
        buf = self._makeMessage(
            device_id, str2bytes(self.msg_id_voice_dump), add_checksum=False
        )
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
        device_id = 0x20 | (channel & 0x0F)
        buf = self._makeMessage(
            device_id, str2bytes(self.msg_id_voice_dump), add_checksum=False
        )
        logging.debug(
            f"createProgramDumpRequest({channel}, {patchNo}): {bytes2str(buf[:32])}"
        )
        return buf

    def convertToProgramDump(
        self, channel: int, message: List[int], patchNo: int
    ) -> List[int]:
        buf = message.copy()
        buf[OFFSET_DEVICE_ID] = channel & 0x0F
        buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
        logging.debug(f"convertToProgramDump({channel}, msg): bytes:{len(buf)}")
        return buf

    # ##### BANK CAPABILITIES

    def createBankDumpRequest(self, channel: int, bank: int) -> List[int]:
        if self.msg_id_all_voice_dump is None:
            raise ValueError("msg_id_all_voice_dump is not defined")
        device_id = 0x20 | (channel & 0x0F)
        buf = self._makeMessage(
            device_id, str2bytes(self.msg_id_all_voice_dump), add_checksum=False
        )
        logging.debug(
            f"createBankDumpRequest({channel}, {bank}): {bytes2str(buf[:32])}"
        )
        return buf

    def extractPatchesFromBank(self, bank: List[int]) -> List[int]:
        msg_ids = []
        if self.msg_id_all_voice_dump is not None:
            msg_ids.append(str2bytes(self.msg_id_all_voice_dump))
        if self.msg_id_alt_all_voice_dump is not None:
            msg_ids.append(str2bytes(self.msg_id_alt_all_voice_dump))
        if self._getMessageType(bank) not in msg_ids:
            raise ValueError(
                f"Unsupported message_id: {bytes2str(self._getMessageType(bank))}"
            )

        if self.msg_id_all_voice_dump is None:
            raise ValueError("msg_id_all_voice_dump is not defined.")
        if len(bank) < 6:
            raise ValueError("File is too short to be a valid SysEx message.")
        if bank[0] != SYSEX_START:
            raise ValueError(f"Invalid Status Byte: Expected 0xF0, got 0x{bank[0]:02x}")
        if bank[1] != YAMAHA_ID:
            raise ValueError(
                f"Invalid Manufacturer ID: Expected 0x43 (Yamaha), got 0x{bank[1]:02x}"
            )
        # channel = bank[2] & 0x0F
        if bank[3] != self.synth_id:
            raise ValueError(
                f"Invalid Synth ID: Expected 0x{self.synth_id:02x}, got 0x{bank[3]:02x}"
            )

        # all_voice_bytes, ptr = self._parseBankBlocks(bank)
        ptr = 4  # Pointer setup (skip the 4 byte header)
        all_voice_bytes = []
        expected_total_blocks = 16  # 64 voices total / 4 voices per block = 16 blocks

        for block_idx in range(expected_total_blocks):
            if ptr + 2 > len(bank):
                raise Exception(
                    f"Error: Unexpected end of file before reading Block {block_idx} length."
                )

            # Read the 7-bit MSB/LSB Byte Count for this packet
            block_size = (bank[ptr] << 7) | bank[ptr + 1]
            ptr += 2

            if ptr + block_size + 1 > len(bank):
                raise Exception(f"Error: File truncated inside Block {block_idx}.")

            # Extract the block payload
            block_payload = bank[ptr : ptr + block_size]
            ptr += block_size

            # Read Checksum byte
            file_checksum = bank[ptr]
            ptr += 1

            # Verify Checksum
            # Currently just logs a warning if checksums don't match. Some
            # sysex banks found online seems to have corrupt checksums even
            # though the patches seem to extract ok.
            calculated_checksum = self._calculateRawChecksum(block_payload)
            # d_start = "".join([f"{x:02x}" for x in block_payload[:4]])
            # d_end   = "".join([f"{x:02x}" for x in block_payload[-4:]])
            # print(f"{ptr:05d} {block_size:4d} {d_start:s}...{d_end:s} {file_checksum:02x} calc:{calculated_checksum:02x}")
            if calculated_checksum != file_checksum:
                logging.warning(
                    f"Waring: Checksum mismatch in Block {block_idx}! "
                    f"(Expected 0x{file_checksum:02x}, calculated 0x{calculated_checksum:02x})"
                )

            # remove the first block's 10-byte ASCII header (msg_id_all_voice_dump)
            if block_idx == 0:
                all_voice_bytes += block_payload[len(self.msg_id_all_voice_dump) :]
            else:
                all_voice_bytes += block_payload

        # ##### Parsing Individual Voices
        total_voices = 64
        if len(all_voice_bytes) == 0:
            print("No voice data extracted.")
            return []

        # detect voice size: SY33/SY35 differ from TG33
        voice_size = len(all_voice_bytes) // total_voices
        payload_size = voice_size + len(self.msg_id_all_voice_dump)

        # Reconstruct voices
        patch_prefix = [
            SYSEX_START,
            YAMAHA_ID,
            0,
            0x7E,
            (payload_size & 0x3F80) >> 7,
            payload_size & 0x7F,
            *str2bytes(self.msg_id_voice_dump),
        ]
        patch_suffix = [0, SYSEX_END]
        patches = []
        for i in range(total_voices):
            start_idx = i * voice_size
            end_idx = start_idx + voice_size
            voice_patch = (
                patch_prefix + list(all_voice_bytes[start_idx:end_idx]) + patch_suffix
            )
            voice_patch[OFFSET_CHECKSUM] = self._calculateChecksum(voice_patch)
            if not self._validateVoiceMessage(voice_patch):
                print(" ".join([f"{i:02x}" for i in voice_patch]))
                raise ValueError("extractPatchesFromBank(): Invalid voice message")
            patches += voice_patch
            # for debugging
            # voice_name = "".join(
            #     [
            #         chr(c)
            #         for c in voice_patch[
            #             self.offset_voice_name : self.offset_voice_name
            #             + self.voice_name_length
            #         ]
            #     ]
            # )
            # print(f"Voice: {i:2d} {voice_name}")

        # ##### Multi Data & EOX Footers
        # After the 16th voice block, the file contains a Multi Data block and end with 0xF7
        if ptr < len(bank) and bank[ptr] != SYSEX_END:
            multi_msb = bank[ptr]
            multi_lsb = bank[ptr + 1]
            ptr += 2
            multi_size = (multi_msb << 7) | multi_lsb
            ptr += multi_size + 1  # skip payload + multi checksum

        if ptr < len(bank) and bank[ptr] == SYSEX_END:
            logging.debug("Reached End of Exclusive (EOX) marker successfully.")

        return patches

    def isBankDumpFinished(self, message: List[List[int]]) -> bool:
        return message[-1][-1] == SYSEX_END

    def convertToEditBuffer(self, channel: int, msg: List[int]) -> List[int]:
        buf = msg.copy()
        buf[OFFSET_DEVICE_ID] = channel & 0x0F
        buf[OFFSET_CHECKSUM] = self._calculateChecksum(buf)
        logging.debug(
            f"convertToEditBuffer({channel}, msg): "
            f"MemType/Num:{msg[self.offset_memory_type]:02x}{msg[self.offset_memory_number]:02x}"
            f"->{buf[self.offset_memory_type]:02x}{buf[self.offset_memory_number]:02x}"
        )
        return buf

    def isEditBufferDump(self, buf: List[int]) -> bool:
        # SY22 can't place individual voices into internal memory slots, only to the edit
        # buffer, so here we just check it's a valid voice message
        return self._validateVoiceMessage(buf)

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
        if rtn is None:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module):
        super().install(module)
        setattr(module, "createBankDumpRequest", self.createBankDumpRequest)
        setattr(module, "convertToProgramDump", self.convertToProgramDump)
        setattr(module, "isBankDumpFinished", self.isBankDumpFinished)
        setattr(module, "extractPatchesFromBank", self.extractPatchesFromBank)
        setattr(module, "convertToEditBuffer", self.convertToEditBuffer)
        setattr(module, "bankSlotToPatchNo", self.bankSlotToPatchNo)
        delattr(module, "numberFromDump")


synth = YamahaSY22(
    synth_name="Yamaha SY22",
    synth_id=0x7E,
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
        stream = synth.extractPatchesFromBank(data.all_messages[0])
        patches = knobkraft.sysex.splitSysex(stream)
        return [
            testing.ProgramTestData(
                message=patches[0],
                number=0,
                name="Genesis ",
                friendly_number="I01",
                rename_name="NewName0",
            ),
            testing.ProgramTestData(
                message=patches[2],
                number=2,
                name="Full Str",
                friendly_number="I03",
                rename_name="NewName2",
            ),
            testing.ProgramTestData(
                message=patches[5],
                number=5,
                name="PowerBrs",
                friendly_number="I06",
                rename_name="NewName5",
            ),
        ]

    def edit_buffers(data: testing.TestData):
        stream = synth.extractPatchesFromBank(data.all_messages[0])
        edit_buffer = knobkraft.sysex.splitSysex(stream)[5]
        return [
            testing.ProgramTestData(
                message=edit_buffer, number=5, name="PowerBrs", rename_name="NewName5"
            )
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    td = testing.TestData(
        sysex="testData/Yamaha_SY22_SY35_TG33/SY22A.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,  # type: ignore
        program_dump_request=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x50, 0x4B, 0x20, 0x20,
            0x32, 0x32, 0x30, 0x33, 0x41, 0x45, SYSEX_END,
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x50, 0x4B, 0x20, 0x20,
            0x32, 0x32, 0x30, 0x33, 0x41, 0x45, SYSEX_END,
        ],
        device_detect_reply=([
            SYSEX_START, YAMAHA_ID, 0x00, synth.synth_id, 0x04, 0x48, 0x50, 0x4B,
            0x20, 0x20, 0x32, 0x32, 0x30, 0x33, 0x41, 0x45,
            0x01, 0x25, 0x00, 0x53, 0x74, 0x65, 0x65, 0x6C,
            0x20, 0x20, 0x20, 0x01, 0x02, 0x02, 0x00, 0x00,
            0x7F, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00,
            0x02, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x30, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x3F,
            0x00, 0x53, 0x15, 0x16, 0x7F, 0x00, 0x08, 0x7F,
            0x00, 0x75, 0x00, 0x00, 0x02, 0x00, 0x33, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x70, 0x04, 0x06, 0x00,
            0x13, 0x08, 0x00, 0x00, 0x53, 0x00, 0x1B, 0x00,
            0x04, 0x03, 0x00, 0x0F, 0x3F, 0x35, 0x3B, 0x00,
            0x41, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00,
            0x7F, 0x1A, 0x23, 0x7F, 0x00, 0x0A, 0x7F, 0x4F,
            0x00, 0x00, 0x02, 0x00, 0x20, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x30, 0x02, 0x04, 0x00, 0x00, 0x10,
            0x00, 0x3F, 0x00, 0x53, 0x15, 0x16, 0x7F, 0x00,
            0x08, 0x7F, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00,
            0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00,
            0x05, 0x00, 0x03, 0x08, 0x01, 0x01, 0x03, 0x00,
            0x3F, 0x00, 0x56, 0x10, 0x00, 0x71, 0x2F, 0x32,
            0x5F, 0x00, 0x01, 0x0A, 0x00, 0x00, 0x00, 0x00,
            0x3F, 0x00, 0x7F, 0x19, 0x23, 0x7F, 0x00, 0x0A,
            0x7F, 0x03, 0x03, 0x01, 0x7F, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x01,
            0x7F, 0x1F, 0x1F, 0x01, 0x7F, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x00,
            0x00, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x01,
            0x7F, 0x1F, 0x1F, 0x00, 0x00, 0x0C, 0x51, SYSEX_END
        ], 0),
        expected_patch_count=64,
        friendly_bank_name=(0, "Internal"),
    )
    return td
