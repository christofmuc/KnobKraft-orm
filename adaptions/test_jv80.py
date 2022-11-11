from Roland_JV80 import *
import knobkraft


def test_program_position():
    xv_90_edit_buffer = "F0 41 10 46 12 00 08 20 00 43 72 79 73 74 61 6C 20 56 6F 78 20 00 04 7F 55 00 01 7F 7D 0B 63 01 08 6A 40 3E 02 00 00 00 01 00 32 10 F7" \
                        "F0 41 10 46 12 00 08 28 00 00 04 0F 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 01 58 00 40 04 46 00 40 00 40 00 40 04 46 01 02 01 5A 01 00 00 15 42 40 42 05 02 01 3C 00 00 00 3C 45 7F 44 4C 40 00 06 40 07 07 07 4C 00 47 5B 3C 66 32 4A 40 01 50 00 00 05 00 4C 07 07 07 7F 00 7F 62 56 67 50 14 7F 68 07 08 00 07 00 00 00 00 60 04 07 07 2A 7F 4D 75 5F 78 50 00 7F 7F 55 F7" \
                        "F0 41 10 46 12 00 08 29 00 00 02 0B 01 00 00 00 7F 01 01 05 44 00 40 00 40 00 40 05 41 00 40 03 7F 04 4D 00 40 00 40 00 40 04 46 01 02 01 5C 01 00 00 15 47 56 4C 05 02 01 47 00 00 00 5A 4A 7B 4D 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 4F 00 00 05 00 60 07 07 07 7F 00 7F 2B 56 43 19 14 7F 6A 07 00 00 07 00 00 0E 00 60 04 07 07 34 68 36 66 4A 6C 50 03 7F 7F 36 F7" \
                        "F0 41 10 46 12 00 08 2A 00 00 02 09 01 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 6E 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 07 0F 07 00 00 00 00 60 07 07 07 2A 6A 13 79 1A 7F 50 4A 7F 5A 11 F7" \
                        "F0 41 10 46 12 00 08 2B 00 00 02 0E 00 00 00 00 7F 01 01 00 40 00 40 00 40 00 40 00 40 00 40 03 7F 00 40 00 40 00 40 00 40 04 46 00 02 01 3C 00 00 00 00 40 40 40 05 02 00 3C 00 00 00 00 40 40 40 34 40 00 0C 40 07 07 07 40 00 40 00 40 00 40 00 40 01 7F 00 00 05 00 40 07 07 07 40 00 00 00 00 00 00 00 00 7F 07 04 00 07 00 00 00 00 60 07 07 07 36 6A 13 79 1A 7F 50 4A 7F 7F 5C F7"
    test_data = knobkraft.stringToSyx(xv_90_edit_buffer)
    program_dump1 = jv_80.convertToProgramDump(0x00, test_data, 0x22)
    program_dump2 = jv_80.convertToProgramDump(0x00, test_data, 0x33)

    # Test Blank out algorithm
    model_id_length = 1
    num_address_bytes = 4
    data_block_overhead = 3 + model_id_length + 1 + 2 + num_address_bytes
    program_position = 6

    blank_out_zones = [(jv_80.program_dump._start_index_of_block(x, data_block_overhead) + program_position, 1) for x in
                       range(len(jv_80.program_dump.data_blocks))]
    knobkraft.list_compare(program_dump1, program_dump2)
    # Check that the start of the block is correct
    for i in range(len(jv_80.program_dump.data_blocks)):
        start = jv_80.program_dump._start_index_of_block(i, data_block_overhead)
        assert program_dump1[start] == 0xf0

    # Check the end calculations are correct
    for i in range(len(jv_80.program_dump.data_blocks)):
        end = jv_80.program_dump._end_index_of_block(i, data_block_overhead)
        start = jv_80.program_dump._start_index_of_block(i, data_block_overhead)
        assert program_dump1[end] == 0xf7
        assert start + jv_80.program_dump.data_blocks[i].size + data_block_overhead == end + 1

    # Check the blank out zones work
    for i in range(len(jv_80.program_dump.data_blocks)):
        zone = blank_out_zones[i]
        assert program_dump1[zone[0]] == 0x22 + 0x40
        assert program_dump2[zone[0]] == 0x33 + 0x40

