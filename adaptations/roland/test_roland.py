import pytest
from .GenericRoland import *

_jv80_patch_data = [DataBlock((0x00, 0x00, 0x00, 0x00), 0x22, "Patch common"),
                    DataBlock((0x00, 0x00, 0x08, 0x00), 0x73, "Patch tone 1"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x09, 0x00), 0x73, "Patch tone 2"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0A, 0x00), 0x73, "Patch tone 3"),  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
                    DataBlock((0x00, 0x00, 0x0B, 0x00), 0x73, "Patch tone 4")]  # 0x74 size for the JV-880, 0x75 size for the JV-90/JV-1000
_jv80_edit_buffer_addresses = RolandData("JV-80 Temporary Patch"
                                         , num_items=1
                                         , num_address_bytes=4
                                         , num_size_bytes=4
                                         , base_address=(0x00, 0x08, 0x20, 0x00)
                                         , blocks=_jv80_patch_data)


def test_block_sizes():
    assert _jv80_patch_data[0].size == 0x22
    assert _jv80_edit_buffer_addresses._start_index_of_block(0, 5) == 0
    assert _jv80_edit_buffer_addresses._end_index_of_block(0, 5) == _jv80_edit_buffer_addresses._start_index_of_block(0, 5) + 0x22 + 5 - 1
