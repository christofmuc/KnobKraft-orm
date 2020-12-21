#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#
import json
# This uses information from https://docs.electra.one/developers/midiimplementation.html


def name():
    return "Electra One"


def createDeviceDetectMessage(channel):
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
        # The Electra One does not use MIDI Channel or Sysex ID to communicate, so we just return MIDI channel 1
        return 1
    return -1


def createEditBufferRequest(channel):
    # This requests the current, active preset
    return [0xF0, 0x00, 0x21, 0x45, 0x02, 0x00, 0xF7]


def isEditBufferDump(message):
    return (len(message) > 5
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
        jsonString = ''.join([chr(x) for x in jsonBlock])
        presetInfo = json.loads(jsonString)
        return presetInfo["name"]
    return "Invalid"


def renamePatch(message, newName):
    if isEditBufferDump(message):
        presetAsJson = presetToJson(message)
        presetAsJson["name"] = newName
        return jsonToPreset(presetAsJson)
    raise Exception("Can only rename Electra One preset dumps")


def convertToEditBuffer(channel, message):
    if isEditBufferDump(message):
        return message
    raise Exception("This is not an Electra One preset dump - can't be converted")


def presetToJson(message):
    jsonBlock = message[6:-1]
    jsonString = ''.join([chr(x) for x in jsonBlock])
    return json.loads(jsonString)


def jsonToPreset(json_data):
    jsonString = json.dumps(json_data, separators=(',', ':'))
    dataBlock = [ord(x) for x in list(jsonString)]
    return bytearray([0xF0, 0x00, 0x21, 0x45, 0x01, 0x00] + dataBlock + [0xf7])


# Some test code that is not run by the KnobKraft Orm on load
if __name__ == '__main__':
    with open(R"D:\Christof\Music\ElectraOne\elektraOne-demo-preset.syx", mode="rb") as preset:
        content = preset.read()
        old_name = nameFromDump(content)
        print(old_name)
        same = renamePatch(content, old_name)
        assert same == content
        new = renamePatch(content, "betterName")
        print(nameFromDump(new))
