#
#   Copyright (c) 2021 Christof Ruch. All rights reserved.
#   Zoom MS Series Adaptation version 0.9 by Cedric Tessier, 2022.
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
# based on https://github.com/g200kg/zoom-ms-utility/blob/master/midimessage.md
#
import hashlib

from enum import IntEnum
from typing import List

import testing

MODEL_OFFSET = 0x3

NAME_OFFSET = 0x84
NAME_OFFSET_60B = 0x5b

NAME_LEN = 10

NAME_SPECIALCHARSET = ' !#$%&\'()+,-.;=@[]^_`{}~'


class Model(IntEnum):
    MS50G = 0x58
    MS60B = 0x5f
    MS70CDR = 0x61

    def __str__(self):
        return 'MS-{0}'.format(self.name[2:])


CURRENT_MODEL = Model.MS50G
EDIT_ENABLED = False


def getNameOffset(model=CURRENT_MODEL):
    return NAME_OFFSET_60B if model == Model.MS60B else NAME_OFFSET


def getNamePattern(model=CURRENT_MODEL):
    # name encoding in a patch dump as (offset, len) tuples
    return [(0, 2), (3, 7), (11, 1)] if model == Model.MS60B else [(0, 1), (2, 7), (10, 2)]


def wrapSysex(data):
    return [0xf0,  # Sysex message
            0x52, 0x00,  # Zoom
            int(CURRENT_MODEL),
            ] + data + [0xf7]  # EOX


def name():
    return 'ZOOM MS Series'


def isDefaultName(patchName):
    return patchName == 'Empty'


def createDeviceDetectMessage(channel):
    return [0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7]


def deviceDetectWaitMilliseconds():
    return 200


def generalMessageDelay():
    return 100


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    global CURRENT_MODEL
    global EDIT_ENABLED

    EDIT_ENABLED = False
    if (len(message) > 10
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x7e  # Identity Request
            and message[2] == 0x00
            and message[3] == 0x06
            and message[4] == 0x02
            and message[5] == 0x52
            and message[6] in list(Model)
            and message[7] == 0x00
            and message[8] == 0x00
            and message[9] == 0x00):
        CURRENT_MODEL = Model(message[6])
        version = bytes(message[10:14]).decode('ascii')
        print('Zoom {} ({})'.format(str(CURRENT_MODEL), version))
        #  no midi channel
        return 0
    return -1


def enableParameterEditRequest():
    return wrapSysex([0x50])


def unlock():
    # making sure pedal is 'unlocked' (aka edit enabled)!
    # as we need the model from 'device detect' answer and don't want to send it everytime,
    # a global variable is used to keep track and only send the request once...
    global EDIT_ENABLED

    if not EDIT_ENABLED:
        EDIT_ENABLED = True
        return enableParameterEditRequest()
    return []


def createEditBufferRequest(channel):
    return unlock() + wrapSysex([0x29])


def isEditBufferDump(message):
    return (len(message) > 5
            and message[0] == 0xf0
            and message[1] == 0x52  # Zoom
            and message[2] == 0x00
            and message[3] in list(Model)
            and message[4] == 0x28
            )


def convertToEditBuffer(channel, message):
    return message


def modelFromDump(message):
    if isEditBufferDump(message):
        return Model(message[MODEL_OFFSET])
    raise Exception("Neither edit buffer nor program dump can't be converted")


def nameFromDump(message):
    if isEditBufferDump(message):
        model = modelFromDump(message)
        off = getNameOffset(model)
        pat = getNamePattern(model)
        # name is at a different offset and encoded differently between models
        # MS-50G / 70CDR: N0 N1 00 N2 N3 N4 N5 N6 N7 N8 00 N9
        # MS-60B:         N0 00 N1 N2 N3 N4 N5 N6 N7 00 N8 N9
        namearr = sum([message[off+p:off+p+l] for (p, l) in pat], [])
        name = ''.join([chr(x) for x in namearr])
        return name.strip()
    return 'Invalid'


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 50


def createProgramDumpRequest(channel, patchNo):
    # concat Program Change with sysex
    return unlock() + [0xc0, patchNo] + wrapSysex([0x29])


def isSingleProgramDump(message):
    return isEditBufferDump(message)


def convertToProgramDump(channel, message, program_number):
    # TODO: a program dump is just an edit buffer, stored using the following sysex (with XX == program_number)
    # sendmidi dev "ZOOM MS Series" hex syx 52 00 61 32 01 00 00 XX 00 00 00 00 00
    return message


def renamePatch(message, new_name):
    if isEditBufferDump(message):
        model = modelFromDump(message)
        off = getNameOffset(model)
        pat = getNamePattern(model)
        # name is alpha-num + special chars (replace everything else by '_' and pad with spaces)
        clean_name = new_name.strip()[:NAME_LEN].ljust(NAME_LEN, ' ')
        valid_name = [ord(x) if x.isalnum() or x in NAME_SPECIALCHARSET else ord('_') for x in clean_name]
        # name is encoded differently between models...
        # MS-50G / 70CDR: N0 N1 00 N2 N3 N4 N5 N6 N7 N8 00 N9
        # MS-60B:         N0 00 N1 N2 N3 N4 N5 N6 N7 00 N8 N9
        # compute position of null bytes...
        l1 = pat[0][1]
        l2 = l1 + pat[1][1]
        patch_name = valid_name[0:l1] + [0x00] + valid_name[l1:l2] + [0x00] + valid_name[l2:]
        return message[:off] + patch_name + message[off+NAME_LEN+2:]
    raise Exception("Neither edit buffer nor program dump can't be converted")


def friendlyProgramName(program):
    # follow Zoom numbering
    return str(program + 1)


def calculateFingerprint(message):
    # Skip sysex and patch name
    model = modelFromDump(message)
    off = getNameOffset(model)
    data = message[1:off]
    return hashlib.md5(bytearray(data)).hexdigest()


def make_test_data():
    def generate(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        raw_data = test_data.all_messages[0]

        # Some additional tests
        assert modelFromDump(raw_data) == Model.MS70CDR
        assert str(modelFromDump(raw_data)) == "MS-70CDR"
        same_same = renamePatch(raw_data, "C-D-R<$o  overflow")
        assert nameFromDump(same_same) == "C-D-R_$o"

        # Supply the edit buffer and some test helpers to the generic test routines
        yield testing.ProgramTestData(message = raw_data, name="C-D-R", rename_name="CDR")

    return testing.TestData(sysex="testData/ZoomMS-CDR.syx", edit_buffer_generator=generate)
