#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   John Bowen Solaris Adaptation version 0.3 by Stéphane Conversy, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#


# see https://owner.johnbowen.com/images//files/Solaris%20SysEx%20v1.2.2.pdf

# STATUS
# Device detection: done
# Edit Buffer Capability: done
# Getting the patch's name: done
# Send to edit buffer: done with DIN, does not work with USB (Solaris firmware bug)

# TODO
# verify checksum
# Send preset according to Device ID before sending
# verify that the OS of the patch to send is the same as the Solaris unit

# FUTURE
# ASA Solaris supports it, Preset and Bank Dump Capability


# ----------------
# helper functions

def print_midi_message(message):
    for m in message: print(f'{m:02x}',end=' ')
    print()

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


# ----------------
# Identity, number of presets and banks


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


# def generalMessageDelay():
#     return 20


# ----------------
# Device detection

# Sending message to force reply by device
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

# Checking if reply came
def channelIfValidDeviceResponse(message):
    message[0:1] = [0xf0] # for some reason, 0xf0 is not in the message anymore?!
    #print_midi_message(message)

    # Identity Request Response
    expected_message = [
        0xf0,  # Start of SysEx (SOX)
        0x7e,  # Non real time
        0x7f,  # Device ID (n = 0x00 – 0x0F or 0x7F)
        0x06,  # General Information
        0x02,  # Identity Reply
        0x00, 0x12, 0x34,  # Manufacturer ID
        0x10, 0x00,  # Device family code (1 = Solaris) (shouldn't it be 0x01 instead??)
        0x01, 0x00,  # Device family member code (1 = Keyboard)
        0x0, 0x0, 0x0, 0x0,  # Software revision level
        0xf7   # End of SysEx (EOX)
    ]

    if len(message) == len(expected_message):
        device_id = message[2]  # save msg value
        message[2] = 0x7f  # set to expected value for the msg comparison
        software_revision_level = message[12:16]  # save msg value
        message[12:16] = [0]*4  # set to expected value for the msg comparison
        if (message == expected_message):
            print("Solaris id:" + str(device_id) +
                " OS v" + ".".join((str(m) for m in software_revision_level)))
            return 1 # no midi channel from message
    return -1


# Optionally specifying to not to channel specific detection
def needsChannelSpecificDetection():
    return False


# ----------------------
# Edit Buffer Capability

# Requesting the edit buffer from the synth
def createEditBufferRequest(channel):
    # Bulk Dump Request
    # To request all blocks in the current preset (edit buffer), send a bulk dump request with
    # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F.
    # The data returned will begin with a Frame Start block followed by all preset blocks and ending with a Frame End block
    return [
        0xf0,
        0x00, 0x12, 0x34,  # Manufacturer
        0x7f,  # Device ID
        0x10,  # Solaris ID
        0x10,  # Bulk Dump Request
        0x7e, 0x7f, 0x7f,  # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F
        0xf7
    ]


# Handling edit buffer dumps that consist of more than one MIDI message
def isPartOfEditBufferDump(message):
    # Bulk Dump
    expected_message = [
        0xf0,  # Start of SysEx (SOX)
        0x00, 0x12, 0x34,  # Manufacturer
        0x7f,  # Device ID
        0x10,  # Solaris ID
        0x11   # Bulk Dump
    ]
    if len(message) > len(expected_message):
        #print_midi_message(message[0:7])
        device_id = message[4]  # save msg value
        message[4] = 0x7f  # set to expected value for the msg comparison
        if message[:len(expected_message)] == expected_message:
            return True
    return False


# Checking if a MIDI message is an edit buffer dump
def isEditBufferDump(data):
    # Bulk Dump with Frame End
    expected_message = [
        0xf0,  # Start of SysEx (SOX)
        0x00, 0x12, 0x34,  # Manufacturer
        0x7f,  # Device ID
        0x10,  # Solaris ID
        0x11,  # Bulk Dump
        0x7f   #, bb, pp,
        # 0xf7
    ]

    if len(data) > len(expected_message):
        # find last sysex msg
        pos = -1
        for i, m in enumerate(reversed(data)): 
            if m == 0xf0:
                pos = i
                break
        if pos == -1 or pos < len(expected_message):
            return False
        # last sysex found
        message = data[len(data)-pos-1:]
        
        # check it's a Frame End
        device_id = message[4]  # save msg value
        message[4] = 0x7f  # set to expected value for the msg comparison
        #print("maybe "+ ' '.join(str(hex(m)) for m in message))
        if message[:len(expected_message)] == expected_message:
            #print("yes")
            return True  # signal the end since we received a Frame End
    return False


# Creating the edit buffer to send
def convertToEditBuffer(channel, long_message):
    #messages = splitSysexMessage(long_message)
    #res = sum(messages[1:-1], []) # remove Frame Start and Frame End
    res = long_message
    #print_midi_message(res)
    return res



# ------------------------
# Getting the patch's name

def nameFromDump(data):
    name = "unknown name"

    # find the message containing the preset name within all Bulk Dump messages
    messages = splitSysexMessage(data)

    # Bulk Dump
    expected_message = [
        0xf0,
        0x00, 0x12, 0x34,
        0x7f,
        0x10,
        0x11,
        0x00, 0x00, 0x00,
    ]

    # Preset Name
    # address to find
    def hml_to_address(high, mid, low):
        return ((( (high & 0x7f) << 7) + (mid & 0x7f)) << 7) + (low & 0x7f)

    name_address = hml_to_address(0x20, 0x0, 0x0)
    
    for message in messages:
        # header + name + checksum + 0xf7
        if len(message) < len(expected_message) + 20*2 + 2 : continue

        device_id = message[4]  # save msg value
        message[4] = 0x7f  # set to expected value for the msg comparison
        high, mid, low = message[7:10] # save adress
        message[7:10] = [0]*3  # set to expected value for the msg comparison
        if message[:len(expected_message)] == expected_message:
            payload = len(message) - (len(expected_message)+1+1)  # header checksum 0xf7
            #print(high,mid,low, payload)
            address = hml_to_address(high, mid, low)
            #print(address, name_address)
            if address <= name_address < address + payload:
                # found it!
                offset = name_address - address
                #print(offset, name_address, address)
                offset += len(expected_message)
                name = ""
                # Part Name has 20 characters 2 bytes each
                for i in range(0, 20*2, 2):
                    c = (message[offset+i] << 8) +  message[offset+i+1]
                    name += chr(c)
                name = name.strip()
                break # found it => exit message loop

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
