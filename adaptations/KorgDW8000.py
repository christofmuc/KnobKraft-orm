#
#   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import hashlib
import re
from typing import List
from functools import cache

import testing

_OCTAVE_OPTIONS = ['16"', '8"', '4"']
_WAVEFORM_OPTIONS = [
    "Sawtooth", "Square", "Piano", "Electric piano 1", "Electric piano 2", "Clavinet",
    "Organ", "Brass", "Sax", "Violin", "Guitar", "Electric guitar", "Bass", "Digital bass",
    "Bell and whistle", "Sine",
]
_AUTO_BEND_SELECT_OPTIONS = ["Off", "Osc 1", "Osc 2", "Both"]
_AUTO_BEND_MODE_OPTIONS = ["Up", "Down"]
_INTERVAL_OPTIONS = ["1", "-3 ST", "3 ST", "4 ST", "5 ST"]
_ASSIGN_MODE_OPTIONS = ["Poly 1", "Poly 2", "Unison 1", "Unison 2"]
_KBD_TRACK_OPTIONS = ["0", "1/4", "1/2", "Full"]
_POLARITY_OPTIONS = ["Positive", "Negative"]
_MG_WAVEFORM_OPTIONS = ["Triangle", "Sawtooth", "Inverse Saw", "Square"]
_ON_OFF_OPTIONS = ["On", "Off"]

DW8000_PARAMETER_SPECS = [
    {"index": 0, "name": "Osc 1 Octave", "type": "choice", "choices": _OCTAVE_OPTIONS},
    {"index": 1, "name": "Osc 1 Wave Form", "type": "choice", "choices": _WAVEFORM_OPTIONS},
    {"index": 2, "name": "Osc 1 Level", "max": 31},
    {"index": 3, "name": "Auto Bend Select", "type": "choice", "choices": _AUTO_BEND_SELECT_OPTIONS},
    {"index": 4, "name": "Auto Bend Mode", "type": "choice", "choices": _AUTO_BEND_MODE_OPTIONS},
    {"index": 5, "name": "Auto Bend Time", "max": 31},
    {"index": 6, "name": "Auto Bend Intensity", "max": 31},
    {"index": 7, "name": "Osc 2 Octave", "type": "choice", "choices": _OCTAVE_OPTIONS},
    {"index": 8, "name": "Osc 2 Wave Form", "type": "choice", "choices": _WAVEFORM_OPTIONS},
    {"index": 9, "name": "Osc 2 Level", "max": 31},
    {"index": 10, "name": "Osc 2 Interval", "type": "choice", "choices": _INTERVAL_OPTIONS},
    {"index": 11, "name": "Osc 2 Detune", "max": 6},
    {"index": 12, "name": "Noise Level", "max": 31},
    {"index": 13, "name": "Assign Mode", "type": "choice", "choices": _ASSIGN_MODE_OPTIONS},
    {"index": 14, "name": "Default Parameter", "max": 62},
    {"index": 15, "name": "Cutoff", "max": 63},
    {"index": 16, "name": "Resonance", "max": 31},
    {"index": 17, "name": "VCF Keyboard Tracking", "type": "choice", "choices": _KBD_TRACK_OPTIONS},
    {"index": 18, "name": "VCF Envelope Polarity", "type": "choice", "choices": _POLARITY_OPTIONS},
    {"index": 19, "name": "VCF Env Intensity", "max": 31},
    {"index": 20, "name": "VCF Env Attack", "max": 31},
    {"index": 21, "name": "VCF Env Decay", "max": 31},
    {"index": 22, "name": "VCF Env Break Point", "max": 31},
    {"index": 23, "name": "VCF Env Slope", "max": 31},
    {"index": 24, "name": "VCF Env Sustain", "max": 31},
    {"index": 25, "name": "VCF Env Release", "max": 31},
    {"index": 26, "name": "VCF Velocity Sensitivity", "max": 7},
    {"index": 27, "name": "VCA Env Attack", "max": 31},
    {"index": 28, "name": "VCA Env Decay", "max": 31},
    {"index": 29, "name": "VCA Env Break Point", "max": 31},
    {"index": 30, "name": "VCA Env Slope", "max": 31},
    {"index": 31, "name": "VCA Env Sustain", "max": 31},
    {"index": 32, "name": "VCA Env Release", "max": 31},
    {"index": 33, "name": "VCA Velocity Sensitivity", "max": 7},
    {"index": 34, "name": "Modulation Wave Form", "type": "choice", "choices": _MG_WAVEFORM_OPTIONS},
    {"index": 35, "name": "Modulation Frequency", "max": 31},
    {"index": 36, "name": "Modulation Delay", "max": 31},
    {"index": 37, "name": "Modulation Osc", "max": 31},
    {"index": 38, "name": "Modulation VCF", "max": 31},
    {"index": 39, "name": "Pitch Bend Oscillators", "max": 12},
    {"index": 40, "name": "Pitch Bend VCF", "type": "choice", "choices": _ON_OFF_OPTIONS},
    {"index": 41, "name": "Delay Time", "max": 7},
    {"index": 42, "name": "Delay Factor", "max": 15},
    {"index": 43, "name": "Delay Feedback", "max": 15},
    {"index": 44, "name": "Delay Frequency", "max": 31},
    {"index": 45, "name": "Delay Intensity", "max": 31},
    {"index": 46, "name": "Delay Effect Level", "max": 15},
    {"index": 47, "name": "Portamento", "max": 31},
    {"index": 48, "name": "Aftertouch Osc Modulation", "max": 3},
    {"index": 49, "name": "Aftertouch VCF Modulation", "max": 3},
    {"index": 50, "name": "Aftertouch VCA Modulation", "max": 3},
]

def name():
    return "Korg DW 8000"


def createDeviceDetectMessage(channel):
    # Page 5 of the service manual - Device ID Request
    return [0xf0, 0x42, 0x40 | (channel % 16), 0xf7]


def deviceDetectWaitMilliseconds():
    return 100


def needsChannelSpecificDetection():
    return True


def generalMessageDelay():
    return 75


def channelIfValidDeviceResponse(message):
    # Page 3 of the service manual - Device ID
    if (len(message) > 3
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30  # Device ID
            and message[3] == 0x03):  # DW-8000
        return message[2] & 0x0f
    return -1


def createEditBufferRequest(channel):
    # Page 5 - Data Save Request
    return [0xf0, 0x42, 0x30 | (channel % 16), 0x03, 0x10, 0xf7]


def isEditBufferDump(message):
    if isLegacyFormat(message):
        return True
    # Page 3 - Data Dump
    return (len(message) > 4
            and message[0] == 0xf0
            and message[1] == 0x42  # Korg
            and (message[2] & 0xf0) == 0x30  # Format, ignore MIDI Channel in lower 4 bits
            and message[3] == 0x03  # DW-8000
            and message[4] == 0x40  # Data Dump
            )


def numberOfBanks():
    return 1


def numberOfPatchesPerBank():
    return 64


def convertToEditBuffer(channel, message):
    if isLegacyFormat(message):
        return convertFromLegacyFormat(channel, message)
    elif isEditBufferDump(message):
        return message[0:2] + [0x30 | channel] + message[3:]
    raise Exception("This is not an edit buffer - can't be converted")


def friendlyProgramName(program):
    return "%d%d" % (program // 8 + 1, (program % 8) + 1)


def program_change(channel, program_number):
    return [0xc0 | (channel & 0x0f), program_number]


def createStoreToProgramMessage(channel, program_number):
    # 0x11 is the WRITE_REQUEST message
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x03, 0x11, program_number & 0x3f, 0xf7]


def createProgramDumpRequest(channel, patchNo):
    return program_change(channel, patchNo) + createEditBufferRequest(channel)


def isSingleProgramDump(message):
    # Single Program Dumps are just edit buffer dumps
    return isEditBufferDump(message)


def convertToProgramDump(channel, message, program_number):
    if isLegacyFormat(message):
        message = convertFromLegacyFormat(channel, message)
    if isEditBufferDump(message):
        return message + createStoreToProgramMessage(channel, program_number)
        #return program_change(channel, program_number) + message + createStoreToProgramMessage(channel, program_number)
    raise Exception("Can only convert edit buffers!")


def isLegacyFormat(message):
    return len(message) == 51


def convertFromLegacyFormat(channel, message):
    return [0xf0, 0x42, 0x30 | (channel & 0x0f), 0x03, 0x40] + message + [0xf7]


def convertToLegacyFormat(message):
    return message[5:-1]


def calculateFingerprint(message):
    # To be backwards compatible with the old C++ implementation of the Korg DW 8000 implementation,
    # calculate the fingerprint only across the data block
    if not isLegacyFormat(message):
        message = message[5:-1]
    return hashlib.md5(bytearray(message)).hexdigest()


def isDefaultName(patchName):
    return re.match("[1-8][1-8]", patchName) is not None


def _parameter_payload(data):
    if not data:
        return []
    payload = list(data)
    if isLegacyFormat(payload):
        return payload[:]
    if len(payload) >= 7 and payload[0] == 0xf0:
        if payload[1] == 0x42 and payload[4] == 0x40:
            return payload[5:-1]
    # If the caller passed a larger message that contains the edit buffer somewhere in the middle (e.g., after a program‑dump prefix),
    # this scans for a F0 42 xx 03 40 sequence, finds the matching F7, and extracts the data between them.
    for start in range(len(payload) - 6):
        if (payload[start] == 0xf0 and payload[start + 1] == 0x42
                and payload[start + 4] == 0x40):
            for end in range(start + 6, len(payload)):
                if payload[end] == 0xf7:
                    return payload[start + 5:end]
            break
    return []


def _map_parameter_value(raw_value, spec):
    if spec.get("type") == "choice":
        choices = spec["choices"]
        if 0 <= raw_value < len(choices):
            return choices[raw_value]
    return raw_value


@cache
def parameterDefinitions():
    definitions = []
    for spec in DW8000_PARAMETER_SPECS:
        definition = {
            "param_id": spec["index"],
            "name": spec["name"],
            "description": spec.get("description", spec["name"]),
        }
        if spec.get("type") == "choice":
            definition["param_type"] = "choice"
            definition["values"] = list(spec["choices"])
        else:
            definition["param_type"] = "value"
            definition["values"] = [spec.get("min", 0), spec["max"]]
        definitions.append(definition)
    return definitions


def parameterValues(patch, onlyActive):
    del onlyActive
    payload = _parameter_payload(patch)
    if len(payload) < len(DW8000_PARAMETER_SPECS):
        return []
    values = []
    for spec in DW8000_PARAMETER_SPECS:
        index = spec["index"]
        if index >= len(payload):
            break
        raw_value = payload[index]
        values.append({
            "param_id": index,
            "value": _map_parameter_value(raw_value, spec),
        })
    return values


def make_test_data():
    def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = [x for sublist in data.all_messages[0:2] for x in sublist]
        yield testing.ProgramTestData(message=patch)

    assert friendlyProgramName(0) == "11"
    return testing.TestData(sysex="testData/KorgDW8000_bank_a.syx", edit_buffer_generator=programs, friendly_bank_name=(63, "88"), expected_patch_count=64)
