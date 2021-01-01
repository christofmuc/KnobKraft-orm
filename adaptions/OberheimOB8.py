#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re

has_encore = False
oberheim_id = [0x10, 0x01]
encore_id = [0x00, 0x00, 0x2f, 0x04]
default_name_detector = re.compile("OB-8(:| \\(E\\):) [A-D]*[1-8]")


def name():
    return "Oberheim OB-8"


def createDeviceDetectMessage(channel):
    # The only message the OB-8 replies to is a program dump request...
    # As we do not know if it has factory MIDI or Encore MIDI, just send both requests and hope for an answer
    return createProgramDumpRequestOberheim(channel, 0) + createProgramDumpRequestEncore(channel, 0)


def needsChannelSpecificDetection():
    return False


def deviceDetectWaitMilliseconds():
    return 500


def channelIfValidDeviceResponse(message):
    # We asked for program 0, see if that is what we got
    global has_encore
    if isSingleProgramDump(message):
        has_encore = isEncore(message)
        return 1
    return -1


def numberOfBanks():
    return 15


def numberOfPatchesPerBank():
    return 8


def createProgramDumpRequest(channel, patchNo):
    return [0xf0] + makeCorrectHeader() + [0x00, patchNo % numberOfPatchesPerBank(), 0xf7]


def createProgramDumpRequestOberheim(channel, patchNo):
    return [0xf0] + oberheim_id + [0x00, patchNo % numberOfPatchesPerBank(), 0xf7]


def createProgramDumpRequestEncore(channel, patchNo):
    return [0xf0] + encore_id + [0x00, patchNo % numberOfPatchesPerBank(), 0xf7]


def isSingleProgramDump(message):
    return len(message) > 3 and message[0] == 0xf0 and (
            (isOberheim(message) and message[3] == 0x01) or (isEncore(message) and message[5] == 0x01))


def convertToProgramDump(channel, message, patchNo):
    if isSingleProgramDump(message):
        return [0xf0] + makeCorrectHeader() + [0x01, patchNo % numberOfPatchesPerBank()] + getData(message) + [0xf7]
    raise Exception("Only program dumps can be converted")


def numberFromDump(message):
    if isSingleProgramDump(message):
        if isOberheim(message):
            return message[4]
        elif isEncore(message):
            return message[6]
    raise Exception("Can't extract program number from message")


def nameFromDump(message):
    if isSingleProgramDump(message):
        program = numberFromDump(message)
        if isOberheim(message):
            return "OB-8: %s" % friendlyProgramName(program)
        elif isEncore(message):
            return "OB-8 (E): %s" % friendlyProgramName(program)
    return "invalid name"


def friendlyBankName(bank_number):
    # This is just too good to not implement it. The OB-8 has 4 bank buttons, which can be used to combine! the bank
    # selected. So basically you are punching in 4 bits. And it is one based, so you have 15 banks ("groups" here)
    bank = bank_number + 1
    return ("A" if (bank & 1 != 0) else "") + \
           ("B" if (bank & 2 != 0) else "") + \
           ("C" if (bank & 4 != 0) else "") + \
           ("D" if (bank & 8 != 0) else "")


def friendlyProgramName(program):
    return "%s%d" % (friendlyBankName(program // 8), (program % 8) + 1)


def isDefaultName(patchName):
    # This regex ideally matches exactly the default names we produce with the above method nameFromDump()
    return default_name_detector.match(patchName) is not None


def calculateFingerprint(message):
    if isSingleProgramDump(message):
        return hashlib.md5(bytearray(getData(message))).hexdigest()
    raise Exception("Only program dumps can be converted")


def makeCorrectHeader():
    return encore_id if has_encore else oberheim_id


def isOberheim(message):
    return message[1:3] == oberheim_id


def isEncore(message):
    return message[1:5] == encore_id


def getData(message):
    if isSingleProgramDump(message):
        if isOberheim(message):
            return message[5:-1]
        elif isEncore(message):
            return message[7:-1]
    raise Exception("Non program dump given to getData, program error")


def denibble(message, start, stop):
    return [message[x] | (message[x + 1] << 4) for x in range(start, stop, 2)]


def nibble(message):
    result = []
    for b in message:
        result.append(b & 0x0f)
        result.append((b & 0xf0) >> 4)
    return result


# Some tests not run by the Orm. You can run these in PyCharm by just clicking "Run", as there is no dependency
# on KnobKraft in this module
if __name__ == "__main__":
    fake_program_117 = [0xf0] + makeCorrectHeader() + [0x01, 117, 0xf7]
    assert isSingleProgramDump(fake_program_117)
    assert isOberheim(fake_program_117)
    assert nameFromDump(fake_program_117) == "OB-8: ABCD6"
    assert isDefaultName(nameFromDump(fake_program_117))
    has_encore = True
    fake_program_117 = [0xf0] + makeCorrectHeader() + [0x01, 117, 0xf7]
    assert isSingleProgramDump(fake_program_117)
    assert isEncore(fake_program_117)
    assert nameFromDump(fake_program_117) == "OB-8 (E): ABCD6"
    assert isDefaultName(nameFromDump(fake_program_117))
    assert friendlyBankName(14) == "ABCD"
