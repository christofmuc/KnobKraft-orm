#
#   Copyright (c) 2026 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import sys
from pathlib import Path
from typing import Iterator

import sequential
import testing

this_module = sys.modules[__name__]
SAMPLE_FILE = Path(__file__).resolve().parent / "testData" / "Sequential_Fourm" / "a_new_legend.syx"


fourm = sequential.GenericSequential(
    name="Sequential Fourm",
    device_id=0x3B,
    banks=4,
    patches_per_bank=128,
    name_len=19,
    name_position=89,
)
fourm.install(this_module)


def make_test_data():
    def programs(data: testing.TestData) -> Iterator[testing.ProgramTestData]:
        yield testing.ProgramTestData(
            message=data.all_messages[0],
            name="A New Legend",
            number=0,
            rename_name="Fourm Rename Test",
        )

    def edit_buffers(data: testing.TestData) -> Iterator[testing.ProgramTestData]:
        yield testing.ProgramTestData(
            message=fourm.convertToEditBuffer(0, data.all_messages[0]),
            name="A New Legend",
            rename_name="Fourm Rename Test",
        )

    return testing.TestData(
        sysex=str(SAMPLE_FILE),
        program_generator=programs,
        edit_buffer_generator=edit_buffers,
        device_detect_call=[0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7],
        device_detect_reply=([0xF0, 0x7E, 0x00, 0x06, 0x02, 0x01, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7], 0),
        program_dump_request=(0, 0, [0xF0, 0x01, 0x3B, 0x05, 0x00, 0x00, 0xF7]),
        expected_patch_count=1,
    )
