#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# The Roland D-50 implements the Roland Exclusive Format Type IV, and thus is a good Roland example

roland_id = 0x41  # Roland
model_id = 0b00010100  # D-50
command_rq1 = 0x11
command_dt1 = 0x12

# Construct the Roland character set as specified in the MIDI implementation
character_set = [' '] + [chr(x) for x in range(ord('A'), ord('Z') + 1)] + \
                [chr(x) for x in range(ord('a'), ord('z') + 1)] + \
                [chr(x) for x in range(ord('1'), ord('9') + 1)] + ['0', '-']


def name():
    return "Roland D-50"


def createDeviceDetectMessage(channel):
    # Just request the upper common block from the edit buffer - if we get a reply, there is a D-50
    # Command is RQ1 = 0x11, the address is 0 1 0, the size is 0x40 for one block
    return buildRolandMessage(channel, command_rq1, [0x00, 0x01, 0x00], [0x00, 0x00, 0x40])


def channelIfValidDeviceResponse(message):
    # We are expecting a DT1 message response (command 0x12)
    if isOwnSysex(message):
        command, address, data = parseRolandMessage(message)
        if command == command_dt1 and address == [0x00, 0x01, 0x00]:
            return message[2] & 0x0f
    return -1


def nameFromDump(message):
    patch_parts = splitSysex(message)
    command, address, data = parseRolandMessage(patch_parts[6])
    if command == command_dt1 and address == [0x00, 0x03, 0x00]:
        return "".join([character_set[x] for x in data[:18]])
    raise Exception("Error in Roland D-50 data structure - did not find Patch data block")


def loadD50BankDump(messages):
    # The Bank dumps of the D-50 basically are just a lists of messages with the whole memory content of the synth
    # We need to put them together, and then can read the individual data items from the RAM
    synth_ram = [0xff] * (address_to_index([0x04, 0x0c, 0x08]) + 376)
    for message in messages:
        command, address, data = parseRolandMessage(message)
        if command == command_dt1:
            memory_base_index = address_to_index(address)
            synth_ram[memory_base_index:memory_base_index + len(data)] = data

    # Now pull 64 patches out of the RAM and create individual edit buffer messages
    result = []
    for p in range(64):
        patch = []
        patch_base = address_to_index([0x2, 0x00, 0x00]) + p * (7 * 0x40)
        for subsection in range(7):
            target_base = subsection * 0x40
            source_base = patch_base + target_base
            patch = patch + buildRolandMessage(0, command_dt1,
                                               index_to_address(target_base),
                                               synth_ram[source_base:source_base + 0x40])
        result.append(patch)
    return result


def isOwnSysex(message):
    return len(message) > 3 and message[0] == 0xf0 and message[1] == roland_id and message[3] == model_id


def buildRolandMessage(channel, command_id, address, data):
    message = [0xf0, roland_id, channel & 0x0f, model_id, command_id] + address + data + [0, 0xf7]
    message[-2] = roland_checksum(message[5:-2])
    return message


def parseRolandMessage(message):
    if isOwnSysex(message):
        checksum = roland_checksum(message[5:-2])
        if checksum == message[-2]:
            command = message[4]
            address = message[5:8]
            return command, address, message[8:-2]
        raise Exception("Checksum error in Roland message parsing, expected", message[-1], "but got", checksum)
    return None, None, None


def roland_checksum(data_block):
    return sum([-x for x in data_block]) & 0x7f


def address_to_index(address):
    return address[2] + (address[1] << 7) + (address[0] << 14)


def index_to_address(index):
    return [index >> 14, (index & 0x3f80) >> 7, index & 0x7f]


def splitSysex(byte_list):
    result = []
    index = 0
    while index < len(byte_list):
        sysex = []
        if byte_list[index] == 0xf0:
            # Sysex start
            while byte_list[index] != 0xf7 and index < len(byte_list):
                sysex.append(byte_list[index])
                index += 1
            sysex.append(0xf7)
            index += 1
            result.append(sysex)
        else:
            print("Skipping invalid byte", byte_list[index])
            index += 1
    return result


if __name__ == "__main__":
    detectMessage = createDeviceDetectMessage(0x7)
    g_command, g_address, g_data = parseRolandMessage(detectMessage)
    assert (g_command == command_rq1)
    assert (g_address == [0x00, 0x01, 0x00])
    assert (g_data == [0x00, 0x02, 0x0f])

    with open(R"D:\Christof\Music\RolandD50\BB1_SYX\BobbyBluz_1.syx", "rb") as bankDump:
        g_sysex = bankDump.read()
        sysex_messages = splitSysex(g_sysex)
        patches = loadD50BankDump(sysex_messages)
        for g_p in patches:
            print("Found patch", nameFromDump(g_p))
