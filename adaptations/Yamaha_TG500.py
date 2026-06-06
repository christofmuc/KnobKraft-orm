"""
# Yamaha TG500 adaptation.

Adaptation created by github.com/hmmbug.

- The TG500 has different settings for MIDI Channel and SysEx Device ID.
  (UTILITY → SYNTH SETUP → MENU → 2:MIDI 1)
- Requires synth to be in program change "direct" mode
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Requires "Bulk Protect" set to "off"
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Bank transmission is not supported as Knobcraft currently doesn't provide a
  means to select which of the four internal banks to load the patches.
  Defaulting to override bank 0 (Internal1) was not considered a good idea.


## Mapping SysEx Banks to Interface Banks

The TG500 maps voices (patches) to 4 banks:

- Preset1 - Preset4 (read-only), 64 voice slots each, non-adressable via SysEx
- Internal1, Internal2 (editable), 64 voice slots each

The last voice in each bank is a special drum voice.

## References:
- TG500 Manuals:
  https://usa.yamaha.com/support/manuals/index.html?l=en&c=music_production&k=tg500
"""

from enum import IntEnum
import logging
import sys
from types import ModuleType
from typing import List

import testing

logging.basicConfig(level=logging.DEBUG)

from yamaha.Yamaha_SY_TG_common import (
    bytes2str,
    SYSEX_START,
    YAMAHA_ID,
    OFFSET_DEVICE_ID,
    OFFSET_CHECKSUM,
    SYSEX_END,
    str2bytes,
)

from Yamaha_SY85 import YamahaSY85

this_module = sys.modules[__name__]


HELP_STRING = \
"""TG500 Systems Settings:
- The TG500 has different settings for MIDI Channel and SysEx Device ID.
  (UTILITY → SYNTH SETUP → MENU → 2:MIDI 1)
- Requires synth to be in program change "direct" mode
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)
- Requires "Bulk Protect" set to "off"
  (UTILITY → SYNTH SETUP → MENU → 3:MIDI 2)

Limitations:
- Only Voices are supported, no Multi, sequencer or other capabilities.
- Bank transmission is not supported.
- May be confused with a Yamaha SY85 if both connected simultaneously.
"""

class MemoryType(IntEnum):
    INTERNAL1 = 0x00
    INTERNAL2 = 0x03
    EDIT      = 0x7F

BANKS = [
    { "bank": 0, "name": "Internal1", "size": 64, "type": "Voice", "isROM": False, },
    { "bank": 1, "name": "Internal2", "size": 64, "type": "Voice", "isROM": False, },
]


class YamahaTG500(YamahaSY85):

    # ##### DEVICE DETECTION CAPABILITIES

    def createDeviceDetectMessage(self, channel: int) -> List[int]:
        device_id = 0x20 | (channel & 0x0f)
        # Request INTERNAL4, voice 0. No other synth should respond to this request
        msg = str2bytes(self.msg_id_voice_dump) + [0x00]*14 + [int(self.memory_types.INTERNAL1), 0]
        buf = self._makeMessage(device_id, msg, add_checksum=False)
        logging.debug(f"createDeviceDetectMessage({channel}: {bytes2str(buf[:32])} ... {bytes2str(buf[-2:])}")
        return buf


    # ##### PROGRAM DUMP CAPABILITY

    def createCustomProgramChange(self, channel: int, patchNo: int) -> List[int]:
        bankchange  = [0, 3][(patchNo and 0b111000000) >> 6]
        memnum = patchNo and 0b111111
        rtn = [
            0xb0 | (channel & 0x0f), 0x00, bankchange,  # requires TG500 set to Program Change "direct" mode
            0xc0 | (channel & 0x0f), memnum
        ]
        logging.debug(f"createCustomProgramChange({channel},{patchNo}) -> {bytes2str(rtn)}")
        return rtn


    # ##### BANK CAPABILITIES

    def friendlyProgramName(self, patchNo: int) -> str:
        # TG500: I[12]-nn, e.g. I2-34
        banks = ["I1", "I2"]
        bank  = banks[(patchNo >> 6) & 0b111]
        prog  = (patchNo & 0b00111111) + 1
        rtn = f"{bank}-{prog:02d}"
        logging.debug(f"friendlyProgramName: patchNo:{patchNo} -> rtn:{rtn}")
        return rtn

    def friendlyBankName(self, bank_number: int) -> str:
        rtn = None
        if   bank_number == 0: rtn = BANKS[0]["name"]
        elif bank_number == 3: rtn = BANKS[1]["name"]
        if rtn is None:
            raise ValueError(f"Invalid bank number ({bank_number})")
        logging.debug(f"friendlyBankName(bank_number) -> {rtn}")
        return rtn

    def bankSlotToPatchNo(self, memory_type: int, memory_number: int) -> int:
        self._memoryTypeNumberChecks(memory_type, memory_number)
        if   memory_type == self.memory_types["EDIT"]:      rtn = -1
        elif memory_type == self.memory_types["INTERNAL1"]: rtn = memory_number
        elif memory_type == self.memory_types["INTERNAL2"]: rtn = memory_number + 64
        else:
            raise ValueError("Invalid memory type/number combination")
        return rtn

    def _mapPatchNumToSynthMemory(self, patchno: int) -> tuple[int, int]:
        "Maps Knobcraft flat patch numbers to synth memory types and banks"
        if   patchno <=  63: rtn = (int(self.memory_types["INTERNAL1"]), patchno)
        elif patchno <= 127: rtn = (int(self.memory_types["INTERNAL2"]), patchno -  64)
        else:
            raise ValueError(f"Invalid patchno ({patchno})")
        return rtn

    def install(self, module: ModuleType):
        super().install(module)
        setattr(module, 'createDeviceDetectMessage', self.createDeviceDetectMessage)
        setattr(module, 'createCustomProgramChange', self.createCustomProgramChange)
        setattr(module, 'friendlyProgramName', self.friendlyProgramName)
        setattr(module, 'friendlyBankName', self.friendlyBankName)
        setattr(module, 'bankSlotToPatchNo', self.bankSlotToPatchNo)


synth = YamahaTG500(
    synth_name="Yamaha TG500",
    synth_id=0x7a,

    # voice
    first_preset_name="",          # No addressable presets on TG500
    msg_id_voice_dump="LM  0065VC",
    voice_default_name="Init Vce",
    voice_name_length=8,           # 8 characters in the voice name
    offset_voice_name=105,         # location of voice name

    # banks
    memory_types=MemoryType,
    banks=BANKS,

    # data offsets in voice bulk dumps
    offset_memory_type=30,          # location of memory type (eg. INT, Preset)
    offset_memory_number=31,        # location of memory number (eg. patch num.)
    offset_req_memory_type=28,      # location of memory type in request
    offset_req_memory_number=29,    # location of memory num. in request

    help_string=HELP_STRING,
)
synth.install(this_module)

# # ##### TEST FUNCTIONS

# Unimplemented tests:
# - layers: not applicable
# - legacy_loader: no idea.. it's legacy so I guess it's deprecated...
# - *_via_mock_device: mock device not implemented

def make_test_data():
    def programs(data: testing.TestData):
        return [
            testing.ProgramTestData(
                message=data.all_messages[0],
                number=0,
                name="SP Makro",
                friendly_number="I1-01",
                rename_name="NewName1"
            ),
            testing.ProgramTestData(
                message=data.all_messages[6],
                number=6,
                name="SP Abyss",
                friendly_number="I1-07",
                rename_name="NewName2"
            ),
            testing.ProgramTestData(
                message=data.all_messages[126],
                number=126,
                name="ME Templ",
                friendly_number="I2-63",
                rename_name="NewName3"
            ),
        ]

    def edit_buffers(data: testing.TestData):
        edit_buffer = synth.convertToEditBuffer(0, data.all_messages[16])
        return [
            testing.ProgramTestData(
                message=edit_buffer, name="BA Wood ", number=0, rename_name="01234567")
        ]

    def banks(test_data: testing.TestData):
        yield test_data.all_messages[0]

    return testing.TestData(
        sysex="testData/Yamaha_TG500/TG500_test_bank.syx",
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        bank_generator=banks,
        program_dump_request=[
            # Internal 1, Group A, Voice 1 dump request
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4c, 0x4d, 0x20, 0x20, 0x30, 0x30, 0x36, 0x35, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END
        ],
        device_detect_call=[
            SYSEX_START, YAMAHA_ID, 0x20, synth.synth_id, 0x4c, 0x4d, 0x20, 0x20, 0x30, 0x30, 0x36, 0x35, 0x56, 0x43, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SYSEX_END
        ],
        device_detect_reply=(
            [
                # Internal 4 Group A Voice 1 dump
                0xf0, 0x43, 0x00, 0x7a, 0x01, 0x68, 0x4c, 0x4d, 0x20, 0x20, 0x30, 0x30, 0x36, 0x35, 0x56, 0x43,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
                0x00, 0x01, 0x19, 0x01, 0x15, 0x00, 0x08, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00,
                0x00, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34,
                0x00, 0x0c, 0x64, 0x64, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a,
                0x00, 0x00, 0x00, 0x04, 0x00, 0x37, 0x00, 0x07, 0x00, 0x06, 0x00, 0x14, 0x64, 0x64, 0x32, 0x32,
                0x32, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x50, 0x20, 0x4d, 0x61, 0x6b, 0x72,
                0x6f, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x40,
                0x28, 0x64, 0x1b, 0x00, 0x32, 0x7f, 0x7f, 0x00, 0x01, 0x00, 0x00, 0x40, 0x40, 0x00, 0x19, 0x3f,
                0x3f, 0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 0x20, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
                0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x3f, 0x3f, 0x00, 0x18, 0x3f, 0x3f, 0x24, 0x37, 0x4c,
                0x60, 0x01, 0x00, 0x01, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x22, 0x00, 0x09, 0x00, 0x14, 0x00,
                0x00, 0x00, 0x10, 0x3e, 0x1d, 0x0f, 0x0e, 0x0f, 0x00, 0x00, 0x6d, 0x61, 0x53, 0x49, 0x40, 0x40,
                0x40, 0x00, 0x24, 0x37, 0x4c, 0x60, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x27, 0xf7,
           ], 0
        ),
        expected_patch_count=128,
        friendly_bank_name=(0, "Internal1"),
    )
