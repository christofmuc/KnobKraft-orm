#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

def name():
	return "Pioneer Toraiz AS-1"

def numberOfBanks():
	return 10

def numberOfPatchesPerBank():
	return 100

def createDeviceDetectMessage(channel):
	return [0xf0, 0x7e, 0x7f, 0b0110, 1, 0xf7]

def deviceDetectWaitMilliseconds():
	return 200

def needsChannelSpecificDetection():
	return False

def channelIfValidDeviceResponse(message):
	# The manual states the AS1 replies with a 15 byte long message
	if len(message) == 15 and message[0] == 0xf0 and message[1] == 0x7e:
		# Goodness, the Toraiz sends quite a large packge we need to check
		expected = [0b0110, 0b0010, 0, 0b01000000, 0b0101, 0, 0, 1, 0b1000 ]
		for i in range(len(expected)):
			if expected[i] != message[i + 3]:
				# Mismatch, this is not from the AS1
				return -1
		# If we reached this, all bytes of the message are as expected, we can extract
		# the real channel of the AS1 from index 2 of the message
		if message[2] == 0x7f:
			# The Toraiz is set to OMNI. Not a good idea, but let's treat this as channel 1 for now
			return 1
		else:
			return message[2]
	return -1
