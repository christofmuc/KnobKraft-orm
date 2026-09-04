#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from pathlib import Path
from typing import List

import sequential
import sys

import testing

this_module = sys.modules[__name__]

BANK_COUNT = 8
USER_BANK_COUNT = 4
PATCHES_PER_BANK = 128


def pro3_bank_name(bank):
    if not 0 <= bank < BANK_COUNT:
        raise ValueError(f"Invalid Pro 3 bank {bank}")
    # Firmware notes for 1.2.0.4 confirm F5-F8, despite the manual's F1-F4:
    # https://forum.sequential.com/index.php?topic=5201.0
    return f"{'U' if bank < USER_BANK_COUNT else 'F'}{bank + 1}"


def pro3_program_name(program):
    bank, slot = divmod(program, PATCHES_PER_BANK)
    return f"{pro3_bank_name(bank)}-P{slot + 1}"

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="Sequential Pro 3",
                                     device_id=0b00110001,  # See Page 147 of the Pro 3 manual
                                     banks=BANK_COUNT,
                                     patches_per_bank=PATCHES_PER_BANK,
                                     name_len=20,
                                     name_position=321,
                                     friendlyBankName=pro3_bank_name,
                                     friendlyProgramName=pro3_program_name,
                                     ).install(this_module)


def bankDescriptors():
    return [{
        "bank": bank,
        "name": f"{'User' if bank < USER_BANK_COUNT else 'Factory'} Bank {pro3_bank_name(bank)}",
        "size": PATCHES_PER_BANK,
        "type": "Single Patch",
        "isROM": bank >= USER_BANK_COUNT,
    } for bank in range(BANK_COUNT)]


def generalMessageDelay():
    # Radek Pilich reports stalled transfers without this pacing in issue #545.
    # https://github.com/christofmuc/KnobKraft-orm/issues/545
    return 50


def setupHelp():
    return (
        "In Global settings, enable MIDI Rx Prog Chg (MIDI Program Receive), and set MIDI SysEx Cable "
        "to the USB or MIDI connection used by KnobKraft. Select a matching MIDI channel and output port. "
        "User banks U1-U4 are writable; factory banks F5-F8 are read-only. Each bank has 128 programs. "
        "Transfers use a 50 ms message delay following the hardware report in issue #545."
    )


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[2], name='Staircase', number=2, friendly_number='U1-P3')

    return testing.TestData(
        sysex=str(Path(__file__).resolve().parent / "testData" / "P3_Factory_Sounds_v1.01.syx"),
        program_generator=programs, friendly_bank_name=(4, "F5"), expected_patch_count=512)
