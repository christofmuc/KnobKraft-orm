#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   John Bowen Solaris Adaptation version 0.4 by Stéphane Conversy, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#


# see https://owner.johnbowen.com/images//files/Solaris%20SysEx%20v1.2.2.pdf

# STATUS
# Device detection: done
# Edit Buffer Capability: done
# Getting the patch's name with checksum verification: done
# Setting the patch's name with new checksum: done
# Send to edit buffer: done with DIN, not working with USB

# TODO
# Send preset according to Device ID before sending
# verify that the OS of the patch to send is the same as the Solaris unit

# FUTURE
# Preset Dump Capability: as soon as Solaris supports it
# Bank Dump Capability: as soon as Solaris supports it 


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

# find last sysex msg (more efficient than splitting all sysex msgs)
def find_last_sysex(data):
    pos = -1
    for i, m in enumerate(reversed(data)): 
        if m == 0xf0:
            pos = i
            break
    return pos

# much of the below functions compare a received message with an expected one
# however, the received message may differ from the expected one in some variable fields (e.g. deviceid ou channel) 
# get_and_set_expected* make field-variable midi messages comparable,
# by assigning the expected message field to the received message
# and by returning the original value of the field, would this prove useful
def get_and_set_expected_index(field_index, message, expected_message):
        original = message[field_index]
        message[field_index] = expected_message[field_index]
        return original
def get_and_set_expected_range(first_index, last_index, message, expected_message):
        original = message[first_index:last_index]
        message[first_index:last_index] = expected_message[first_index:last_index]
        return original
# pseudo-overloaded version to make it more usable
def get_and_set_expected(index_or_range, message, expected_message):
    if type(index_or_range) == int:
        return get_and_set_expected_index(index_or_range, message, expected_message)
    else:
        return get_and_set_expected_range(index_or_range[0], index_or_range[1], message, expected_message)



# ----------------
# Identity, number of presets and banks


def setupHelp():
    return '''\
Solaris - can send, receive, rename Edit Buffer only (beware: there is a bug that prevents sending with USB)
'''


def name():
    return 'JB Solaris'


def numberOfBanks():
    return 128


def numberOfPatchesPerBank():
    return 128


def isDefaultName(patchName):
    return patchName == 'INIT'


def generalMessageDelay():
    return 50

# ----------------
# Device detection

identity_common = [
    0xf0,  # Start of SysEx (SOX)
    0x7e,  # Non real time
    0x7f,  # Device ID (n = 0x00 – 0x0F or 0x7F)
    0x06,  # General Information
]

# Identity Request (Universal SysEx)
identity_request = identity_common + [
    0x01,  # Identity Request
    0xf7   # End of SysEx (EOX)
]


# Sending message to force reply by device
def createDeviceDetectMessage(channel):
    return identity_request


# Identity Request Response
identity_reply = identity_common + [
    0x02,  # Identity Reply
    0x00, 0x12, 0x34,  # Manufacturer ID
    0x10, 0x00,  # Device family code (1 = Solaris) (shouldn't it be 0x01 instead??)
    0x01, 0x00,  # Device family member code (1 = Keyboard)
    0x0, 0x0, 0x0, 0x0,  # Software revision level
    0xf7   # End of SysEx (EOX)
]


# Checking if reply came
def channelIfValidDeviceResponse(message):
    message[0:1] = [0xf0] # FIXME for some reason, 0xf0 is no longer in the message?!
    expected_message = identity_reply
    if len(message) == len(expected_message):
        device_id = get_and_set_expected(2, message, expected_message)
        software_revision_level = get_and_set_expected((12, 16), message, expected_message)
        if (message == expected_message):
            print("Solaris id:" + str(device_id) +
                  " OS v" + ".".join((str(m) for m in software_revision_level)))
            return 1 # no midi channel from message
    return -1


# Optionally specifying to not do channel specific detection
def needsChannelSpecificDetection():
    return False


# ----------------------
# Edit Buffer Capability

common_header = [
    0xf0,  # Start of SysEx (SOX)
    0x00, 0x12, 0x34,  # Manufacturer
    0x7f,  # Device ID
    0x10   # Solaris ID
]


# Bulk Dump Request
# To request all blocks in the current preset (edit buffer), send a bulk dump request with
# base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F.
# The data returned will begin with a Frame Start block followed by all preset blocks and ending with a Frame End block
bulk_dump_request = common_header + [
    0x10,  # Bulk Dump Request
    0x7e, 0x7f, 0x7f,  # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F
    0xf7  # End of SysEx (EOX)
]

# Requesting the edit buffer from the synth
def createEditBufferRequest(channel):
    return bulk_dump_request


# Bulk Dump
bulk_dump_reply = common_header + [
    0x11  # Bulk Dump
]

# Handling edit buffer dumps that consist of more than one MIDI message
def isPartOfEditBufferDump(message):
    expected_message = bulk_dump_reply
    if len(message) > len(expected_message):
        device_id = get_and_set_expected(4, message, expected_message)
        if message[:len(expected_message)] == expected_message:
            return True
    return False


# Bulk Dump with Frame End
bulk_dump_frame_end = bulk_dump_reply + [
    0x7f
] #, bb, pp,

# Checking if a MIDI message is an edit buffer dump
def isEditBufferDump(data):
    expected_message = bulk_dump_frame_end
    if len(data) > len(expected_message):
        pos = find_last_sysex(data)
        if pos == -1 or pos < len(expected_message):
            return False
        # last sysex found, put it in message
        message = data[len(data)-pos-1:]
        # check whether it's a Frame End
        device_id = get_and_set_expected(4, message, expected_message)
        if message[:len(expected_message)] == expected_message:
            return True  # signal the end of multi-frame message

    return False


# Creating the edit buffer to send
def convertToEditBuffer(channel, long_message):
    #messages = splitSysexMessage(long_message)
    #res = sum(messages[1:-1], []) # remove Frame Start and Frame End
    res = long_message
    return res



# ------------------------
# Getting and Setting the patch's name

def hml_to_address(high, mid, low):
    return ((( (high & 0x7f) << 7) + (mid & 0x7f)) << 7) + (low & 0x7f)

# Preset Name address to find
name_address = hml_to_address(0x20, 0x0, 0x0)

bulk_dump_address = bulk_dump_reply + [
    0x00, 0x00, 0x00  # Address
]

def find_name(data):
    ''' returns the offset of the preset name within a multipart sysex message'''
    print([f'{s:02x}' for s in data])

    # Part Name has 20 characters with 2 bytes each
    # find the message containing the preset name address within all Bulk Dump messages
    offset = -1
    expected_message = bulk_dump_address
    messages = splitSysexMessage(data)
    for msg_index, message in enumerate(messages):
        # the name sysex message is composed of header (including address) + name + checksum + cat1 + cat2 + 0xf7
        if len(message) < len(expected_message): # + 20*2 + 1 + 1 + 1 + 1:
            continue
        orig = message[:]
        device_id = get_and_set_expected(4, message, expected_message)
        high, mid, low = get_and_set_expected((7, 10), message, expected_message)

        if message[:len(expected_message)] == expected_message:
            if [high, mid, low] == [0x20, 0x0, 0x0]:
                # found it!
                offset = len(expected_message)

                # verify checksum
                checksum = message[-2]
                #s = sum([0x20,0,0] + message[10:-2]) # 7 = len (expected_message) - len([0x20,0,0])
                s = sum(orig[7:-2]) # 7 = len (expected_message) - len([0x20,0,0])
                
                #s = 0
                #for d in [0x20,0,0] + message[10:-2]:
                #    s += d & 0x7f
                #    #s &= 0x7f

                if ((s + checksum) & 0x7f) != 0:
                    print("bad cheksum")
                    #offset = -1
                    #assert(0) # FIXME should do something smarter

                    print('solaris debug')
                    print('orig: ' + str([f'{s:02x}' for s in orig]))
                    print('msg : ' + str([f'{s:02x}' for s in message]))
                    print('data: ' + str([f'{s:02x}' for s in orig[7:-2]]))
                    print(f'sum : {s:03x}')
                    print(f'checksum: {checksum:02x}')
                    print(f'd+c : {(s + checksum):03x}')
                    print(f'd+c&: {(s + checksum) & 0x7f:02x}')

                

                # found it => exit message loop
                break
    
    # if offset has been found
    if offset != -1:
        # so far, offset is related to the single sysex message, not the whole multipart sysex message
        # so add the length of the preceding messages to make it relative to the whole multipart sysex message
        offset += sum(len(msg) for msg in messages[:msg_index])

    return offset


def nameFromDump(data):
    offset = find_name(data)
    if offset == -1:
        return "unknown"

    name = ""
    # Part Name has 20 characters with 2 bytes each
    # denibblize it
    for i in range(0, 20*2, 2):
        c = (data[offset+i] << 8) +  data[offset+i+1]
        name += chr(c)
    name = name.strip()

    return name


def renamePatch(data, new_name):
    offset = find_name(data)
    if offset == -1:
        return

    # make new_name 20-character long
    name = new_name.ljust(20)[:20]

    # nibblize it
    name_enc = []
    for c in name:
        name_enc += [(ord(c) >> 8) & 0x7f, ord(c) & 0x7f]

    # compute new checksum    
    s = sum([0x20,0,0] + name_enc + data[offset+40:offset+40+1+1]) # cat1 + cat2
    checksum = 0x80 - (s & 0x7f)
    
    # verify new checksum
    assert (s + checksum) & 0x7 == 0

    # assemble new data
    # 1 + 1 + 1 : cat1 + cat2 + 0xf7
    new_data = data[:offset] + name_enc + [0,0] + [checksum] + data[offset + len(name_enc) + 1 + 1 + 1:]

    return new_data



# --------------
# Testing


def run_tests():
    with open("testData/JBSolaris-INIT.syx", "rb") as sysex:
        raw_data = list(sysex.read())
        assert nameFromDump(raw_data) == "INIT"

        # same name produces the same data
        renamed = renamePatch(raw_data, "INIT")
        assert len(raw_data) == len(renamed)
        assert raw_data == renamed
        check_name = nameFromDump(renamed)
        assert check_name == "INIT"
        
        # different name
        renamed = renamePatch(raw_data, "SolarisINIT")
        assert len(raw_data) == len(renamed)
        assert raw_data != renamed
        check_name = nameFromDump(renamed)
        assert check_name == "SolarisINIT"
        
        # assert isSingleProgramDump(raw_data)
        # assert numberFromDump(raw_data) == 35

        # buffer = convertToEditBuffer(1, raw_data)
        # assert isEditBufferDump(buffer)
        # assert numberFromDump(buffer) == 0

        # back_dump = convertToProgramDump(1, buffer, 130)
        # assert numberFromDump(back_dump) == 130

        # assert nameFromDump(raw_data) == "Poppy"
        # assert nameFromDump(buffer) == nameFromDump(raw_data)
        # same_patch = renamePatch(raw_data, "Papaver")
        # assert nameFromDump(same_patch) == "Papaver"
        # same_same = renamePatch(same_patch, "Cr4zy Name°$    overflow")
        # assert nameFromDump(same_same) == "Cr4zy Name_$"

        # assert calculateFingerprint(same_same) == calculateFingerprint(raw_data)
        # assert friendlyBankName(2) == 'C'


if __name__ == "__main__":
    run_tests()
