#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re
import binascii


bc_manufacturer_id = 0x02  # This is not really Black Corporation, but IDP, probably a lost entry in the ID list?
command_firmware_version = 0x15  # Seems to be both a request and an answer
command_request_patch = 0x13  # Specs say that this is both a request and the answer code
command_transfer_patch = 0x00  # Don't send a message 0x13 with patch data, but to store send this command?
raw_length_of_program = 268/2


def name():
    return "BC Kijimi"  # for the Black Corporation's Kijimi


def createDeviceDetectMessage(channel):
    # Ask for firmware version
    return [0xF0, bc_manufacturer_id, command_firmware_version, 0xF7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if len(message) > 2 and message[:3] == [0xF0, bc_manufacturer_id, command_firmware_version]:
        # message continues  ,MAJOR, MINOR, MINOR_MINOR, 0xF7]
        # Channel not contained in message, just return 1
        return 1
    return -1


def numberOfBanks():
    return 10


def numberOfPatchesPerBank():
    return 128


def createProgramDumpRequest(channel, program_no):
    bank = program_no % numberOfPatchesPerBank()
    program = program_no // numberOfPatchesPerBank()
    return [0xf0, bc_manufacturer_id, command_request_patch, bank, program, 0xf7]


def isSingleProgramDump(message):
    # The third 0x00 is guessed, because it would make it symmetric with the transfer patch command specified to send
    # But as we don't know, accept both until we have a MIDI log to confirm
    return len(message) == raw_length_of_program and (
            message[:3] == [0xf0, bc_manufacturer_id, command_request_patch] or
            message[:3] == [0xf0, bc_manufacturer_id, command_transfer_patch])


def convertToProgramDump(channel, message, program_no):
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    if isSingleProgramDump(message):
        # Make sure to send with the command_transfer_patch, and to the proper position
        # As the Kijimi doesn't have an edit buffer, append a program change
        patch_transfer_message = [0xf0, bc_manufacturer_id, command_request_patch, bank, program] + message[5:]
        assert len(patch_transfer_message) == raw_length_of_program
        return patch_transfer_message
    raise Exception("Can only convert singleProgramDumps")


def nameFromDump(message):
    if isSingleProgramDump(message):
        number = numberFromDump(message)
        return "Kijimi %d-%03d" % (number // numberOfPatchesPerBank(), (number % numberOfPatchesPerBank()) + 1)
    raise Exception("Only single program dumps can be named")


def isDefaultName(patchName):
    return re.match("Kijimi [0-9]-[0-9][0-9][0-9]$", patchName) is not None


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[3] * numberOfPatchesPerBank() + message[4]
    raise Exception("Only single program dumps have program numbers")


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        raw = message[5:-1]
        #assert len(raw) == raw_length_of_program
        return hashlib.md5(bytearray(raw)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint singleProgramDumps")


def run_tests():
    patch_data = "F0020002000B2200253E3C3A3C3C5D3C003D3D007E3B5E00001D496A1B2D7272287E422A703C3E0068433C2109231F000F6441007E00030001010100000001010000020004000000000000010100010008040000080A010000000100640A00000000000000000101020101010101010101010201010001010101017F7F7F7F7F7F7F7F7F7FF7"
    patch_data = "f0020002000b2200253e3c3a3c3c5d3c003d3d007e3b5e00001d496a1b2d7272287e422a703c3e0068433c2109231f000f6441007e00030001010100000001010000020004000000000000010100010008040000080a010000000100640a00000000000000000101020101010101010101010201010001010101017f7f7f7f7f7f7f7f7f7ff7"
    patch_data = "f00200027f2e7e003e5d3d3a3c243c3c613d3d7e513b273b740e5f7e000000636f003b233c3d6c733c3e3d0057645b007e3b7e007e02030301010000000003000000020004000000000000010100010008040000090a010000000000640a00000000000000000101010101010201010101010101020101010101017f7f7f7f7f7f7f7f7f7ff7"
    patch = list(binascii.unhexlify(patch_data))
    assert isSingleProgramDump(patch)
    assert isDefaultName(nameFromDump(patch))

    different_position = convertToProgramDump(0, patch, 666)
    assert isSingleProgramDump(different_position)
    assert numberFromDump(different_position) == 666
    assert nameFromDump(different_position) == "Kijimi 5-027"
    assert isDefaultName(nameFromDump(different_position))

    assert calculateFingerprint(different_position) == calculateFingerprint(patch)


if __name__ == "__main__":
    run_tests()
