#
#   Copyright (c) 2023 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import pytest

from Roland_MKS70V4 import *

bulk_message_length = 66
bulk_0x37_message_length = 48
bulk_tone_message_length = 88
apr_message_length = 61
apr_0x37_message_length = 61
apr_tone_message_length = 109


def test_saveParametersDoesntCrash():
    # This is not really meaningful test, but I used it to reproduce some errors I forgot by now
    result = [0] * bulk_message_length
    save_parameter(result, bulk_mapping_patch["BendRange"], 1)
    result = [0] * bulk_message_length
    save_parameter(result, bulk_mapping_patch["BendRange"], 3)
    result = [0] * apr_message_length
    save_parameter(result, apr_mapping_patch["Keymode"], 2)


testdata1 = [
    (bulk_message_length, bulk_mapping_patch),
    (apr_message_length, apr_mapping_patch),
    (bulk_tone_message_length, bulk_mapping_tone),
    (apr_tone_message_length, apr_mapping_tone),
    (bulk_0x37_message_length, bulk_mapping_patch_0x37),
    (apr_0x37_message_length, apr_mapping_patch_0x37)
]


@pytest.mark.parametrize("message_length, mapping_1", testdata1)
def test_load_and_save(message_length, mapping_1):
    for i in range(message_length):  # Iterate over all bytes in the message
        for j in range(7):  # Iterate over all bits in a byte
            if not is_parameter_bit(i, j, mapping_1):
                continue
            msg = [0] * message_length
            msg[i] = 1 << j

            # Load all parameters
            values = {}
            for key in mapping_1.keys():
                values[key] = load_parameter(msg, mapping_1[key])

            # Build a new message
            dst = [0] * message_length
            for key in mapping_1.keys():
                save_parameter(dst, mapping_1[key], values[key])

            if msg != dst:
                assert msg == dst


testdata2 = [
    (apr_message_length, bulk_message_length, apr_mapping_patch, bulk_mapping_patch),
    (bulk_message_length, apr_message_length, bulk_mapping_patch, apr_mapping_patch),
    (apr_tone_message_length, bulk_tone_message_length, apr_mapping_tone, bulk_mapping_tone),
    (bulk_tone_message_length, apr_tone_message_length, bulk_mapping_tone, apr_mapping_tone),
    (apr_0x37_message_length, bulk_0x37_message_length, apr_mapping_patch_0x37, bulk_mapping_patch_0x37),
    (bulk_0x37_message_length, apr_0x37_message_length, bulk_mapping_patch_0x37, apr_mapping_patch_0x37),
]


@pytest.mark.parametrize("src_message_length, dst_message_length, mapping_1, mapping_2", testdata2)
def test_conversion(src_message_length, dst_message_length, mapping_1, mapping_2):
    message_length = src_message_length
    for i in range(message_length):  # Iterate over all bytes in the message
        for j in range(7):  # Iterate over all bits in a byte
            if not is_parameter_bit(i, j, mapping_1):
                continue

            # Initialize an all-zero APR message
            src_msg = [0] * src_message_length

            # Set a single bit in the APR message
            src_msg[i] = 1 << j

            # Load all parameters
            values = {}
            for key in mapping_1.keys():
                values[key] = load_parameter(src_msg, mapping_1[key])

            # Convert to BULK and back
            dst_msg = convert_message(src_msg, dst_message_length, mapping_1, mapping_2)
            src_msg_back = convert_message(dst_msg, src_message_length, mapping_2, mapping_1)

            if src_msg != src_msg_back:
                # Check that the original message is recovered
                dst_msg = convert_message(src_msg, dst_message_length, mapping_1, mapping_2)
                src_msg_back = convert_message(dst_msg, src_message_length, mapping_2, mapping_1)
                assert src_msg == src_msg_back, f"Conversion failed for byte {i}, bit {j}"

    print("All conversions succeeded")


def test_single_tone_apr_0x35():
    single_apr = knobkraft.sysex.stringToSyx(
        "F0 41 35 00 24 20 01 20 4B 41 4C 49 4D 42 41 20 20 20 40 20 00 00 00 60 40 60 5D 39 00 00 7F 7F 7F 00 60 56 00 7F 40 60 00 37 00 00 2B 51 20 60 77 20 00 40 00 3E 00 0E 26 4A 20 12 31 00 30 20 7F 40 F7")
    assert isAprMessage(single_apr)
    assert isToneAprMessage(single_apr)
    assert nameFromDump(single_apr) == " KALIMBA  "
    assert not isEditBufferDump2(single_apr)


def test_single_tone_apr_0x38():
    single_apr = knobkraft.sysex.stringToSyx(
        " f0 41 38 00 24 20 01 50 41 44 20 31 20 20 20 20 20 48 05 00 00 00 00 00 00 34 4b 50 31 2e 28 63 63 27 35 19 25 22 15 2b 63 00 0c 63 00 11 63 00 35 5f 37 2a 43 5e 00 7f 00 7f 00 7f 3d 00 34 00 41 51 60 49 30 00 00 20 40 20 40 20 40 00 00 40 40 40 20 00 00 20 20 30 00 00 10 10 00 30 30 00 20 40 20 40 00 00 00 10 00 00 00 00 f7"
    )
    assert isAprMessage(single_apr)
    assert isToneAprMessage(single_apr)
    assert nameFromDump(single_apr) == "PAD 1     "
    assert not isEditBufferDump2(single_apr)


def test_old_bank_format():
    old_bank = knobkraft.load_sysex('testData/Roland_MKS70/MKS70_MKSSYNTH.SYX', False)
    patches = extractPatchesFromAllBankMessages(old_bank)
    assert len(patches) > 0


def test_MKS70V4():
    # Those are V3 bank dumps, and CANNOT be loaded by this adaptation which only supports the Vecoven firmware. We need to
    # reverse engineer the bank dump format to be able to convert them into APR messages first!
    #assert channelIfValidDeviceResponse(single_apr) == 0x00
    bank_dump = knobkraft.load_sysex('testData/Roland_MKS70/RolandMKS70_GENLIB-A.SYX')
    patches = []
    converted = extractPatchesFromAllBankMessages(bank_dump)
    if len(converted) > 0:
        # That worked
        for patch in converted:
            patches.append(patch)
    assert len(patches) > 0

    for patch in patches:
        assert isSingleProgramDump(patch)

    # mks_message = knobkraft.sysex.stringToSyx("f0 41 3a 00 24 30 01 00 00 53 59 4e 54 48 20 42 00 41 53 53 20 2f 20 50 00 41 44 20 20 3f 54 27 00 26 00 4a 13 3d 00 0b 41 00 4e 1f 00 41 0c 43 00 1b 01 4b 0c 10 30 00 03 00 7e 7f 27 04 00 20 40 f7")
    # assert isPartOfBankDump(mks_message)

    patch_prg = knobkraft.sysex.stringToSyx("f0 41 34 00 24 30 01 64 00 f7")
    patch_apr = knobkraft.sysex.stringToSyx(
        "f0 41 38 00 24 30 01 20 20 43 41 54 48 45 44 52 41 4c 20 4f 52 47 41 4e 20 50 47 27 26 00 60 00 78 00 00 2e 3c 74 00 4e 01 18 00 01 00 3c 0c 00 4d 01 26 00 01 00 5a 01 1a 00 00 01 f7")
    utone_prg = knobkraft.sysex.stringToSyx("f0 41 34 00 24 20 01 3c 00 f7")
    utone_apr = knobkraft.sysex.stringToSyx(
        "f0 41 38 00 24 20 01 50 49 50 45 20 4f 52 47 41 4e 20 3a 00 00 3a 45 00 00 40 00 00 40 00 00 5b 7f 00 4e 00 00 00 00 3e 50 00 66 00 00 00 00 00 00 00 00 00 00 00 00 7f 00 7f 00 7f 00 2d 00 00 40 2b 67 6c 34 00 00 00 60 40 60 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 60 60 00 50 70 00 70 40 20 00 00 00 00 00 00 10 00 00 f7")
    ltone_pgr = knobkraft.sysex.stringToSyx("f0 41 34 00 24 20 02 3c 00 f7")
    ltone_apr = knobkraft.sysex.stringToSyx(
        "f0 41 38 00 24 20 02 50 49 50 45 20 4f 52 47 41 4e 20 3a 00 00 3a 45 00 00 40 00 00 40 00 00 5b 7f 00 4e 00 00 00 00 3e 50 00 66 00 00 00 00 00 00 00 00 00 00 00 00 7f 00 7f 00 7f 00 2d 00 00 40 2b 67 6c 34 00 00 00 60 40 60 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 60 60 00 50 70 00 70 40 20 00 00 00 00 00 00 10 00 00 f7")
    all_messages = patch_prg + patch_apr + utone_prg + utone_apr + ltone_pgr + ltone_apr
    assert isSingleProgramDump(all_messages)
#    print(nameFromDump(all_messages))

    bank_dump = knobkraft.load_sysex('testData/Roland_MKS70/MKS-70_full_internal_bank_manual_dump.syx')
    patches = []
    assert isBankDumpFinished(bank_dump)
    converted = extractPatchesFromAllBankMessages(bank_dump)
    if len(converted) > 0:
        # That worked
        count = 0
        for patch in converted:
            assert numberFromDump(patch) == count
            assert calculateFingerprint(patch) != ""
            patches.append(patch)
            edit_buffer = convertToEditBuffer(0, patch)
            assert isEditBufferDump2(edit_buffer)
            assert nameFromDump(edit_buffer) == nameFromDump(patch)
            assert convertToProgramDump(0, edit_buffer, count) == patch
            count += 1
    assert len(patches) == 64
