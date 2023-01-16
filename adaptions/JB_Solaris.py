#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   John Bowen Solaris Adaptation version 0.6 by Stéphane Conversy, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#


# see https://owner.johnbowen.com/images//files/Solaris%20SysEx%20v1.2.2.pdf

# STATUS
# Device detection: done
# Edit Buffer Capability: done
# Getting the patch's name with checksum verification: done
# Setting the patch's name with new checksum: done
# Send to edit buffer: done with DIN, not working with USB (USB connection lost)
# Getting the part/layer's name with checksum verification: done
# Setting the part/layer's name with new checksum: done

# TODO
# Categories
# Send preset according to Device ID before sending
# Verify that the OS of the patch to send is the same as the Solaris unit

# FUTURE
# Preset Dump Capability: as soon as Solaris supports it
# Bank Dump Capability: as soon as Solaris supports it



# ----------------
# helper functions

def m2str(message):
    return ' '.join(f'{m:02x}' for m in message)


def print_midi(message):
    print(m2str(message))


def nibblize(data):
    nibblized = []
    for c in data:
        nibblized += [(ord(c) >> 8) & 0x7f, ord(c) & 0x7f]
    return nibblized


def denibblize(data):
    denibblized = ""
    for i in range(0, len(data), 2):
        c = (data[i] << 8) + data[i+1]
        denibblized += chr(c)
    return denibblized


# sysex message generators to iterate through bulk messages
def nextSysexMessage(messages, SOX=0xf0, EOX=0xf7):
    start = 0
    read = 0
    message = []
    for b in messages:
        message.append(b)
        if b == SOX:
            start = read
        elif b == EOX:
            yield (message, slice(start, read + 1))
            message = []
        read = read + 1


def nextSysexMessageReversed(messages):
    for message, revslice_ in nextSysexMessage(reversed(messages), SOX=0xf7, EOX=0xf0):
        yield list(reversed(message)), revslice_


# ----------------
# Identity, number of presets and banks


def setupHelp():
    return '''\
Solaris - can send, receive, rename Edit Buffer only (beware: sending through USB crashes the Solaris USB)
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
#     return 50


# ----------------
# Device detection

# Sending message to force reply by device
def createDeviceDetectMessage(channel):
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
    match (message):
        case [
            0xf0,  # Start of SysEx (SOX)
            0x7e,  # Non real time
            device_id,  # Device ID (n = 0x00 – 0x0F or 0x7F)
            0x06,  # General Information
            0x02,  # Identity Reply
            0x00, 0x12, 0x34,  # Manufacturer ID
            0x10, 0x00,  # Device family code (1 = Solaris) (shouldn't it be 0x01 instead??)
            0x01, 0x00,  # Device family member code (1 = Keyboard)
            v1,v2,v3,v4,  # Software revision level
            0xf7   # End of SysEx (EOX)
        ]:
            print("Solaris id:" + str(device_id) +
                  " OS v" + ".".join((str(m) for m in [v1,v2,v3,v4])))
            return 1
    return -1


# Optionally specifying to not do channel specific detection
def needsChannelSpecificDetection():
    return False


# ----------------------
# Edit Buffer Capability


# Bulk Dump Request
# To request all blocks in the current preset (edit buffer), send a bulk dump request with
# base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F.
# The data returned will begin with a Frame Start block followed by all preset blocks and ending with a Frame End block

# Requesting the edit buffer from the synth
def createEditBufferRequest(channel):
    return  [
        0xf0,  # Start of SysEx (SOX)
        0x00, 0x12, 0x34,  # Manufacturer
        0x7f,  # Device ID
        0x10,  # Solaris ID
        0x10,  # Bulk Dump Request
        0x7e, 0x7f, 0x7f,  # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F
        0xf7   # End of SysEx (EOX)
    ]


# Handling edit buffer dumps that consist of more than one MIDI message
def isPartOfEditBufferDump(message):
    match message:
        case [
            0xf0,  # Start of SysEx (SOX)
            0x00, 0x12, 0x34,  # Manufacturer
            _,     # Device ID
            0x10,  # Solaris ID,
            0x11,  # Bulk Dump Request
            *_
        ]:
            return True
    return False

# Checking if a MIDI message is an edit buffer dump
def isEditBufferDump(data):
    for message, slice_ in nextSysexMessageReversed(data):
        match (message):
            case [
                0xf0,  # Start of SysEx (SOX)
                0x00, 0x12, 0x34,  # Manufacturer
                _,     # Device ID
                0x10,  # Solaris ID,
                0x11,  # Bulk Dump Request
                0x7f,  # Frame End
                *rest
            ]:
                return True
        return False


# Creating the edit buffer to send
def convertToEditBuffer(channel, long_message):
    res = long_message
    return res



# ------------------------
# Getting and Setting the patch's name


# 0x20: preset name, 0x17+p: part/layer name
def find_name(data, high=0x20, middle=0x0, low=0x0):
    offset = -1
    name = ""
    for message, slice_ in nextSysexMessage(data):
        #case [0xf0, 0x00, 0x12, 0x34, _, 0x10, 0x11, _, _, _, *_]:
        #        address = message[7:10]
        match message:
            case [
                0xf0,   # Start of SysEx (SOX)
                0x00, 0x12, 0x34,  # Manufacturer
                _,      # Device ID
                0x10,   # Solaris ID,
                0x11,   # Bulk Dump Request
                h,m,l,  # Address
                *_
            ]:
                if [h,m,l] != [high, middle, low]:
                    continue

                # verify checksum
                s = sum(message[7:-2]) # 7 = address location
                checksum = message[-2]

                if ((s + checksum) & 0x7f) == 0:
                    offset = slice_.start + 7 + 3
                    # Part Name has 20 characters with 2 bytes each
                    name = denibblize(data[offset:offset+40])
                else:
                    print("solaris: bad name '" + denibblize(data[offset:offset+40]) + "' checksum " + f'{checksum:02x}')
                    
                break
    return name, slice(offset, offset+40)


def nameFromDump(data):
    name, slice_ = find_name(data)
    if slice_.start == -1:
         return "unknown"

    return name.strip()


def _rename(data, new_name, slice_):

    # make new_name 20-character long
    name = new_name.ljust(20)[:20]

    # nibblize it
    nibblized = nibblize(name)

    # compute new checksum
    categories = data[slice_.stop:slice_.stop+2]
    s = sum([0x20,0,0] + nibblized + categories)
    checksum = 0x80 - (s & 0x7f)
    
    # verify checksum just in case
    if (s + checksum) & 0x7 != 0:
        print("solaris: bad new name '" + new_name + "' checksum " + f'{checksum:02x}')
        return None

    # assemble new data
    # 1 + 1 + 1 : cat1 + cat2 + 0xf7
    new_data = data[:slice_.start] + nibblized + categories + [checksum] + data[slice_.stop+1+1+1:]

    return new_data


def renamePatch(data, new_name):
    _, slice_ = find_name(data)
    if slice_.start == -1:
         return
    return _rename(data, new_name, slice_)


# --------------
# Exposing layers/splits of a patch

def numberOfLayers(messages):
    return 4


def layerName(messages, layerNo):
    print("layer:", layerNo, " - ",end="")
    name, slice_ = find_name(messages, 0x17, 0x0+layerNo)
    if slice_.start == -1:
         return "unknown"

    print(name.strip())
    return name.strip()


def setLayerName(self, messages, layerNo, new_name):
    _, slice_ = find_name(messages, 0x17, 0x0+layerNo)
    if slice_.start == -1:
         return
    return _rename(messages, new_name, slice_)



# --------------
# Importing categories stored in the patch in the synth


# from typing import List
# def storedTags(message) -> List[str]:
#     return [""]



# --------------
# Testing


def run_tests():
    message1 = [240, 0, 18, 52, 16, 16, 17, 126, 127, 127, 4, 247]
    for msg, slice_ in nextSysexMessage(message1):
        assert msg == message1

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
    
    with open("testData/JBSolaris-RotorDreams.syx", "rb") as sysex:
        raw_data = list(sysex.read())
        assert nameFromDump(raw_data) == "JBaRotor Dreams"
        renamed = renamePatch(raw_data, "JB Rotor Dreams")
        assert len(raw_data) == len(renamed)
        assert raw_data != renamed
        check_name = nameFromDump(renamed)
        assert check_name == "JB Rotor Dreams"



def run_midi_tests():
        class MidiInputHandler(object):
            def __init__(self, port):
                self.port = port
                self._wallclock = time.time()

            def __call__(self, event, data=None):
                message, deltatime = event
                self._wallclock += deltatime
                #print("[%s] @%0.6f %r" % (self.port, self._wallclock, message))
                print ([f'{x:02x}' for x in message])

        import time
        import rtmidi
        midiout = rtmidi.MidiOut()
        available_ports = midiout.get_ports()
        from rtmidi.midiutil import open_midiinput
        midiin, port_name = open_midiinput(0)
        print(port_name)

        #midiin = rtmidi.MidiIn()
        # Don't ignore sysex, timing, or active sensing messages.
        midiin.ignore_types( False, True, True )
        midiin.set_callback(MidiInputHandler(port_name))
        
        midiout.open_port(0)

        msg = [0x90, 60, 112]
        midiout.send_message(msg)
        time.sleep(1)
        msg = [0x80, 60, 0]
        midiout.send_message(msg)

        msg = identity_request # bulk_dump_request
        msg = bulk_dump_request
        print(msg)
        midiout.send_message(msg)
        
        while(True):
            time.sleep(1)
        #print (midiin.get_message())
        
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
