#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import json
import re


# This uses information from https://docs.electra.one/developers/midiimplementation.html
from typing import List

import testing


def name():
    return "Electra One"


def createDeviceDetectMessage(channel):
    # Send an Electa One "Get Electra Info Request" to detect the Electra One
    return [0xF0, 0x00, 0x21, 0x45, 0x02, 0x7F, 0xF7]


def needsChannelSpecificDetection():
    return False


def channelIfValidDeviceResponse(message):
    if (len(message) > 5
            and message[0] == 0xf0  # Sysex
            and message[1] == 0x00
            and message[2] == 0x21
            and message[3] == 0x45  # Electra manufacturer ID
            and message[4] == 0x01  # Data dump
            and message[5] == 0x7f):  # Electra information
        # Parse version information just to show we can
        try:
            data = presetToJson(message)
            if isinstance(data, dict):
                print("Electra One identifies with version %s and serial #%s" % (data["versionText"], data["serial"]))
        except json.JSONDecodeError as error:
            print("Found Electra One but with invalid version information, error is %s" % error.msg)
        # The Electra One does not use MIDI Channel or Sysex ID to communicate, so we just return MIDI channel 1
        return 1
    return -1


def createEditBufferRequest(channel):
    # This requests the current, active preset
    return [0xF0, 0x00, 0x21, 0x45, 0x02, 0x00, 0xF7]


def isEditBufferDump(message):
    # The length is arbitrarily chosen - the Electra One sometimes sends out just empty preset dump messages?
    return (len(message) > 10
            and message[0] == 0xf0
            and message[1] == 0x00
            and message[2] == 0x21
            and message[3] == 0x45  # Electra manufacturer ID
            and message[4] == 0x01  # Data dump
            and message[5] == 0x00  # Preset file
            )


def numberOfBanks():
    # The documentation mentions there are 12 preset slots
    return 12


def numberOfPatchesPerBank():
    return 1


def nameFromDump(message):
    if isEditBufferDump(message):
        jsonBlock = message[6:-1]
        if isinstance(jsonBlock, list):
            jsonString = ''.join([chr(x) for x in jsonBlock])
            try:
                presetInfo = json.loads(jsonString)
                if isinstance(presetInfo, dict):
                    return presetInfo["name"]
                return "Empty patch"
            except json.JSONDecodeError:
                # That is non valid JSON, maybe an extra comma, let's try to regex the name then
                found = re.search("\"name\"\\s*:\\s*\"([^\"]*)\"", jsonString)
                if found is None:
                    return "JSON Error"
                return found.group(1)
    return "Invalid"


def renamePatch(message, newName):
    if isEditBufferDump(message):
        try:
            presetAsJson = presetToJson(message)
            presetAsJson["name"] = newName
            return jsonToPreset(presetAsJson)
        except json.JSONDecodeError as error:
            # Don't print this as I currently call rename way too often.
            print("Can only rename valid JSON, the preset may be corrupted: %s" % error.msg)
            return message
    raise Exception("Can only rename Electra One preset dumps")


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("This is not an Electra One preset dump - can't be converted")


def presetToJson(message):
    jsonBlock = message[6:-1]
    jsonString = ''.join([chr(x) for x in jsonBlock])
    # This throws a json.JSONDecodeError in case the preset is not valid JSON (which could happen when hand-tweaking)
    return json.loads(jsonString)


def jsonToPreset(json_data):
    jsonString = json.dumps(json_data, separators=(',', ':'))
    dataBlock = [ord(x) for x in list(jsonString)]
    return [0xF0, 0x00, 0x21, 0x45, 0x01, 0x00] + dataBlock + [0xf7]


def stringToPreset(jsonString):
    dataBlock = [ord(x) for x in list(jsonString)]
    return [0xF0, 0x00, 0x21, 0x45, 0x01, 0x00] + dataBlock + [0xf7]


def make_test_data():
    defect = [240, 0, 33, 69, 1, 0, 53, 247]
    nameFromDump(defect)

    # Test parse errors
    invalid_json = '{  "version": 2,  "name" :"ROLAND MKS-80 v3",\n   "data":{  \r\n },  ]}'
    testCrash = list(stringToPreset(invalid_json))
    assert isEditBufferDump(testCrash)
    name_from_corrupt = nameFromDump(testCrash)
    assert name_from_corrupt == "ROLAND MKS-80 v3"
    not_renamed = renamePatch(testCrash, "do crash")
    assert nameFromDump(not_renamed) == "ROLAND MKS-80 v3"

    def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
        with open(R"testData/elektraOne-demo-preset.syx", mode="rb") as preset:
            content = preset.read()
            yield testing.ProgramTestData(message=list(content), name="tx-81z", rename_name="betterName")

        with open(R"testData/elektraOne-corrupted-preset.syx", mode="rb") as preset:
            content = preset.read()
            # This cannot be renamed, as the JSON is not parsable. Legacy problems in the ElectraOne editor
            yield testing.ProgramTestData(message=list(content), name="ROLAND MKS-80 v3", dont_rename=True)

    return testing.TestData(program_generator=programs)
