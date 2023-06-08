#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

import sequential
import sys

this_module = sys.modules[__name__]

#
# Configure the GenericSequential module
#
synth = sequential.GenericSequential(name="DSI OB-6 Adaptation",
                                     device_id=0b00101110,
                                     banks=10,
                                     patches_per_bank=100,
                                     name_len=20,
                                     name_position=107,
                                     friendlyBankName=lambda x: f"{x}00 - {x}99",  # no bank names, but rather the hundredth
                                     friendlyProgramName=lambda x: f"{x:03d}",  # triple digit
                                     ).install(this_module)


def setupHelp():
    return "The DSI/Sequential OB-6 has only one relevant global setting:\n\n" \
           "You must choose the MIDI Sysex Cable.\n\n" \
           "Options are DIN MIDI cable or the USB for sysex. USB is much faster.\n\n" \
           "This is GLOBAL setting 8."


# Test data picked up by test_adaptation.py
#def make_test_data():
#    def programs(messages):
#        yield {"message": messages[0], "name": "Cascades", "number": 1}
#
#    return {"sysex": "testData/Pro_2_Programs_v1.0a.syx", "program_generator": programs}
