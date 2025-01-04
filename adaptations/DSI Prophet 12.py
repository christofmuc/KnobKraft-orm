#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
from typing import List

import sequential
import sys

import testing

this_module = sys.modules[__name__]


#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="DSI Prophet 12",
                                     device_id=0b00101010,  # See Page 82 of the prophet 12 manual
                                     banks=8,
                                     patches_per_bank=99,
                                     name_len=20,
                                     name_position=402,
                                     id_list=[0b00101010, 0x2b],  # The Pro12 Desktop module calls itself 0x2b,
                                     blank_out_zones=[(914, 20)],  # Make sure to blank out the layer B name as well
                                     friendlyBankName=lambda x: f"bank {x+1}",  # 1-based bank names
                                     friendlyProgramName=lambda x: f"bank {(x//99)+1} - {(x % 99)+1:02d}",  # 1-based program names with leading zero
                                     numberOfLayers=2,
                                     layerNameIndex=[(402, 20), (402+512, 20)]
                                     ).install(this_module)


#
# Synth specific functions
#


def setupHelp():
    return "The DSI Prophet 12 has two relevant global settings:\n\n" \
           "1. You must set MIDI Sysex Enable to On\n" \
           "2. You must choose the MIDI Sysex Cable.\n\n" \
           "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
           "Both settings are accessible via the GLOBALS menu."


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[5], name='Wurly Trem ModWheel', number=5, second_layer_name= "Electric Standard",
                                      friendly_number="bank 1 - 06")

    return testing.TestData(sysex="testData/P12_Programs_v1.1c.syx", program_generator=programs)
