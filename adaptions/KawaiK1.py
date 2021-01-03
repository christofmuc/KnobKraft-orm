#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

adaptation = {
    "Manufacturer Name": "Kawai",
    "Manufacturer MIDI ID": 0x40,
    "Model": "K1",
    "Device ID": (2, 1, 16),  # sysex offset, min value, max value. this is the sysex device ID, not the K1 ID
    "Default Timeout": 200,  # milliseconds to wait during Scan
    "Default Send Pause": 80,  # milliseconds to wait during sending multiple messages
    "Scan with Universal Device Inquiry": False,
    "Scan request": [0xf0, 0x40, 0x00, 0b01100000, 0xf7],
    "Scan reply": [0xf0, 0x40, 0x00, 0b01100001, 0x00, 0x03, 0xf7],
    "Data Types": {"Single": {"Size": 87, "Name Size": 10, "Name Offset": 0, "Name format": "Ascii"}},
    "Bank Drivers": [{"Bank Name": "Int-Singles I/1", "Data Type": "Single", "# of Entries": 32,
                      "Transmission Format": "7bit", "Checksum Type": "Kawai K1/K4",
                      "Memory Bank": True,
                      "Offsets": (0, 0, 0),  # These are the program offsets for single, bank, program change
                      "Single Request": [0xf0, 0x40, 0x00, 0x00, 0x00, 0x03, 0x00, "EN#", 0xf7],
                      "Single Reply": [0xf0, 0x40, 0x00, 0x20, 0x00, 0x03, 0x00, "EN#", "SUM", "SIN", "CHK", 0xf7],
                      "Bank Request": [0xf0, 0x40, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0xf7],
                      "Bank Reply": [0xf0, 0x40, 0x00, 0x21, 0x00, 0x03, 0x00, 0x00, "[", "SUM", "SIN", "CHK", "]",
                                     0xf7]},
                     {"Bank Name": "Int-Singles i/2", "Data Type": "Single", "# of Entries": 32,
                      "Transmission Format": "7bit", "Checksum Type": "Kawai K1/K4",
                      "Memory Bank": True,
                      "Offsets": (32, 32, 32),  # These are the program offsets for single, bank, program change
                      "Single Request": [0xf0, 0x40, 0x00, 0x00, 0x00, 0x03, 0x00, "EN#", 0xf7],
                      "Single Reply": [0xf0, 0x40, 0x00, 0x20, 0x00, 0x03, 0x00, "EN#", "SUM", "SIN", "CHK", 0xf7],
                      "Bank Request": [0xf0, 0x40, 0x00, 0x01, 0x00, 0x03, 0x20, 0x00, 0xf7],
                      "Bank Reply": [0xf0, 0x40, 0x00, 0x21, 0x00, 0x03, 0x20, 0x00, "[", "SUM", "SIN", "CHK", "]",
                                     0xf7]}
                     ],
}


#####################################################################################################################
#
# No user serviceable parts below this line

def name():
    return adaptation["Manufacturer Name"] + " " + adaptation["Model"]


def createDeviceDetectMessage(channel):
    if adaptation["Scan with Universal Device Inquiry"]:
        raise Exception("Universal Device Inquiry not implemented yet")
    else:
        return insertDeviceID(channel, adaptation["Scan request"])


def needsChannelSpecificDetection():
    return True


def channelIfValidDeviceResponse(message):
    if ignoreDeviceID(message) == adaptation["Scan reply"]:
        return message[2] & 0x0f  # I can't believe device ID is really 1-based?
    return -1


def numberOfBanks():
    # First bank is internal, the second bank is the cartridge if present
    return len(adaptation["Bank Drivers"])


def numberOfPatchesPerBank():
    # Ouch, that is not necessarily the same for all banks
    bank0 = adaptation["Bank Drivers"][0]
    return bank0["# of Entries"]


def createProgramDumpRequest(channel, program_no):
    bank = bankNoForProgramNo(program_no)
    request = adaptation["Bank Drivers"][bank]["Single Request"]
    result = []
    for c in request:
        if type(c) is str:
            if c == "EN#":
                result.append(program_no - adaptation["Bank Drivers"][bank]["Offsets"][0])
            else:
                pass  # For now, ignore other pseude-bytes
        else:
            result.append(c)
    return insertDeviceID(channel, result)


def isSingleProgramDump(message):
    for b in range(numberOfBanks()):
        worked, program, data = parseMessage(b, message)
        if worked:
            return True
    return False


def convertToProgramDump(channel, data, program_no):
    raise Exception("Not implemented yet")


def nameFromDump(message):
    for bank in range(numberOfBanks()):
        worked, program, data = parseMessage(bank, message)
        if worked:
            name = []
            for i in range(adaptation["Data Types"]["Single"]["Name Size"]):
                name.append(data[adaptation["Data Types"]["Single"]["Name Offset"] + i])
            # Ignoring character set conversion for now
            return "".join([chr(c) for c in name])
    raise Exception("Not implemented yet")


def numberFromDump(message):
    for b in range(numberOfBanks()):
        worked, program, data = parseMessage(b, message)
        if worked:
            return program
    raise Exception("Only single program dumps have program numbers")


def friendlyBankName(bank):
    return adaptation["Bank Drivers"][bank]["Bank Name"]


def parseMessage(bank, message):
    reply = ignoreDeviceID(message)
    reply_expected = adaptation["Bank Drivers"][bank]["Single Reply"]
    data = []
    data_block = False
    reply_ptr = 0
    expected_ptr = 0
    program_no = -1
    while reply_ptr < len(reply):
        if type(reply_expected[expected_ptr]) is str:
            if data_block:
                # We're in the status of reading the data block!
                while len(data) < adaptation["Data Types"]["Single"]["Size"]:
                    data.append(reply[reply_ptr])
                    reply_ptr = reply_ptr + 1
                data_block = False
                expected_ptr = expected_ptr + 1
            elif reply_expected[expected_ptr] == "SUM":
                summation_start = reply_ptr
                expected_ptr = expected_ptr + 1
            elif reply_expected[expected_ptr] == "EN#":
                program_no = reply[reply_ptr]
                expected_ptr = expected_ptr + 1
                reply_ptr = reply_ptr + 1
            elif reply_expected[expected_ptr] == "SIN":
                data_block = True
            elif reply_expected[expected_ptr] == "CHK":
                # Ignore checksum for now
                expected_ptr = expected_ptr + 1
                reply_ptr = reply_ptr + 1
            else:
                raise Exception("Unknown pseudo byte" + reply_expected[expected_ptr])
        else:
            # Just check that all bytes are as expected
            if reply[reply_ptr] != reply_expected[expected_ptr]:
                return False, -1, []
            reply_ptr = reply_ptr + 1
            expected_ptr = expected_ptr + 1
    return True, program_no, data


def bankNoForProgramNo(program_number):
    bank = 0
    count = 0
    while adaptation["Bank Drivers"][bank]["# of Entries"] + count < program_number + 1:
        count = count + adaptation["Bank Drivers"][bank]["# of Entries"]
        bank = bank + 1
    return bank


def insertDeviceID(channel, message):
    # TODO not sure what to do with the device ID min and max values. Is this for display?
    device_id = (channel + adaptation["Device ID"][1]) % adaptation["Device ID"][2]
    return message[0:adaptation["Device ID"][0]] + [channel] + message[adaptation["Device ID"][0] + 1:]


def ignoreDeviceID(message):
    return message[0:adaptation["Device ID"][0]] + [0] + message[adaptation["Device ID"][0] + 1:]


import binascii


def runTests():
    print("Adaption for", name())
    print("Autodetection message", createDeviceDetectMessage(0))
    for channel in range(16):
        reply = [0xf0, 0x40, channel, 0x61, 0x00, 0x03, 0xf7]
        assert channelIfValidDeviceResponse(reply) == channel
    assert numberOfBanks() == 2
    assert numberOfPatchesPerBank() == 32
    assert friendlyBankName(0) == "Int-Singles I/1"
    assert friendlyBankName(1) == "Int-Singles i/2"
    assert createProgramDumpRequest(2, 31) == [0xf0, 0x40, 2, 0x00, 0x00, 0x03, 0x00, 31, 0xf7]
    assert bankNoForProgramNo(31) == 0
    assert bankNoForProgramNo(32) == 1

    test_single = "F040002000030000467265746C65737320313B2432323E02150010005F320032343237484848483D3C3D6F0E0E0A2A4E515164000000000C100E073C3B3C2A000000001A1616224D4D435E323232321D1E2B143B3D3E321F323236323232320BF7"
    test_single2 = "F040002000030001467265746C65737320325D0C32323E021D00321B323200323235363C4E403C57255A6E0E0F2F0E64646464000000000B09062D40251F4B000000001517111E4233613D323232323200212C323232321832170C3232323265F7"
    single_program = list(binascii.unhexlify(test_single))
    assert isSingleProgramDump(single_program)
    print(nameFromDump(single_program))


if __name__ == "__main__":
    runTests()
