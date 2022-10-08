#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   John Bowen Solaris Adaptation version 0.1 by Stéphane Conversy, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#


# see https://owner.johnbowen.com/images//files/Solaris%20SysEx%20v1.2.2.pdf

def setupHelp():
    return '''\
Solaris - can only receive preset dump, unable to send one for now
'''


def name():
    return 'JB Solaris'


def numberOfBanks():
    return 128


def numberOfPatchesPerBank():
    return 128


def isDefaultName(patchName):
    return patchName == 'INIT'


def needsChannelSpecificDetection():
    return False


def createDeviceDetectMessage(channel):
    # Identity Request (Universal SysEx)
    return [
        0xf0,  # Start of SysEx (SOX)
        0x7e,  # Non real time
        0x7f,  # Device ID (n = 0x00 – 0x0F or 0x7F)
        0x06,  # General Information
        0x01,  # Identity Request
        0xf7   # End of SysEx (EOX)
    ]


def channelIfValidDeviceResponse(message):
    # Identity Request Response
    message[0:1] = [0xf0] # for some reason, 0xf0 is not here anymore?!
    #for m in message: print(hex(m))
    device_id = message[2] # save device_id just in case
    message[2] = 0x7f 

    if (len(message) == 17 and message == [
            0xf0,  # Start of SysEx (SOX)
            0x7e,  # Non real time
            0x7f,  # Device ID (n = 0x00 – 0x0F or 0x7F)
            0x06,  # General Information
            0x02,  # Identity Reply
            0x00, 0x12, 0x34,  # Manufacturer ID
            0x10, 0x00,  # Device family code (1 = Solaris) (shouldn't it be 0x01 instead??)
            0x01, 0x00,  # Device family member code (1 = Keyboard)
            0xf7   # End of SysEx (EOX)
            ]
        ):
        print("Solaris OS " + ".".join((str(m) for m in message[12:16])))
        return 1 # no midi channel from message
    return -1


def createEditBufferRequest(channel):
    # To request all blocks in the current preset (edit buffer), send a bulk dump request with
    # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F.
    # The data returned will begin with a Frame Start block followed by all preset blocks and ending with a Frame End block
    return [0xf0,
            0x00, 0x12, 0x34,  # Manufacturer
            0x7f,  # Device ID
            0x10,  # Solaris ID
            0x10,  # Bulk Dump Request
            0x7e, 0x7f, 0x7f,  # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F
            0xf7
            ]


def isPartOfEditBufferDump(message):
    if len(message)>7:
        #print(message[0:7])
        device_id = message[4]  # save device_id just in case
        message[4] = 0x7f 
        if message[:7] == [
            0xf0,
            0x00, 0x12, 0x34,  # Manufacturer
            0x7f,  # Device ID
            0x10,  # Solaris ID
            0x11   # Bulk Dump
            ]:
            return True
    return False


def isEditBufferDump(data):
    if len(data)>8:
        # find last sysex msg
        pos = -1
        for i, m in enumerate(reversed(data)): 
            if m == 0xf0:
                pos = i
                break
        if pos==-1: return False
        message = data[len(data)-pos-1:]

        # check it's Frame End
        device_id = message[4]  # save device_id just in case
        message[4] = 0x7f
        #print("maybe "+ ' '.join(str(hex(m)) for m in message))
        if message[:8] == [
            0xf0,
            0x00, 0x12, 0x34,  # Manufacturer
            0x7f,  # Device ID
            0x10,  # Solaris ID
            0x11,  # Bulk Dump
            0x7f
            ]:
            #print("yes")
            return True
    return False


def convertToEditBuffer(channel, message):
    return message


# don't know how to import knobkraft.sysex.splitSysexMessage
def splitSysexMessage(messages):
    result = []
    start = 0
    read = 0
    while read < len(messages):
        if messages[read] == 0xf0:
            start = read
        elif messages[read] == 0xf7:
            result.append(messages[start:read + 1])
        read = read + 1
    return result


def nameFromDump(data):
    name = "unknown name"
    messages = splitSysexMessage(data)

    # find the message with the preset name
    name_address = (((0x20 << 8) + 0x0) << 8) + low
    for message in messages:
        device_id = message[4] # save device_id just in case
        message[4] = 0x7f # device id
        if message[:7] == [
            0xf0,
            0x00, 0x12, 0x34,
            0x7f,
            0x10,
            0x11
            ]:
            high = message[7]
            mid = message[8]
            low = message[9]
            payload = len(message)-(7+3+1+1)  # header 7 address 3 checksum 1 0xf7 1
            #print(high,mid,low, payload)
            address = (((high << 8) + mid) << 8) + low
            
            #print(address, name_address)
            
            if address <= name_address < address + payload:
                # found it!
                offset = name_address - address
                #print(offset, name_address, address)
                offset += 7+3  # header 7 address 3
                name = ""
                # Part Name has 20 characters
                for i in range(0,20,2):
                    c = (message[offset+i] << 8) +  message[offset+i+1]
                    name += chr(c)
                break

    return name


# def run_tests():
#     with open("testData/Ultranova_poppy.syx", "rb") as sysex:
#         raw_data = list(sysex.read())
#         assert isSingleProgramDump(raw_data)
#         assert numberFromDump(raw_data) == 35

#         buffer = convertToEditBuffer(1, raw_data)
#         assert isEditBufferDump(buffer)
#         assert numberFromDump(buffer) == 0

#         back_dump = convertToProgramDump(1, buffer, 130)
#         assert numberFromDump(back_dump) == 130

#         assert nameFromDump(raw_data) == "Poppy"
#         assert nameFromDump(buffer) == nameFromDump(raw_data)
#         same_patch = renamePatch(raw_data, "Papaver")
#         assert nameFromDump(same_patch) == "Papaver"
#         same_same = renamePatch(same_patch, "Cr4zy Name°$    overflow")
#         assert nameFromDump(same_same) == "Cr4zy Name_$"

#         assert calculateFingerprint(same_same) == calculateFingerprint(raw_data)
#         assert friendlyBankName(2) == 'C'


# if __name__ == "__main__":
#     run_tests()
