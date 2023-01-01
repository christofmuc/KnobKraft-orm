#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import sequential
import sys

this_module = sys.modules[__name__]

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="DSI Pro 2",
                                     device_id=0b00101100,  # See Page 134 of the Pro 2 manual
                                     banks=8,
                                     patches_per_bank=99,
                                     name_len=20,
                                     name_position=378,
                                     ).install(this_module)


def setupHelp():
    return "The DSI Pro 2 has two relevant global settings:\n\n" \
           "1. You must set MIDI Sysex Enable to On\n" \
           "2. You must choose the MIDI Sysex Cable.\n\n" \
           "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
           "Both settings are accessible via the GLOBALS menu."


# Test data picked up by test_adaptation.py
def test_data():
    def programs(messages):
        yield {"message": messages[0], "name": "Cascades", "number": 1}

    return {"sysex": "testData/Pro_2_Programs_v1.0a.syx", "program_generator": programs}
