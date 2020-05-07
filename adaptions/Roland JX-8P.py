#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

# Note that for real life usage the native C++ implementation of the Matrix1000 is more powerful and should be used
# This is an example adaption to show how a fully working adaption can look like

def name():
	return "Roland JX-8P"

def createDeviceDetectMessage(channel):
	# The JX-8P cannot be detected - at all. There does not seem to be a single MIDI message we can send to it that it
	# will reply to. Send a meaningless empty Sysex to make our code work
	return [0xf0, 0xf7]

def deviceDetectWaitMilliseconds():
	return 10

def needsChannelSpecificDetection():
	return False

def channelIfValidDeviceResponse(message):
	# Return invalid channel (-1)
	return -1

def createEditBufferRequest(channel):
	# The JX-8P has no Edit Buffer Request, but sending this will make the Orm wait for a reply
	return [0xf0, 0xf7]

def isEditBufferDump(message):
	return (len(message) > 6
		and message[0] == 0xf0  # Sysex
		and message[1] == 0x41  # Roland ID
		and message[2] == 0b00110101  # APR All tone parameters
		and message[4] == 0b00100001  # JX-8P
		and message[5] == 0b00100000  # Level
		and message[6] == 0b00000001  # Group
		)

def numberOfBanks():
	return 1

def numberOfPatchesPerBank():
	return 1

def createProgramDumpRequest(channel, patchNo):
	# The JX-8P has no Program Buffer Request, but sending this will make the Orm wait for a reply
	return [0xf0, 0xf7]

def isSingleProgramDump(message):
	return isEditBufferDump(message)

def nameFromDump(message):
	if isSingleProgramDump(message):
		return ''.join([chr(x) for x in message[7:17]])

def convertToEditBuffer(channel, message):
	# The JX-8P only has edit buffer messages (called All Tone Parameters). 
	# To turn it into a program dump you need to append a "program number message"
	# But we need to poke the channel into position 3
	message[3] = channel if channel != -1 else 0
	return message
