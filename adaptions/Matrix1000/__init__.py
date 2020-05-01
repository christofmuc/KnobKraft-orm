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
	if len(message) == 15 and message[0] == 0xf0 and message[1] == 0x7e:
		# Goodness, the Toraiz sends quite a large packge we need to check
		expected = [0x06, 
			0x02, 
			0x10,  # Oberheim
			0x06,  # Matrix
			0x00, 
			0x02,  # Family member Matrix 1000
			0x00
			]
		for i in range(len(expected)):
			if expected[i] != message[i + 3]:
				# Mismatch, this is not from the AS1
				return -1
		# If we reached this, all bytes of the message are as expected, we can extract
		# the real channel from index 2 of the message
		return message[2]
	return -1
