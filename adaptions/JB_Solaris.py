#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   John Bowen Solaris Adaptation version 0.6 by Stéphane Conversy, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
#

# ####This adaptation uses Structural Pattern Matching, available from Python 3.10
# => reverted back to 3.8-compatible code
# import sys
# assert sys.version_info >= (3,10)


# see https://owner.johnbowen.com/images//files/Solaris%20SysEx%20v1.2.2.pdf

# STATUS

# Device detection: done
# Edit Buffer Capability: done
# Getting the patch's name with checksum verification: done
# Setting the patch's name with new checksum: done
# Send to edit buffer: done with DIN, not working with USB (USB connection lost)
# Getting the part/layer's name with checksum verification: done
# Setting the part/layer's name with new checksum: done

# DOING
# Categories
# Bank Descriptors

# TODO
# Send preset according to Device ID before sending
# Verify that the OS of the patch to send is the same as the Solaris unit, not sure if it's possible

# FUTURE
# Preset Dump Capability: as soon as Solaris supports it
# Bank Dump Capability: as soon as Solaris supports it

from typing import List, Dict, Tuple, Iterator
#TypeAlias # 3.10

# ----------------
# helper functions

MidiMessage = list[int]
#MidiMessage = bytes

def m2str(message:MidiMessage) -> str:
    return ' '.join(f'{m:02x}' for m in message)


def print_midi(message:MidiMessage) -> None:
    print(m2str(message))


def nibblize_str(data:str) -> MidiMessage:
    nibblized = MidiMessage()
    for c in data:
        nibblized += MidiMessage([(ord(c) >> 8) & 0x7f, ord(c) & 0x7f])
    return nibblized


def denibblize_str(data:MidiMessage) -> str:
    denibblized = ""
    for i in range(0, len(data), 2):
        c = (data[i] << 8) + data[i+1]
        denibblized += chr(c)
    return denibblized


# sysex message iterator through bulk messages
def nextSysexMessage(messages:MidiMessage, SOX:int=0xf0, EOX:int=0xf7) -> Iterator[Tuple[MidiMessage, slice]]:
    start = 0
    read = 0
    message = MidiMessage()
    # for i in range(len(messages)):
    #     b = bytes(messages[i:i+1]) # bytes
    #     message = message + b
    for b in messages:
        message.append(b)
        if b == SOX:
            start = read
        elif b == EOX:
            yield (message, slice(start, read + 1))
            message = MidiMessage()
        read = read + 1


def nextSysexMessageReversed(messages:MidiMessage) -> Iterator[Tuple[MidiMessage, slice]]:
    for message, revslice_ in nextSysexMessage(MidiMessage(reversed(messages)), SOX=0xf7, EOX=0xf0):
        yield MidiMessage(reversed(message)), revslice_



# ------------------
# Identity and Setup

def name() -> str:
    return 'JB Solaris'

def setupHelp() -> str:
    return '''Settings: on the Solaris unit, in the 'Midi' page, switch 'TxSysEx' and 'RxSysEx' on.

Capabilities: can send, receive, rename preset and rename parts in Edit Buffer mode only.

**Caution**:
Sending SysEx through the Solaris DIN connection works.
On Windows, sending SysEx through the Solaris USB connection works.
On macOS, sending SysEx through a USB3 or a USB2 port from the computer to the Solaris USB currently disconnects the Solaris USB.
With a USB2 port, the connection can be reestablished by unplugging/plugging the USB cable.
With a USB3 port, the connection cannot be reestablished, and this will require a Solaris reboot.
If you have a Mac and a USB1 port, please report if sending SysEx is working, this could prove valuable to debug this USB problem.
'We have top men working on it. Top. Men.'

DOING:
Categories
Bank descriptors

FUTURE:
(Random Access) Preset Dump Capability: as soon as Solaris supports it
Bank Dump Capability: as soon as Solaris supports it
'''

# def generalMessageDelay() -> int:
#     return 50


# -----
# banks


def numberOfBanks() -> int:
    return 128


def numberOfPatchesPerBank() -> int:
    return 128


def bankDescriptors() -> List[Dict]:
    # this is the official Factory bank set
    # TODO: find a way to build it according to the actual location of the bank, maybe by recognizing some preset names?
    return [
        {
            "bank": 0,
            "name": "John Bowen",
            "size": 62,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 1,
            "name": "Marco Paris",
            "size": 128,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 2,
            "name": "Carl Lofgren",
            "size": 91,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 3,
            "name": "Scarr, Ader, Hummel, Kuchar, Keel",
            "size": 84,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 4,
            "name": "Ken Elhardt",
            "size": 52,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 5,
            "name": "Jimmy V.",
            "size": 41,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 6,
            "name": "Robert Wittek",
            "size": 51,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 7,
            "name": "Toby Emerson",
            "size": 128,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 8,
            "name": "Christoph Eckert",
            "size": 95,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 9,
            "name": "Francois Neumann-rystow",
            "size": 90,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 10,
            "name": "Brian Kehew",
            "size": 0,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 11,
            "name": "Mike Johnson 1",
            "size": 128,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 12,
            "name": "Mike Johnson 2",
            "size": 128,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 13,
            "name": "Mike Johnson 3",
            "size": 128,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 14,
            "name": "Wouter van Beek",
            "size": 72,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 15,
            "name": "??",
            "size": 0,
            "type": "Patch",
            "isROM": False
        },
        {
            "bank": 16,
            "name": "OSv2 demo",
            "size": 24,
            "type": "Patch",
            "isROM": False
        },
    ]


def bankSelect(channel:int, bank:int) -> MidiMessage:
    # MIDI CC with controller number 32, which is used by many synth as the bank select controller.
    return MidiMessage([0xb0 | (channel & 0x0f), 32, bank])



# ----------------
# Device detection


# Sending message to force reply by device
def createDeviceDetectMessage(channel:int) -> MidiMessage:
    return MidiMessage([
        0xf0,  # Start of SysEx (SOX)
        0x7e,  # Non real time
        0x7f,  # Device ID (n = 0x00 – 0x0F or 0x7F)
        0x06,  # General Information
        0x01,  # Identity Request
        0xf7   # End of SysEx (EOX)
    ])


# Checking if reply came
def channelIfValidDeviceResponse(message:MidiMessage) -> int:
    # match message:
    #     case [
    if len(message) < 16: return -1
    device_id = message[2]
    [v1,v2,v3,v4] = message[12:16]
    if message == [
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
            print("Solaris id:" + str(device_id) + " OS v" + ".".join((str(m) for m in [v1,v2,v3,v4])))
            return 1
    return -1


# Optionally specifying to not do channel specific detection
def needsChannelSpecificDetection() -> bool:
    return False


# def deviceDetectWaitMilliseconds(): pass



# ----------------------
# Edit Buffer Capability


# Bulk Dump Request
# To request all blocks in the current preset (edit buffer), send a bulk dump request with
# base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F.
# The data returned will begin with a Frame Start block followed by all preset blocks and ending with a Frame End block

# Requesting the edit buffer from the synth
def createEditBufferRequest(channel:int) -> MidiMessage:
    return MidiMessage([
        0xf0,  # Start of SysEx (SOX)
        0x00, 0x12, 0x34,  # Manufacturer
        0x7f,  # Device ID
        0x10,  # Solaris ID
        0x10,  # Bulk Dump Request
        0x7e, 0x7f, 0x7f,  # base address of Frame Start (high byte 7E) and bank# = 7F and preset# = 7F
        0xf7   # End of SysEx (EOX)
    ])


# Handling edit buffer dumps that consist of more than one MIDI message
def isPartOfEditBufferDump(message:MidiMessage) -> bool:
    # match message:
    #     case [
    if len(message) < 7: return False
    device_id = message[4]
    if message[0:7] == [
            0xf0,  # Start of SysEx (SOX)
            0x00, 0x12, 0x34,  # Manufacturer
            device_id, #_,     # Device ID
            0x10,  # Solaris ID
            0x11,  # Bulk Dump Request
            #*_
        ]:
            return True
    return False


# Checking if a MIDI message is an edit buffer dump
def isEditBufferDump(data:MidiMessage) -> bool:
    for message, _ in nextSysexMessageReversed(data):
        # match message:
        #     case [
        if len(message) < 8: return False
        device_id = message[4]
        if message[0:8] == [
                0xf0,  # Start of SysEx (SOX)
                0x00, 0x12, 0x34,  # Manufacturer
                device_id,     # Device ID
                0x10,  # Solaris ID
                0x11,  # Bulk Dump Request
                0x7f,  # Frame End
                #*_     # should be 0xf7 (?) # FIXME: actually check it
            ]:
                return True
        return False
    return False


# Creating the edit buffer to send
def convertToEditBuffer(channel:int, long_message:MidiMessage) -> MidiMessage:
    return long_message



# ----------------------
# Program Dump Capability


# def createProgramDumpRequest(channel, patchNo)
# def isSingleProgramDump(message)
# def convertToProgramDump(channel, message, program_number)

# def numberFromDump(message)



# ----------------------
# Bank Dump Capability

# def createBankDumpRequest(channel, bank)
# def isPartOfBankDump(message)
# def isBankDumpFinished(messages)
# def extractPatchesFromBank(messages)



# ------------------------------------
# Getting and Setting the patch's name


# high 0x20: preset name
# high 0x17+p: part/layer name
def _find_name(data:MidiMessage, high:int=0x20, middle:int=0x0, low:int=0x0) -> Tuple[str, slice]:
    ''' return (name,slice_): 'name' is denibbled, 20-character long name (inc. spaces), 'slice_' is the slice in the wole data'''

    offset = -1
    name = ""
    for message, slice_ in nextSysexMessage(data):
        # match message:
        #     case [
        if len(message) < 10: continue
        device_id = message[4]
        [h, m, l] = message[7:10]
        if message[0:10] == [
                0xf0,   # Start of SysEx (SOX)
                0x00, 0x12, 0x34,  # Manufacturer
                device_id, #_,      # Device ID
                0x10,   # Solaris ID,
                0x11,   # Bulk Dump Request
                h,m,l,  # Address
                #*_
                ]:
                    if [h, m, l] != [high, middle, low]:
                        continue

                    # verify checksum
                    s = sum(message[7:-2]) # 7 = address location
                    checksum = message[-2]

                    if ((s + checksum) & 0x7f) == 0:
                        offset = slice_.start + 7 + 3
                        # Part Name has 20 characters with 2 bytes each
                        name = denibblize_str(data[offset:offset+40])
                    else:
                        print("solaris: bad name '" + denibblize_str(data[offset:offset+40]) + "' checksum " + f'{checksum:02x}')
                        
                    break
    return name, slice(offset, offset+40)


def _rename(data:MidiMessage, new_name:str, slice_:slice) -> MidiMessage:

    # make new_name 20-character long
    name = new_name.ljust(20)[:20]

    # nibblize it
    nibblized = nibblize_str(name)

    # compute new checksum
    categories = data[slice_.stop:slice_.stop+2]
    s = sum(MidiMessage([0x20,0,0]) + nibblized + categories)
    checksum = 0x80 - (s & 0x7f)
    
    # verify checksum just in case
    if (s + checksum) & 0x7 != 0:
        print("solaris: bad new name '" + new_name + "' checksum " + f'{checksum:02x}')
        #return None

    # assemble new data
    # 1 + 1 + 1 : cat1 + cat2 + 0xf7
    new_data = data[:slice_.start] + nibblized + categories + MidiMessage([checksum]) + data[slice_.stop+1+1+1:]

    return new_data


def isDefaultName(patchName:MidiMessage) -> bool:
    return patchName == 'INIT'


def nameFromDump(data:MidiMessage) -> str:
    name, slice_ = _find_name(data)
    if slice_.start == -1:
        return "unknown"

    return name.strip()


def renamePatch(data:MidiMessage, new_name:str) -> MidiMessage:
    _, slice_ = _find_name(data)
    if slice_.start == -1:
        return MidiMessage()
    return _rename(data, new_name, slice_)



# ---------------------------------
# Exposing layers/splits of a patch

def numberOfLayers(messages:MidiMessage) -> int:
    # TODO: check effective use of layers, but taking into account the allocated voices or part name != INIT?
    return 4


def layerName(messages:MidiMessage, layerNo:int) -> str:
    #print("layer:", layerNo, " - ",end="")
    name, slice_ = _find_name(messages, 0x17, 0x0+layerNo)
    if slice_.start == -1:
        return "unknown"

    return name.strip()


def setLayerName(messages:MidiMessage, layerNo:int, new_name:str) -> MidiMessage:
    _, slice_ = _find_name(messages, 0x17, 0x0+layerNo)
    if slice_.start == -1:
        return MidiMessage()
    return _rename(messages, new_name, slice_)



# -----------------------------------------------------
# Importing categories stored in the patch in the synth


# Solaris categories
_category1 = ["None", "Arpeggio", "Bass", "Drum", "Effect", "Keyboard", "Lead", "Pad", "Sequence", "Texture", "Atmosphere", "Bells", "Mono", "Noise", "Organ", "Percussive", "Strings", "Synth", "Vocal"]
for i in range(1,11):
    _category1.append("User " + str(i))
_category2 = ["Acoustic", "Aggressive", "Big", "Bright", "Chord", "Classic", "Dark", "Electric", "Moody", "Soft", "Short", "Synthetic", "Upbeat", "Metallic", "Template"]
for i in range(1,11):
    _category2.append("User " + str(i))


# KnobKraft categories:
# Lead, Pad, Brass, Organ, Keys, Bass, Arp, Pluck, Drone, Drum, Bell, SFX, Ambient, Wind, Voice

# TODO: discrepency for Bell/Bells, SFX/Effect, Keys/Keyboard.
# IDEA: rename patch once they are properly tagged
# IDEA: reorganize banks according to tag (Keys, Bass, Pad etc.)

def storedTags(message:MidiMessage) -> List[str]:
    # categories are stored next to the name, so first, get the name position
    _, slice_ = _find_name(message)
    if slice_.start == -1:
        return [""]
    cat1 = message[slice_.stop]
    cat2 = message[slice_.stop+1]
    return [_category1[cat1], _category2[cat2]]



# -------
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
    
    # with open("testData/JBSolaris-RotorDreams.syx", "rb") as sysex:
    #     raw_data = list(sysex.read())
    #     assert nameFromDump(raw_data) == "JBaRotor Dreams"
    #     renamed = renamePatch(raw_data, "JB Rotor Dreams")
    #     assert len(raw_data) == len(renamed)
    #     assert raw_data != renamed
    #     check_name = nameFromDump(renamed)
    #     assert check_name == "JB Rotor Dreams"
    #     assert storedTags(raw_data) == ['Mono', 'Synthetic']



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

        # msg = identity_request # bulk_dump_request
        # msg = bulk_dump_request
        # print(msg)
        # midiout.send_message(msg)
        
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
