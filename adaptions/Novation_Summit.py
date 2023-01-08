#   Copyright (c) 2022 Christof Ruch. All rights reserved.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#   with the help from https://gearspace.com/board/showpost.php?p=15306436&postcount=1566
#   and https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1149964-novation-peak-96.html
#   in turn based on Cedric Tessier's Novation Ultranova adaptatio
import hashlib

NAME_OFFSET = 0x10
NAME_LEN = 16
NAME_SPECIALCHARSET = ' !"#$%&\'()*+,-./:;<=>?@[]^_`{|}'
novation_id = [0x00, 0x20, 0x29]
summit_id = [0x01, 0x11, 0x01, 0x33]
peak_id = [0x01, 0x10, 0x00, 0x7e]


def name():
    return 'Novation Summit'


def createDeviceDetectMessage(channel):
    return [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 9
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e
            and message[2] == 0x7f
            and message[3] == 0x06
            and message[4] == 0x02
            and message[5:8] == novation_id
            and message[8] == 0x33  # This ought to be the Summit
            and message[9] == 0x01):
        # From that reply we cannot find out on which MIDI channel/Device ID the Summit is setup
        return 1
    return -1


def bankDescriptors():
    return [{"bank": x, "name": f"Bank {chr(ord('A')+x)}", "size": 128, "type": "Single Patch"} for x in range(4)]


def createEditBufferRequest(channel):
    return [0xf0] + novation_id + summit_id + [0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7]


def isEditBufferDump(message):
    return isOwnSysex(message) and len(message) > 8 and message[8] == 0x00  # Single patch to edit buffer


def isOwnSysex(message):
    return (len(message) > 8
            and message[0] == 0xf0
            and message[1:4] == novation_id
            and (message[4:8] == summit_id or message[4:8] == peak_id)
            )


def createProgramDumpRequest(channel, patchNo):
    bank = patchNo // 128
    program = patchNo % 128
    return [0xf0] + novation_id + summit_id + [0x41, 0x00, 0x00, 0x00, bank, program, 0xf7]


def isSingleProgramDump(message):
    return isOwnSysex(message) and len(message) > 8 and message[8] == 0x01  # Single patch send command


def numberFromDump(message):
    if not isSingleProgramDump(message):
        return 0
    bank = message[12] % 4  # Not more than 4 banks allowed
    program = message[13] % 128  # Maximum 128 programs per bank
    return bank * 128 + program


def nameFromDump(message):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        name = ''.join([chr(x) for x in message[0x10:0x10+16]])
        return name.strip()
    return 'Invalid'


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    elif isSingleProgramDump(message):
        return message[:8] + [0x00, 0x00, 0x00, 0x00, 0x00, 0x00] + message[0x0e:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def convertToProgramDump(channel, message, program_number):
    bank = program_number // 128
    program = program_number % 128
    if isEditBufferDump(message) or isSingleProgramDump(message):
        return message[:8] + [0x01, 0x00, 0x00, 0x00, bank, program] + message[0x0e:]
    raise Exception("Neither edit buffer nor program dump - can't be converted")


def renamePatch(message, new_name):
    if isSingleProgramDump(message) or isEditBufferDump(message):
        # name is alpha-num + special chars (replace everything else by '_' and pad with spaces)
        clean_name = new_name.strip()[:NAME_LEN].ljust(NAME_LEN, ' ')
        valid_name = [ord(x) if x.isalnum() or x in NAME_SPECIALCHARSET else ord('_') for x in clean_name]
        return message[:NAME_OFFSET] + valid_name + message[NAME_OFFSET+NAME_LEN:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def friendlyBankName(bank):
    return f"Bank {chr(ord('A')+(bank % 4))}"


def friendlyProgramName(program):
    return "%s%d" % (friendlyBankName(program // 128), program % 128)


def calculateFingerprint(message):
    # Only fingerprint data outside of the header, beyond the patch name
    data = message[NAME_OFFSET+NAME_LEN:-1]
    return hashlib.md5(bytearray(data)).hexdigest()


def test_data():
    def programs(messages):
        yield {"message": messages[0], "name": "Reflections", "number": 0, "is_edit_buffer": True}
    return {"sysex": "testData/NovationPeak-Reflections.syx", "program_generator": programs,
            "rename_name": "Cr4zy Name_$",
            "friendly_bank_name": (2, 'Bank C')}
