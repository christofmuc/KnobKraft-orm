#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

def name():
	return "Matrix 1000 generic"

def numberOfBanks():
	return 10

def numberOfPatchesPerBank():
	return 100

def createDeviceDetectMessage(channel):
	# This is a sysex generic device detect message
	return [0xf0, 0x7e, channel, 0x06, 0x01, 0xf7]

def deviceDetectWaitMilliseconds():
	return 200

def needsChannelSpecificDetection():
	return True

def channelIfValidDeviceResponse(message):
	# The Matrix 1000 answers with 15 bytes
	if (len(message) == 15 
		and message[0] == 0xf0  # Sysex
		and message[1] == 0x7e  # Non-realtime
		and message[3] == 0x06  # Device request
		and message[4] == 0x02  # Device request reply
		and message[5] == 0x10  # Oberheim
		and message[6] == 0x06  # Matrix
		and message[7] == 0x00
		and message[8] == 0x02  # Family member Matrix 1000
		and message[9] == 0x00
		):
		# Extrat the current MIDI channel from index 2 of the message
		return message[2]
	return -1

def createProgramDumpRequest(channel, patchNo):
	return [0xf0, 0x10, 0x06, 0x04, 1, patchNo, 0xf7]

def isSingleProgramDump(message):
	return (len(message) > 3 
		and message[0] == 0xf0 
		and message[1] == 0x10   # Oberheim
		and message[2] == 0x06   # Matrix
		and message[3] == 0x01)  # Single Patch Data
