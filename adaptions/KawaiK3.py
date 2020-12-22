#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#


def name():
    return "Kawai K3 Adaptation"


def createDeviceDetectMessage(channel):
    # 96 = MACHINE ID REQUEST
    return [0xF0, 0x40, (channel & 0x0f), 96, 0xF7]


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if (len(message) > 5
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x40  # Kawai
            and (message[2] & 0xf0) == 0x00
            and message[3] == 97):  # MACHINE ID ACKNOWLEDGE
        return message[2] & 0x0f
    return -1


def numberOfBanks():
    # First bank is internal, the second bank is the cartridge if present
    return 2


def numberOfPatchesPerBank():
    return 50


def createProgramDumpRequest(channel, program_no):
    if not (0 <= program_no <= 101):
        raise Exception("Program_no must be in the range 0 to 101 but is %d" % program_no)
    return buildSysexFunction(channel, 0, program_no)  # 0 is the ONE BLOCK DATA REQUEST


def isSingleProgramDump(message):
    # 32 is the ONE BLOCK DATA DUMP function
    return isOwnSysex(message) and message[3] == 32 and 0 <= message[6] < 100


def convertToProgramDump(channel, data, program_no):
    if not(0 <= program_no < 100):
        raise Exception("Program_no must be in the range 0 to 99")
    if isSingleProgramDump(data):
        # Replace channel and program slot in the single program dump message
        return data[0:2] + [channel & 0x0f] + data[3:6] + [program_no] + data[7:]
    raise Exception("Can only convert singleProgramDumps")


def nameFromDump(message):
    if isSingleProgramDump(message):
        return "%s %02d" % ("Internal" if message[6] < 50 else "Cartridge", (message[6] % 50) + 1)
    raise Exception("Only single program dumps can be named")


def numberFromDump(message):
    if isSingleProgramDump(message):
        return message[6] + 1
    raise Exception("Only single program dumps have program numbers")


def friendlyBankName(bank):
    return "Internal" if bank == 0 else "Cartridge"


def isOwnSysex(data):
    return (data[0] == 0xf0  # Sysex
            and data[1] == 0x40  # Kawai
            and (data[2] & 0xf0) == 0x00  # 0 except channel
            and data[4] == 0  # Group No
            and data[5] == 1)  # K3 ID


def buildSysexFunction(channel, function, sub_command):
    return [0xf0, 0x40, (channel & 0x0f), function, 0x00, 0x01, sub_command, 0xf7]
