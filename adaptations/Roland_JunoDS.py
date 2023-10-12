#
#   Copyright (c) 2022-2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Finally owning a classic Roland so I can make a working and tested example on how to implement the Roland Synths
import sys
from typing import List

import knobkraft.sysex
import testing.test_data
from roland import DataBlock, RolandData, GenericRoland

this_module = sys.modules[__name__]

_juno_ds_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x50, "Patch common"),
                      DataBlock((0x00, 0x00, 0x02, 0x00), (0x01, 0x11), "Patch common MFX"),
                      DataBlock((0x00, 0x00, 0x04, 0x00), 0x54, "Patch common Chorus"),
                      DataBlock((0x00, 0x00, 0x06, 0x00), 0x53, "Patch common Reverb"),
                      DataBlock((0x00, 0x00, 0x10, 0x00), 0x29, "Patch common Tone Mix Table"),
                      DataBlock((0x00, 0x00, 0x20, 0x00), (0x01, 0x1a), "Tone 1"),
                      DataBlock((0x00, 0x00, 0x22, 0x00), (0x01, 0x1a), "Tone 2"),
                      DataBlock((0x00, 0x00, 0x24, 0x00), (0x01, 0x1a), "Tone 3"),
                      DataBlock((0x00, 0x00, 0x26, 0x00), (0x01, 0x1a), "Tone 4")]
_juno_ds_edit_buffer_addresses = RolandData("Juno-DS Temporary Patch/Drum (patch mode part 1)", 1, 4, 4,
                                           (0x1f, 0x00, 0x00, 0x00),
                                           _juno_ds_patch_data)
_juno_ds_program_buffer_addresses = RolandData("Juno-DS User Patches", 256, 4, 4,
                                              (0x30, 0x00, 0x00, 0x00),
                                              _juno_ds_patch_data)

juno_ds = GenericRoland("Roland Juno-DS",
                        model_id=[0x00, 0x00, 0x3a],
                        address_size=4,
                        edit_buffer=_juno_ds_edit_buffer_addresses,
                        program_dump=_juno_ds_program_buffer_addresses,
                        category_index=0x0c,
                        device_family=[0x3a, 0x02, 0x02])
juno_ds.install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    return testing.TestData(device_detect_call="f0 7e 00 06 01 f7",
                            device_detect_reply=("f0 7e 10 06 02 41 3a 02 02 00 00 03 00 00 f7", 0))

def test_program_dump():
    message = knobkraft.sysex.stringToSyx("f0 41 10 00 00 3a 12 30 7f 26 00 7f 40 40 00 40 40 00 40 01 00 00 00 7f 00 00 7f 7f 00 01 01 01 00 00 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 00 00 00 00 01 00 00 00 00 00 00 00 00 01 00 00 00 00 4a 40 40 40 40 40 28 50 28 00 40 22 5e 40 40 00 7f 40 01 40 00 40 40 01 40 40 40 40 00 0a 0a 40 00 7f 7f 7f 00 40 3c 03 01 60 40 40 40 00 0a 0a 0a 7f 7f 7f 01 05 0c 02 00 00 40 00 00 00 40 40 40 40 01 05 0c 02 00 00 40 00 00 00 40 40 40 40 00 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 3a f7")
    assert juno_ds.isPartOfSingleProgramDump(message)

