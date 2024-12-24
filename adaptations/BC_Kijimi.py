#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re
import binascii
from typing import List

import testing

bc_manufacturer_id = 0x02  # This is not really Black Corporation, but IDP, probably a lost entry in the ID list?
command_firmware_version = 0x15  # Seems to be both a request and an answer
command_request_patch = 0x13  # Specs say that this is both a request and the answer code
command_get_state = 0x14  # That's the edit buffer of the Kijimi
command_transfer_patch = 0x00  # Don't send a message 0x13 with patch data, but to store send this command?
command_set_state = 0x23  # Send edit buffer
old_program_length = 268 / 2  # This is the length of the messages in the factory patches. Format changed?
hypothetical_old_state_length = 268 / 2 - 2   # Assuming old edit buffers are 2 bytes shorter than old programs?
new_program_length = 262  # Documented length of message, matches what we got from 1.3.6 firmware device
new_state_length = 260


def name():
    return "BC Kijimi"  # for the Black Corporation's Kijimi


def createDeviceDetectMessage(channel):
    # Ask for firmware version
    return [0xF0, bc_manufacturer_id, command_firmware_version, 0xF7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if len(message) == 7 and message[:3] == [0xF0, bc_manufacturer_id, command_firmware_version]:
        major, minor, patch = message[3:6]
        print("Found BC Kijimi with firmware version %d.%d.%d" % (major, minor, patch))
        # Channel not contained in message, just return 1
        return 1
    return -1


def numberOfBanks():
    return 11


def numberOfPatchesPerBank():
    return 128


def createEditBufferRequest(channel):
    return [0xf0, bc_manufacturer_id, command_get_state, 0xf7]


def isEditBufferDump(message):
    return len(message) in [new_state_length , hypothetical_old_state_length] and message[:3] == [0xf0, bc_manufacturer_id, command_set_state]


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    if isSingleProgramDump(message):
        return [0xf0, bc_manufacturer_id, command_set_state] + message[5:]
    raise Exception("can only convert edit buffer or program buffer")


def createProgramDumpRequest(channel, program_no):
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    return [0xf0, bc_manufacturer_id, command_request_patch, bank, program, 0xf7]


def isSingleProgramDump(message):
    return hasCorrectLength(message) and  message[:3] == [0xf0, bc_manufacturer_id, command_transfer_patch]


def convertToProgramDump(channel, message, program_no):
    bank = program_no // numberOfPatchesPerBank()
    program = program_no % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        # Make sure to send with the command_transfer_patch, and to the proper position
        patch_transfer_message = [0xf0, bc_manufacturer_id, command_transfer_patch, bank, program] + message[3:]
        assert hasCorrectLength(patch_transfer_message)
        return patch_transfer_message
    if isSingleProgramDump(message):
        # Make sure to send with the command_transfer_patch, and to the proper position
        patch_transfer_message = [0xf0, bc_manufacturer_id, command_transfer_patch, bank, program] + message[5:]
        assert hasCorrectLength(patch_transfer_message)
        return patch_transfer_message
    raise Exception("Can only convert singleProgramDumps")


def nameFromDump(message):
    if isEditBufferDump(message):
        return "Kijimi tmp"
    if isSingleProgramDump(message):
        number = numberFromDump(message)
        return "Kijimi %d-%03d" % (number // numberOfPatchesPerBank(), (number % numberOfPatchesPerBank()) + 1)
    raise Exception("Only single program dumps can be named")


def isDefaultName(patchName):
    return patchName == "Kijimi tmp" or re.match("Kijimi [0-9]-[0-9][0-9][0-9]$", patchName) is not None


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[3] * numberOfPatchesPerBank() + message[4]
    raise Exception("Only single program dumps have program numbers")


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        raw = message[5:-1]
        return hashlib.md5(bytearray(raw)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    raise Exception("Can only fingerprint singleProgramDumps")


def friendlyBankName(bank):
    return ["User Bank 1", "User Bank 2", "Factory Bank MJ",
            "User Bank 3", "User Bank 4", "User Bank 5", "User Bank 6", "User Bank 7", "User Bank 8", "User Bank 9",
            "Factory Bank RD"][bank]


def hasCorrectLength(message):
    # It seems we have two different formats flying around. I don't know if the device accepts older messages.
    # If it is not backward compatible, we'd need to convert old to new, but we don't know how.
    return len(message) == old_program_length or len(message) == new_program_length


def make_test_data():
    def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        # These are old (short) format data items
        patch_data = "F0020002000B2200253E3C3A3C3C5D3C003D3D007E3B5E00001D496A1B2D7272287E422A703C3E0068433C2109231F000F6441007E00030001010100000001010000020004000000000000010100010008040000080A010000000100640A00000000000000000101020101010101010101010201010001010101017F7F7F7F7F7F7F7F7F7FF7"
        patch = list(binascii.unhexlify(patch_data))
        assert isDefaultName(nameFromDump(patch))
        short_edit_buffer = convertToEditBuffer(0, patch)
        print(binascii.hexlify(bytes(short_edit_buffer), " "))
        yield testing.ProgramTestData(message=patch, name="Kijimi 2-001", number=256, change_number_changes_name=True)
        patch_data = "f0020002000b2200253e3c3a3c3c5d3c003d3d007e3b5e00001d496a1b2d7272287e422a703c3e0068433c2109231f000f6441007e00030001010100000001010000020004000000000000010100010008040000080a010000000100640a00000000000000000101020101010101010101010201010001010101017f7f7f7f7f7f7f7f7f7ff7"
        patch = list(binascii.unhexlify(patch_data))
        assert isDefaultName(nameFromDump(patch))
        yield testing.ProgramTestData(message=patch, name="Kijimi 2-001", number=256, change_number_changes_name=True)
        #yield testing.ProgramTestData(message=patch, name="Kijimi 2-128", number=383, change_number_changes_name=True)

    return testing.TestData(program_generator=programs)
