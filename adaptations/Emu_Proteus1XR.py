import sys
from typing import List

import knobkraft
import testing
from emu.GenericEmu import GenericEmu

this_module = sys.modules[__name__]

proteus1xr = GenericEmu(name="E-mu Proteus/1 XR", model_id=0x07, banks=[
    {"bank": 0, "name": "RAM", "size": 64, "type": "Preset"},
    {"bank": 1, "name": "RAM", "size": 64, "type": "Preset"},
    {"bank": 2, "name": "RAM", "size": 64, "type": "Preset"},
    {"bank": 3, "name": "RAM", "size": 64, "type": "Preset"},
    {"bank": 4, "name": "ROM", "size": 64, "type": "Preset", "isRom": True},
    {"bank": 5, "name": "ROM", "size": 64, "type": "Preset", "isRom": True}
])
proteus1xr.install(this_module)


# Test data picked up by test_adaptation.py
def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        yield testing.ProgramTestData(message=data.all_messages[0], name="Stereo Piano", number=0)  # Adjusted test data for Proteus
        yield testing.ProgramTestData(message=data.all_messages[63], name="Mtlphn Arp 9", number=63)
        yield testing.ProgramTestData(message=data.all_messages[191], name="Lazer Ray   ", number=191)

        additional_syx = "testData/E-mu_Proteus/Proteus2Presets.syx"
        more_messages = knobkraft.load_sysex(additional_syx, as_single_list=False)
        yield testing.ProgramTestData(message=more_messages[0], name="Winter Signs", number=64)  # Adjusted test data for Proteus

    return testing.TestData(sysex="testData/E-mu_Proteus/Proteus1XRPresets.syx", program_generator=programs)
