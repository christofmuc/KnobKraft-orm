#
#   Copyright (c) 2024 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys

from roland import DataBlock, RolandData, GenericRoland

this_module = sys.modules[__name__]

# TD-07 V-Drum
_td07_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x1b, "Kit common"),
                    DataBlock((0x00, 0x00, 0x01, 0x00), 0x4c, "Kit MIDI"),
                    DataBlock((0x00, 0x00, 0x10, 0x00), (0x01, 0x06), "Kit MFX")] + \
                   [DataBlock((0x00, 0x00, 0x20 + i, 0x00), 0x19, f"Kit Unit Common {i}") for i in range(15)] + \
                   [DataBlock((0x00, 0x00, 0x40 + i, 0x00), 0x10, f"Kit Unit Layer {i}") for i in range(15)] + \
                   [DataBlock((0x00, 0x00, 0x60 + i, 0x00), 0x21, f"Kit Unit VEdit {i}") for i in range(15)] + \
                   [DataBlock((0x01, 0x10, 0x00, 0x00), 0x11, "Kit MCR Room"),
                    DataBlock((0x01, 0x20, 0x00, 0x00), 0x07, "Kit MCR Overhead")]
_td07_edit_buffer = RolandData("TD-07 fake edit buffer at program place #50"
                               , num_items=50
                               , num_address_bytes=4
                               , num_size_bytes=4
                               , base_address=(0x03, 0x00, 0x00, 0x00)
                               , blocks=_td07_patch_data)
_td07_kit_addresses = RolandData("TD-07 stored kit"
                                 , num_items=0x50
                                 , num_address_bytes=4
                                 , num_size_bytes=4
                                 , base_address=(0x03, 0x00, 0x00, 0x00)
                                 , blocks=_td07_patch_data)
_td07_setup_message = RolandData("TD-07 setup", 1, 4, 4, (0x01, 0x00, 0x00, 0x00),
                                 [DataBlock((0x00, 0x00, 0x00, 0x00), 0x07, "Metronome"),
                                  DataBlock((0x00, 0x00, 0x01, 0x00), 0x04, "SetupMisc")])

# Useful link: https://github.com/jskeet/DemoCode/blob/main/Drums/VDrumExplorer.Model/SchemaResources/TD07/TD07.json
td_07 = GenericRoland("Roland TD-07",
                      model_id=[0x75],
                      device_family=[0x75, 0x03],
                      address_size=4,
                      edit_buffer=_td07_edit_buffer,
                      program_dump=_td07_kit_addresses,
                      device_detect_message=_td07_setup_message)

td_07.install(this_module)
