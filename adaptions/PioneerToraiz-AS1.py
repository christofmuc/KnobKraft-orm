#
#   Copyright (c) 2020 Christof Ruch. All rights reserved.
#
#   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

def name():
	return "Pioneer Toraiz AS-1"

def createDeviceDetectMessage(channel):
	# See page 33 of the Toraiz AS-1 manual
	return [0xf0, 0x7e, 0x7f, 0b00000110, 1, 0xf7]

def deviceDetectWaitMilliseconds():
	return 200

def needsChannelSpecificDetection():
	return False

def channelIfValidDeviceResponse(message):
	# The manual states the AS1 replies with a 15 byte long message, see page 33
	if (len(message) == 15 
		and message[0] == 0xf0    # Sysex
		and message[1] == 0x7e    # Non-realtime
		# ignore message[2] - that's the current midi channel
		and message[3] == 0b0110  # Device request
		and message[4] == 0b0010  # Reply
		and message[5] == 0b00000000  # Pioneer ID byte 1
		and message[6] == 0b01000000  # Pioneer ID byte 2
		and message[7] == 0b00000101  # Pioneer ID byte 3
		and message[8] == 0b00000000  # Toriaz ID byte 1
		and message[9] == 0b00000000  # Toriaz ID byte 2
		and message[10] == 0b00000001  # Toriaz ID byte 3
		and message[11] == 0b00001000  # Toriaz ID byte 4
		and message[12] == 0b00010000  # Device ID
		): 
		# This is indeed the right package, now extract the MIDI channel from the message
		if message[2] == 0x7f:
			# The Toraiz is set to OMNI. Not a good idea, but let's treat this as channel 1 for now
			return 1
		else:
			return message[2]
	return -1

def createEditBufferRequest(channel):
	# See page 34 of the Toraiz manual
	return [0xf0, 0b00000000, 0b01000000, 0b00000101, 0b00000000, 0b000000000, 0b00000001, 0b00001000, 0b00010000, 0b00000110, 0xf7]

def isEditBufferDump(message):
	# see page 35 of the manual
	return (len(message) > 9
		and message[0] == 0xf0 
		and message[1] == 0b00000000  # Pioneer ID byte 1 
		and message[2] == 0b01000000  # Pioneer ID byte 2
		and message[3] == 0b00000101  # Pioneer ID byte 3
		and message[4] == 0b00000000  # Toriaz ID byte 1
		and message[5] == 0b00000000  # Toriaz ID byte 2
		and message[6] == 0b00000001  # Toriaz ID byte 3
		and message[7] == 0b00001000  # Toriaz ID byte 4
		and message[8] == 0b00010000  # Device ID
		and message[9] == 0b00000011) # Edit Buffer Dump

def numberOfBanks():
	return 10

def numberOfPatchesPerBank():
	return 100

def createProgramDumpRequest(channel, patchNo):
	# Calculate bank and program - the KnobKraft Orm will just think the patches are 0 to 999, but the Toraiz needs a bank number 0-9 and the patch number within that bank
	bank = patchNo / numberOfPatchesPerBank()
	program = patchNo % numberOfPatchesPerBank()
	# See page 33 of the Toraiz manual
	return [0xf0, 0b00000000, 0b01000000, 0b00000101, 0b00000000, 0b000000000, 0b00000001, 0b00001000, 0b00010000, 0b00000101, bank, program, 0xf7]

def isSingleProgramDump(message):
	# see page 34 of the manual
	return (len(message) > 9
		and message[0] == 0xf0 
		and message[1] == 0b00000000  # Pioneer ID byte 1 
		and message[2] == 0b01000000  # Pioneer ID byte 2
		and message[3] == 0b00000101  # Pioneer ID byte 3
		and message[4] == 0b00000000  # Toriaz ID byte 1
		and message[5] == 0b00000000  # Toriaz ID byte 2
		and message[6] == 0b00000001  # Toriaz ID byte 3
		and message[7] == 0b00001000  # Toriaz ID byte 4
		and message[8] == 0b00010000  # Device ID
		and message[9] == 0b00000010) # Program Dump

def nameFromDump(message):
	#TODO this has to be implemented still, for that we need the unpacking of the MIDI data first, and do not have documentation of the patch format. Should be easy to find
	return "AS-1 Patch"

def convertToEditBuffer(message):
	if isEditBufferDump(message):
		return message
	if isSingleProgramDump(message):
		# To turn a single program dump into an edit buffer dump, we need to remove the bank and program number, and switch the command to 0b00000011
		return message[0:9] + [0b00000011] + message[12:]
	raise "Data is neither edit buffer nor single program buffer from Toraiz AS-1"
