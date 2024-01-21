from BC_Kijimi import *


def test_BC_Kijimi():
    deviceDetect = "f00215010306f7"
    deviceDetectReply = list(binascii.unhexlify(deviceDetect))
    assert channelIfValidDeviceResponse(deviceDetectReply) == 1
    patch_from_device = "f00200000000007e3f3d3d393d3c3d3c003d3d655a3b1f00005100001b567e3e7c7e3d0a6b5273004d3e3c00422b6e0060586b007e00000001010000000003000000000004000000000001010100050008040000060a010000000100640a00000000000000000101010100010101010101010101020101010101017f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f021804007f7f7f7f140e040002180400f7"
    patch_from_device_message = list(binascii.unhexlify(patch_from_device))
    assert isSingleProgramDump(patch_from_device_message)

    different_position = convertToProgramDump(0, patch_from_device_message, 666)
    assert isSingleProgramDump(different_position)
    assert numberFromDump(different_position) == 666
    assert nameFromDump(different_position) == "Kijimi 5-027"
    assert isDefaultName(nameFromDump(different_position))

    assert calculateFingerprint(different_position) == calculateFingerprint(patch_from_device_message)

    # Take a patch from the factory patches sysex
    patch_data = "f00200027f2e7e003e5d3d3a3c243c3c613d3d7e513b273b740e5f7e000000636f003b233c3d6c733c3e3d0057645b007e3b7e007e02030301010000000003000000020004000000000000010100010008040000090a010000000000640a00000000000000000101010101010201010101010101020101010101017f7f7f7f7f7f7f7f7f7ff7"
    patch = list(binascii.unhexlify(patch_data))
    assert isSingleProgramDump(patch)
    assert isDefaultName(nameFromDump(patch))
    assert numberFromDump(patch) == 127 + 2 * numberOfPatchesPerBank()

    assert friendlyBankName(0) == "User Bank 1"
    assert friendlyBankName(10) == "Factory Bank RD"

    edit_buffer = convertToEditBuffer(0, patch_from_device_message)
    assert isEditBufferDump(edit_buffer)
    assert nameFromDump(edit_buffer) == "Kijimi tmp"
    assert numberFromDump(edit_buffer) == 0
    program_back = convertToProgramDump(0, edit_buffer, 1)
    assert isSingleProgramDump(program_back)
    assert numberFromDump(program_back) == 1
