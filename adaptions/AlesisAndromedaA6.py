#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib


def name():
    return "Alesis Andromeda A6"


def createDeviceDetectMessage(channel):
    # The A6 replies to Universal Device Inquiry, p. 6 of the "Sysex specs" document
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 500


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    # Check for reply on Universal Device Inquiry
    if len(message) > 12 and message[:12] == [0xf0, 0x7e, 0x7f, 0x06, 0x02, 0x00, 0x00, 0x0e, 0x1d, 0x00, 0x00, 0x00]:
        # Just return any valid channel
        return 0x00
    return -1


def numberOfBanks():
    return 16


def numberOfPatchesPerBank():
    return 128


# Implementation for Edit Buffer commented out because there seems to be a bug that the edit buffer does indeed not work
# https://www.gearslutz.com/board/showpost.php?p=15188863&postcount=623
def createEditBufferRequest(channel):
    #    # Sysex documentation specifies an edit buffer exists
    #    # Edit buffer #16 (decimal) is called the program edit buffer
    #    # MS from https://electro-music.com/forum/viewtopic.php?t=59282
    #    # When you send a program edit buffer dump request (F0 00
    #    # 00 0E 1D 03 10 F7) to an Andromeda, you get in return a
    #    # program edit buffer dump (F0 00 00 0E 1D 02 10 <2341 bytes> F7).
    return [0xf0, 0x00, 0x00, 0x0e, 0x1d, 0x03, 0x10, 0xf7]


def isEditBufferDump(message):
    return len(message) > 6 and message[:7] == [0xf0, 0x00, 0x00, 0x0e, 0x1d, 0x02, 0x10]  # from chris on gh forum


def convertToEditBuffer(channel, message):  # from chris on gh
    bank = 0
    program = numberOfPatchesPerBank() - 1
    if isEditBufferDump(message):
        return [0xf0, 0x00, 0x00, 0x0e, 0x1d, 0x00, bank, program] + message[7:] + [0xc0 | (channel % 0x7f), program]
    if isSingleProgramDump(message):
        # We just need to adjust bank and program and position 6 and 7
        return message[:6] + [bank, program] + message[8:] + [0xc0 | (channel % 0x7f), program]
    raise Exception("Can only create program dumps from master keyboard dumps")


def createProgramDumpRequest(channel, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    return [0xF0, 0x00, 0x00, 0x0E, 0x1D, 0x01, bank, program, 0xf7]


def isSingleProgramDump(message):
    return len(message) > 5 and message[:6] == [0xF0, 0x00, 0x00, 0x0E, 0x1D, 0x00]


def convertToProgramDump(channel, message, program_number):
    bank = program_number // numberOfPatchesPerBank()
    program = program_number % numberOfPatchesPerBank()
    if isEditBufferDump(message):
        return [0xF0, 0x00, 0x00, 0x0E, 0x1D, 0x00, bank, program] + message[7:]
    if isSingleProgramDump(message):
        # We just need to adjust bank and program and position 6 and 7, should there be an extra program change skip it
        f7 = rindex(message, 0xf7)
        return message[:6] + [bank, program] + message[8:f7 + 1]
    raise Exception("Can only create program dumps from master keyboard dumps")


def nameFromDump(message):
    if isSingleProgramDump(message):
        data_block = unescapeSysex(message[8:-1])  # The data block starts at index 8, and does not include the 0xf7
        return ''.join([chr(x) for x in data_block[2:2 + 16]])
    if isEditBufferDump(message):
        data_block = unescapeSysex(message[7:-1])
        return ''.join([chr(x) for x in data_block[2:2 + 16]])
    raise Exception("Can only extract name from master keyboard program dump")


def numberFromDump(message):
    if isSingleProgramDump(message):
        bank = message[6]
        program = message[7]
        return bank * numberOfPatchesPerBank() + program
    if isEditBufferDump(message):
        return 0
    raise Exception("Can only extract number from single program dumps")


def renamePatch(message, new_name):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        data_block = unescapeSysex(getDataBlock(message))
        for i in range(16):
            if i < len(new_name):
                data_block[2 + i] = ord(new_name[i])
            else:
                data_block[2 + i] = ord(" ")
        if isSingleProgramDump(message):
            return message[:8] + escapeSysex(data_block) + [0xf7]
        elif isEditBufferDump(message):
            return message[:7] + escapeSysex(data_block) + [0xf7]
    raise Exception("Can only rename single program dumps!")


def createBankDumpRequest(channel, bank):
    # Page 4 of the sysex spec
    return [0xf0, 0x00, 0x00, 0x0e, 0x1d, 0x0a, bank, 0xf7]


def isPartOfBankDump(message):
    # A bank dump on the A6 consists of 128 single dumps
    return isSingleProgramDump(message)


def isBankDumpFinished(messages):
    count = 0
    for message in messages:
        if isPartOfBankDump(message):
            count = count + 1
    return count == numberOfPatchesPerBank()


def extractPatchesFromBank(message):
    if isSingleProgramDump(message):
        return message
    raise Exception("Only Single Program dumps are expected to be part of a bank dump")


def calculateFingerprint(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        data_block = unescapeSysex(getDataBlock(message))
        # Blank out name
        data_block[2:2 + 16] = [0] * 16
        return hashlib.md5(bytearray(data_block)).hexdigest()  # Calculate the fingerprint from the cleaned payload data
    # Don't know why we should come here, but to be safe, just hash all bytes
    return hashlib.md5(bytearray(message)).hexdigest()


def getDataBlock(message):
    if isSingleProgramDump(message):
        return message[8:rindex(message, 0xf7)]  # The data block starts at index 8, and does not include the 0xf7
    if isEditBufferDump(message):
        f7 = rindex(message, 0xf7)
        return message[7:f7]
    raise Exception("Only single programs and edit buffers have a data block that can be extracted!")


def friendlyBankName(bank):
    return ["User", "Preset1", "Preset2", "Card 1", "Card 2", "Card 3", "Card 4", "Card 5", "Card 6", "Card 7",
            "Card 8", "Card 9", "Card 10", "Card 11", "Card 12", "Card 13"][bank]


def friendlyProgramName(patchNo):
    bank = patchNo // numberOfPatchesPerBank()
    program = patchNo % numberOfPatchesPerBank()
    return friendlyBankName(bank) + " %03d" % program


def unescapeSysex(data):
    # The A6 uses the shift technique to store 8 bits in 7 bit bytes. This is some particularly ugly code I could only
    # get to work with the help of the tests
    result = []
    roll_over = 0
    i = 0
    while i < len(data) - 1:
        mask1 = (0xFF << roll_over) & 0x7F
        mask2 = 0xFF >> (7 - roll_over)
        result.append((data[i] & mask1) >> roll_over | (data[i + 1] & mask2) << (7 - roll_over))
        roll_over = (roll_over + 1) % 7
        i = i + 1
        if roll_over == 0:
            i = i + 1
    return result


def escapeSysex(data):
    result = []
    roll_over = 7
    previous = 0
    i = 0
    while i < len(data):
        mask1 = 0xFF >> (8 - roll_over)
        mask2 = 0xFF >> roll_over << roll_over
        if mask1 > 0:
            result.append(((data[i] & mask1) << (7 - roll_over)) | previous)
            previous = (data[i] & mask2) >> roll_over
            roll_over = roll_over - 1
            i = i + 1
        else:
            result.append(previous)
            previous = 0
            roll_over = 7
    result.append(previous)
    return result


def rindex(mylist, myvalue):
    return len(mylist) - mylist[::-1].index(myvalue) - 1


def bitsSet(byte):
    count = 0
    for i in range(8):
        if (byte >> i) & 0x01 == 0x01:
            count = count + 1
    return count


# Test data picked up by test_adaptation.py
def test_data():
    import knobkraft

    def programs(messages):
        single_program = "F000000E1D00000026150812172C1A3720020D23174D5D347472010172003C0000364014000400000001780105000040003800010000000000000000000000000000000000000000000038010E0000000000000000000000000000000000405240164336460C200040507C795F067C1F7F31400108000000000138010E6001000C000000002048030016000002041400007E7D3F102000000000000000000000000000080009030005000000000000000000000000605F7F010004080000000000000000000000007F7E07001020000000000000000000000000043844014002000000000000000000000000706F7F000002040000000000000000000000403F7F030008100000000000000000000000007E7D0F00204000000000000000000000000078773F000001020000000000000000000000605F7F010004080000000000000000000000007F7E070010200000000000000000000000007C7B1F00400001000000000000007A410000706F7F070002000000000000000018020800403F7F1F00080000000000000000501D3400007E7D6F00200000000000000000000000000078773F000001020000000000000022400100605F7F030004000000000000000060011800007F7E3F00100000000000000000203C7800007C7B7F214000000000000000000000000000706F7F00000204000000000000007C7B0F00403F7F0700081000000000000000400A3400007E7D4F00200000000000000000000000000078773F000001020000000000000000000000605F7F010004080000000000000000000000007F7E070010200000000000000000000000007C7B1F004000010000000000000000000000706F7F000102040000000000000000000000403F7F030008100000000000000000000000007E7D0F00204000000000000000000000000078773F000001020000000000000000000000605F7F100004000000000000000000000000007F7E430010000000000000000000000000007C7B1F004000000000000000000031004B07746F7F000002000000000000000060420A00403F7F030008000000000000000000000000007E7D0F0020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002000000100040010407F740100784F1F000000000000000000000100000010000000000200000000407F7801040000004000000000080000000001000000100000000002000000200000000000780F1F4000000000080000000001003C000000000000034414217448593408132A5D4A3933776F213818032615297768653439176E6E5A7F7B0F30404812440E194B067122552D193F761E1E040833645422754E5D4C18776A6D4D7B3F7F7F7D07002003000F000000000000000000000000000000000000000060016E0D010070011640200000000000200000000000000003200043020410100000000000000000000000000000000000000041125E011028621B000A442607000040000620070000000000020000000000001800021014200001010000000000000000000000000000000000000000000000000000005020343A0000000470003400000000001000000000000040011800240102080800000000000000000000000000000000000000000002001003400C60004002000000100000020028707F3F0000000000000000000000000000000000000000000000000000010002400F0A0000547C0600404002000A0C001020000142020408004001003C00000000000000000000000000000000000000002040007C06182A066A1430094A030000200000057E7F07000000000000000000000000000000000000000000000000500C140D3001000000406A6F000C14084000410102020410202840000100180040070000000000000000000000000000000000000000040000004026780118460E0000400000040040647F7F00000000000000000000000000000000000000000000000000000230041D0000000028790C4001010600142000204000022405081040010300780000000000000000000000000000000000001009000000000000706F3F000000301800000701030400000000000000000000000000000074030000000000003004000000000000000000000040006201000000000000000C000002030C0610003030011203600026600049013000183040670042053800184C004001245840594272041A6000386125020D30005F307300360A7224784720660752027C5D3032410170044013004E0038024009002600180160044012004A0040024009002600180160044013000000000000400811224408112244080000007810607F1700040800202027000000000000000000000000000000000000480100000000200000180060464000010000000020401700040800000000000000000000000000000000000000400E0000000000000100681D04000000000000000000600354183000000000000000000000400C007201000000000000000000000000000000000000000000607F7F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007E7F5702020000000000005141400100467D07004016000A002820010A000000000000400C407F50000000000040000001000030204003000008000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F7"
        yield {"message": knobkraft.stringToSyx(single_program), "name": "Brain Activity  ", "number": 0}

    return {"program_generator": programs, "convert_to_edit_buffer_produces_program_dump": True}
