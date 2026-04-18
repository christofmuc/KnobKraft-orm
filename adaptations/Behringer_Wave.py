
##
## created by github/willxy
##
## references/sources:
##
##  - Behringer Wave User Manaul
##       https://www.behringer.com/behringer/product?modelCode=0722-ABD
##       https://cdn.mediavalet.com/aunsw/musictribe/uYwUFVaQ7Uij7Q0Thvt2Eg/uZjKUX3HB0SHV0T3k5XDoQ/Original/M_BE_0722-ABD_WAVE_WW.pdf
##
##  - SYNTHTRIBE app, observed midi traffic
##

##
##  Implementation notes:
##
##  - For patch names (as of Wave firmware 1.0.11):
##    - The Wave hardware does not provide an interface for viewing or
##      changing patch names
##    - The sysex provides for 16 characters of patch name
##    - When sending sysex to/from the Wave, the hardware appears to
##      discard the 16 chars for patch name.  If a patch sysex is sent to
##      the Wave containing a specific patch name, then when the patch
##      sysex is subsequently retrieved from the Wave, the patch name in
##      the sysex is always returned as "1111111111111111" (16 chars of '1').
##    - The User Manual provides a list of names for the factory patches
##      - That list includes also some settings for each patch (KEYB, wt#,
##        etc)
##      - It appears that data is incorrect for some patches (A95-A99 appear
##        to list the wrong wt#, B 5,8,9,11,22,24,72,75,78,79 appear to
##        have various difference in wt# or KEYB setting)
##
##  - To try and provide useful patchn ames, in this adaptation:
##    - If the sysex *does* include an actual patch name, that name is
##      returned (eg in case future firmware starts storing it)
##    - Else, if the patch's fingerprint matches that of a "known"/factory
##      patch, that name is returned (fingerprints calculated from the
##      1.0.11 firmware),
##    - Else, a name is generated based on patch data: wavetable name
##      (from the User Manual), and wavetable position for Main and Sub
##      osc in Group A
##    - In the case of fingerprint-matched patch name and generated patch
##      name, name is returned with a prefix of ".." to indicate that it's
##      a guessed/generated name (eg isDefaultName)
##    - Caveat: Several ranges of wavetables/transients are user-writeable
##      - Wavetables 64-86 have Factory waves and names, but are
##        user-writable; if those wavetables are replaced by the user, the
##        wavetable name in the generated patch name will no longer be
##        accurate
##      - Wavetables 49-63 and Transients 87-127 do not have Factory
##        names, so those will be returned as "UserWave49" through
##        "UserTrans127"
##

from typing import Optional, Generator, Any, cast

import hashlib
import re
import knobkraft
import testing


####  utility conversions  ####

## convert byte list to ascii text
def ascFromBytes(byteList: list[int]) -> str:
	return bytearray(byteList).decode()

## convert ascii text to list of bytes
## w/ optional right-space-padding of string
def bytesFromAsc(string: str, padlength=0) -> list[int]:
	return list(bytearray(string.ljust(padlength), 'utf-8'))


####  synth config  ####

NAME = 'Behringer Wave'

SETUP_HELP = """
	The Wave hardware does not provide an interface for viewing or changing patch name, and appears to not honor/persist patch names that are specified in the sysex.\n
	Patch names returned by the adaptation will thus be generated:\n
	- Either the factory patch name (from the manual) for un-modified patches (based on firmware 1.0.11 fingerprints), eg: "..FanWAVEia"\n
	- Or a patch name generated from the patch data: wavetable name and main/sub osc positions, eg: "..AscendSweep 36/11" or "..UserWave49 21/02"\n
	- And these patch names will begin with ".." to distinguish them as generated patch names, vs names entered by hand (or returned from actual sysex patch name data, if that ever gets implemented in a future firmware update)
"""

BANK_DESC = [
	{"bank": 0, "name": "A", "size": 100, "type": "Patch"},
	{"bank": 1, "name": "B", "size": 100, "type": "Patch"},
]

START = [0xf0]
END = [0xf7]

manufacturer_id = [0x00, 0x20, 0x32] ## Behringer
model_id = [0x00, 0x01, 0x39] ## Wave
device_id = [0x00] ## Default

default_init_name = '1111111111111111'
default_edit_name = '(Edit Buffer)'
generated_patchname_pattern = r'^[.][.].*$'

len_patchname = 16

default_patch_names = [
	default_init_name,
	default_edit_name,
]

default_patch_name_patterns = [
	generated_patchname_pattern,
]

offset_payload_wavetable = 16
offset_payload_keyb = 18
offset_payload_oscA_main = 41
offset_payload_oscA_sub = 42


#####  synth sysex message patterns  ####


### identity
## not in the manual, observed from watching SynthTribe traffic
msg_identify_req = START + manufacturer_id + model_id + device_id + [0x02] + END
msg_identify_match = START + manufacturer_id + model_id ## then 00 03 00 END


### patch request
## pkt=0x74, spkt=0x05 :: Program Preset Sound Parameters Dump Request
msg_patch_req  = START + manufacturer_id + model_id + device_id + [0x74] + [0x05] ## then bank, program, END
## pkt=0x74, spkt=0x06 :: Response for Program Preset Sound Parameters Dump Request
##      "        "     :: Dump to WAVE Preset Sound Parameters (and store to Preset#)
msg_patch_resp = START + manufacturer_id + model_id + device_id + [0x74] + [0x06] ## then bank#, prog#, vers, data, chksum, END
msg_patch_match = msg_patch_resp

### edit buf request
## pkg=0x74, spkt=0x07 :: Edit Buffer Sound Parameters Dump Request
msg_editbuf_req  = START + manufacturer_id + model_id + device_id + [0x74] + [0x07] + END
## pkg=0x74, spkt=0x08 :: Response for Edit Buffer Sound Parameters Dump Request
##     "         "     :: Dump Sound Parameters to WAVE Edit Buffer
msg_editbuf_resp = START + manufacturer_id + model_id + device_id + [0x74] + [0x08] ## then vers, data, chksum, END
msg_editbuf_match = msg_editbuf_resp


####  msg matching  ####


## for convenient "does the sysex message match this prefix" checking
## also supports taking a list of possible prefixes
def msg_match_prefix(message: list[int], prefix: list, exact=False) -> Optional[list[int]]:
	if exact:
		if message == prefix:
			return prefix
		return None
	if bytearray(message).startswith(bytearray(prefix)) and len(message) > len(prefix):
		return prefix
	return None


####  simple synth traits  ####


def needsChannelSpecificDetection() -> bool:
	return False


def name() -> str:
	return NAME

def setupHelp() -> str:
	return SETUP_HELP

def bankDescriptors() -> list[dict]:
	return BANK_DESC


def numberOfBanks() -> int:
	return len(bankDescriptors())

def numberOfPatchesPerBank() -> int:
	return bankDescriptors()[0]['size']


## 'A' => BANK_DESC row
def bankFromName(bank_name: str) -> Optional[dict]:
	return next((x for x in bankDescriptors() if x['name'] == bank_name), None)

## 0 => BANK_DESC row
def bankFromNumber(bank_number: int) -> Optional[dict]:
	return next((x for x in bankDescriptors() if x['bank'] == bank_number), None)


def bankSelect(channel:int , bank: int) -> list[int]:
	return [0xb0 | (channel & 0x0f), 32, bank]


def isDefaultName(patchname: str) -> bool:
	if patchname in default_patch_names:
		return True
	for patn in default_patch_name_patterns:
		if re.fullmatch(patn, patchname):
			return True
	return False


####  bank/patch numbers  ####


## 0 => A
## 1 => B
def friendlyBankName(bank_number: int) -> Optional[str]:
	## bankDescriptors().find(x => x.bank==bank_number)?.name
	return next((x['name'] for x in bankDescriptors() if x['bank'] == bank_number), None)


## convert OrmPatchNo to patch location string similar to shown on the LCD
## (Wave actually uses bank '0' and '1', but this adaptation used 'A' and 'B')
## 0 => A00
## 1 => A01
## 99 => A99
## 100 => B01
## 199 => B99
def friendlyProgramName(OrmPatchNo: int) -> str:
	if OrmPatchNo < 0:
		## edit buffer
		return default_edit_name
	maxNo = sum(x['size'] for x in bankDescriptors() if x['type'] == 'Patch') -1
	if OrmPatchNo > maxNo:
		raise Exception("Program# %d exceeds max %d" % (OrmPatchNo, maxNo))
	n_per_bank = bankDescriptors()[0]["size"]
	bank = OrmPatchNo // n_per_bank
	slot = OrmPatchNo % n_per_bank
	program_name = '%s%02d' % (friendlyBankName(bank), slot) ## 0-based
	return program_name


## convert patch data's program location string to OrmPatchNo
## data => 0..199
## A00 => 0
## A99 => 99
## B00 => 100
## B99 => 199
def numberFromDump(message: list[int]) -> int:
	if isSingleProgramDump(message):
		offset = len(msg_patch_req)
		bank = message[offset]
		program = message[offset+1]
		return bank * numberOfPatchesPerBank() + program
	return -1


####  simple sysex request msgs  ####


def createDeviceDetectMessage(__channel__: int) -> list[int]:
	return msg_identify_req


def channelIfValidDeviceResponse(message: list[int]) -> int:
	if (match := msg_match_prefix(message, msg_identify_match)):
		channel = message[len(match)]
		print("%s valid response, \"channel\": %d" % (NAME, channel))
		return channel
	return -1


def createEditBufferRequest(__channel__: int) -> list[int]:
	return msg_editbuf_req


def createProgramDumpRequest(__channel__: int, OrmPatchNo: int) -> list[int]:
	bank = [OrmPatchNo // numberOfPatchesPerBank()]
	program = [OrmPatchNo % numberOfPatchesPerBank()]
	return msg_patch_req + bank + program + END


####  extracting/modifying data in sysex dumps  ####


## return the payload of patch data from the message, to make it easier to
## use the sysex patch data byte indexes that are in the manual
def payloadFromDump(message):
	## skip preamble, and strip chksum byte and terminating f7
	if isEditBufferDump(message):
		## len(resp) plus vers
		offset = len(msg_editbuf_resp) + 1
		return message[offset:-2]
	if isSingleProgramDump(message):
		## len(resp) plus bank#, prog#, vers
		offset = len(msg_patch_resp) + 3
		return message[offset:-2]
	raise Exception("Neither edit buffer nor program dump; can't get patch data")


####  naming of patches  ####


## if the patch data has an actual non-init name, return that
## else, if patch matches the fingerprint of a known patch, return that
## else, generate a name from the patch data
def nameFromDump(message: list[int]) -> str:
	patch_data = payloadFromDump(message)
	patch_name = ''.join(map(chr, patch_data[0:len_patchname]))
	if patch_name != default_init_name:
		return patch_name
	fpatch = knownPatchName(message)
	if fpatch is not None:
		return '..%s' % fpatch
	return generatePatchName(message)


def knownPatchName(message: list[int]) -> Optional[str]:
	fp = calculateFingerprint(message)
	return KnownPatches.get(fp)


def generatePatchName(message: list[int]) -> str:
	patch_data = payloadFromDump(message)
	wt = patch_data[offset_payload_wavetable] ## wavetable number
	wname = FactoryWavetables.get(wt)
	if wname is None:
		if 49 < wt < 64:
			wname = 'UserWave%s' % wt
		elif 86 < wt < 128:
			wname = 'UserTrans%s' % wt
		else:
			wname = 'User%s' % wt
	## osc position is displayed as 0-63 but stored as 0-127
	main = patch_data[offset_payload_oscA_main] // 2 ## main oscA wt position
	sub = patch_data[offset_payload_oscA_sub] // 2 ## sub oscA wt position
	gen_name = '..%s %s/%s' % (wname, main, sub)
	return gen_name


## The hardware doesn't actually store the new name, so it seems better to
## not provide the rename function.  Also note, this implementation is old
## and may not be correct now.
#def renamePatch(message, name):
#	## "16x ASCII Character Bytes, Range 32-126"
#	valid = all(ord(x) >= 32 and ord(x) <= 126 for x in name)
#	if not valid:
#		raise Exception("Can't rename patch; invalid name: %s" % name)
#	name = name[0:16] ## truncate to 16 chars
#	pad_name = name.ljust(16)
#	array_name = [ord(x) for x in pad_name]
#	if isEditBufferDump(message):
#		new = message[0:11] + array_name + message[27:]
#		return rebuildChecksum(new)
#	if isSingleProgramDump(message):
#		new = message[0:13] + array_name + message[29:]
#		return rebuildChecksum(new)
#	raise Exception("Neither edit buffer nor program dump; can't be converted")


####  identifying patch/editbuf dumpes  ####


def isEditBufferDump(message):
	return bool(msg_match_prefix(message, msg_editbuf_match))

def isSingleProgramDump(message):
	return bool(msg_match_prefix(message, msg_patch_match))


####  converting patch/editbuf dumpes  ####


def convertToEditBuffer(__channel__, message):
	if isEditBufferDump(message):
		return message
	if isSingleProgramDump(message):
		## replace the msg_patch_resp + bank# + prog#
		## with msg_editbuf_resp
		## len(resp) plus bank#, prog#
		offset = len(msg_patch_resp) + 2
		return msg_editbuf_resp + message[offset:]
	raise Exception("Neither edit buffer nor program dump; can't be converted")


def convertToProgramDump(__channel__, message, program_number):
	bank = program_number // numberOfPatchesPerBank()
	program = program_number % numberOfPatchesPerBank()
	if isEditBufferDump(message):
		## replace the msg_editbuf_resp
		## with msg_patch_resp + bank# + prog#
		offset = len(msg_editbuf_resp)
		return msg_patch_resp + [bank] + [program] + message[offset:]
	if isSingleProgramDump(message):
		## len(resp) plus bank#, prog#
		offset = len(msg_patch_resp) + 2
		return msg_patch_resp + [bank] + [program] + message[offset:]
	raise Exception("Neither edit buffer nor program dump; can't be converted")


####  chksums and fingerprints  ####


## "CheckSum = (u8)(D0+...+Dn)&0x7F"
def calculateChecksum(message: list[int]) -> int:
	patch_data = payloadFromDump(message)
	chksum = sum(patch_data) & 0x7f
	return chksum


def validateChecksum(message: list[int]) -> bool:
	oldsum = message[-2]
	newsum = calculateChecksum(message)
	return newsum == oldsum


def rebuildChecksum(message: list[int]) -> list[int]:
	chksum = calculateChecksum(message)
	new_msg = message
	new_msg[-2] = chksum
	return new_msg


## patch data (no bank/prog) and also no patch name
def blankedOut(message):
	patch_data = payloadFromDump(message)
	data = patch_data[len_patchname:]
	return data


def calculateFingerprint(message):
	data = blankedOut(message)
	return hashlib.md5(bytearray(data)).hexdigest()


####  more utility conversions  ####


## convert byte list to 2-char-byte string
## same result as syxToString()
def strFromBytes(byteList: list[int]) -> str:
	return bytearray(byteList).hex(' ')

## convert 2-char-byte string to list of bytes
## same result as stringToSyx
def bytesFromStr(string: str) -> list[int]:
	return list(bytearray.fromhex(string))


## convert byte list to comma-separated string of hex strings
## eg: "0x31, 0x32, 0x33"
def hexstrFromBytes(byteList: list[int]) -> str:
	return ', '.join([format(val, '#04x') for val in byteList])

## convert comma-separated string of hex strings to list of bytes
def bytesFromHexstr(string: str) -> list[int]:
	return list(bytearray.fromhex(string.replace('0x','').replace(',','')))


## convert single byte to string of single-hex
def hexstrFromByte(val: int) -> str:
	return format(val, '#04x')

## convert string of single-hex to byte
def byteFromHexstr(string: str) -> int:
	return int(string, 16)


## convert byte list to string of binary/bits
def binstrFromBytes(byteList: list[int]) -> str:
	return ' '.join([format(val, '08b') for val in byteList])

## convert (space separated) string of binary/bits to list of bytes
def bytesFromBinstr(string: str) -> list[int]:
	return [int(s, 2) for s in string.split()]


## convert single byte to string of binary/bits
def binstrFromByte(val: int) -> str:
	return format(val, '08b')

## convert string of binary single-byte to byte
def byteFromBinstr(string: str) -> int:
	return int(string, 2)


####  wavetable names  ####


FactoryWavetables: dict[int, str] = {

	## Preset wavetables
	0: 'HmcBliss',
	1: 'HmcBliss2',
	2: 'Bells',
	3: 'WindSweep',
	4: 'Churchy',
	5: 'HmcMixBlend',
	6: 'WoodwindSweep',
	7: 'SylvanSweep',
	8: 'PercuStrings',
	9: 'VoxEnsemble',
	10: 'Reso',
	11: 'Chiaroscuro',
	12: 'SweepingHigh',
	13: 'HmcStrike',
	14: 'OrganicRegisters',
	15: 'HmcSaw',
	16: 'SweepFrenzy',
	17: 'SpectrumShift',
	18: 'HmcSweep',
	19: 'ElectricP',
	20: 'Robotic',
	21: 'StrongHrm',
	22: 'EchoScape',
	23: 'HighH',
	24: 'AscendSweep',
	25: 'EchoColor',
	26: 'BrassForm',
	27: 'VoxForm',
	28: 'PhaseString',
	29: 'PulseShift',
	30: 'UpperWave',

	## Preset transients
	31: 'Piano/Sax', ## missing from the manual
	32: 'PanFlute',
	33: 'Pizzi',
	34: 'SteelGuitar',
	35: 'ElecCello',
	36: 'Xylophon',
	37: 'Harp',
	38: 'Pizzagogo',
	39: 'SoftBellEP',
	40: 'MediumEP',
	41: 'Trumpet',
	42: 'MuteTrumpet',
	43: 'Duuhh',
	44: 'PipeBrass',
	45: 'HammerHarp',
	46: 'CathChoir',
	47: 'ChoirVerse',
	48: 'BreathyVox',

	## User-writable transients
	## ..49-63

	## Factory wavetables in user-writable slots
	64: 'PipesXfade',
	65: 'PD_Saw',
	66: 'DX_Tubular',
	67: 'DX_E.Guitar',
	68: 'DX_FullTine',
	69: 'Vibe-rant',
	70: 'DX_Bass',
	71: 'StratoSweep',
	72: 'FormantPad',
	73: 'FairVox',
	74: 'SpaceSweep',
	75: 'SoundTrack',
	76: 'PWMstringer',
	77: 'Hoover',
	78: 'PD_Saw',
	79: 'PD_SQU',
	80: 'PD_TPL',
	81: 'PD_DBL',
	82: 'PD_SPL',
	83: 'PD_RES',
	84: 'WurliWave',
	85: 'Rhodos',
	86: 'Vorcode',

	## User-writable wavetables
	## ..87-127
}


####  factory patch list  ####


KnownPatches: dict[str, str] = {

	## some general patches
	'93c3436154f8e804d03d3c410072c4cb': 'INIT PATCH',

	## Factory Bank 0/A
	'6de4caf05f7c55a242c372b13758f0d3': "Thingie", ## A00
	'ceecf67f27f6045ab65f3f9f3901ff55': "Space Sweep", ## A01
	'59f9c722e30e0b3546b770f412112130': "Dynamic Tines", ## A02
	'fafcd0e72da6113d39a6d464e30af34a': "Bass & Juno Sweep", ## A03
	'7a7d3931d4036c2eb76653c9262412bb': "Dynamic Prophet Brass", ## A04
	'88768b779f2338d249f1c5400c6f89d1': "Stratos", ## A05
	'22af4b7d70ad3639cec0fd1ab96b4018': "Bass & Voco Trem", ## A06
	'ca0eee0346f0760e5ed0524135c1c5f6': "Wurli One", ## A07
	'8bf412fc2db45566880523785768acdf': "Jazz Guitar", ## A08
	'1f7744d56b1a27d099c67f5a8e7bd046': "Rotary B3", ## A09
	'6bfd78325e60b7e04f6896a4c7a0e245': "Bass & PWM Strings", ## A10
	'ef0dfe55e50786a97b62effebb950e11': "Tomita's Bass", ## A11
	'85f2f0f1779c047d04c414c073038e7f': "Church Pipes", ## A12
	'c2f02b5f9caf24854a2ff18debd2a909': "Dream Vibes", ## A13
	'b034341c6e2a8bb6fa6a2068bf5fbd87': "Fretless & Tines", ## A14
	'19192ee0b06e4e8fd5a5c8d5ea97714c': "BellTasia", ## A15
	'adb3b6ef2b85c12375ab4bd7f9e2f69b': "Tubular", ## A16
	'fe9300d557d7676f2b06f4c23d1b9aec': "CZ Sawer", ## A17
	'093c3b65f066e5499c348663872f5d34': "FM Brass", ## A18
	'7c5702d0395bfff42144d9b48a53bf3b': "WAVE Waves", ## A19
	'4aae3c874772337358b89869fb8a0149': "FairVox", ## A20
	'd6611f3cf670e603da2ffa5b6646cf99': "PanFlute", ## A21
	'9f237617a6d47f795b616843d781a30d': "Pizzicato", ## A22
	'806332c87bb9e3e3d17cf8ada26b1c1a': "Steel Guitar", ## A23
	'45da953cf93142d02c93bfc9041b9c32': "Xylophone", ## A24
	'd4a91a27bc916c54ed33b10da6e4a0ea': "Sail Away", ## A25
	'55430e011eb1823f33848f1834ec9edf': "PWMstringer", ## A26
	'39ee59d8a47b6ecf10651189294d4f9a': "Soft Bell EP", ## A27
	'0c27fd867da2ac30a13834a288cf20e8': "Dusty Rhodos", ## A28
	'79cd19966d74258224052b0eac2ac43b': "Solo Trumpet", ## A29
	'fd2a52361e2956a4e51ef8a15e933ff1': "Mute Trumpet", ## A30
	'dd8d42b9755e58274182fe8d74633294': "Duuuh", ## A31
	'87e5414d6f627445ae229dbb95e817e0': "Bass & Wurli", ## A32
	'b87797124df92ea196bf74406de23c90': "Sync Solo", ## A33
	'db8b1c00c334bf9b697b382daf4d0b33': "Synth Flute", ## A34
	'a402626ed545b64e818643454b29bf3f': "Alto Sax", ## A35
	'1fae35927c00094fb1825a061ab9d48c': "Retro Split", ## A36
	'fc10909477a0d47479876a6818539829': "Strings'n Solo", ## A37
	'c0dc9e9cb0cf0395b3a7aebf27bd8d27': "Matrix Strings", ## A38
	'286c047413ed2bc5d59ec80138b63059': "Alpha Pad", ## A39
	'e8547ed8f74b291a5f280266edd3c463': "Cosmic Soup", ## A40
	'c377579b6ed1cdfc5b323749cf1cf5cf': "Harpoon", ## A41
	'a4217c226ce36ef762eec168428184de': "Square Solo", ## A42
	'f560e033ce2740fb6d9200af761b4b67': "Fantasia", ## A43
	'f36119c4e89dc4eb655ac6e4e0e2de2b': "Synth Pizzi", ## A44
	'2540749a044faef7beea62e0a86ef799': "Super Saw", ## A45
	'0c55fcce9737166cc7ee02bfeec721ea': "Organ Chariot", ## A46
	'c64a567a07861e9af59e382585a95fc1': "Noise Siren", ## A47
	'a224f5f320eecbe55751a62b1f5bddcc': "Piano X", ## A48
	'10d3e86e953bb339c4f552617e2daf73': "Percussion", ## A49
	'b031ea6ececa03d919396630ed11bfbb': "Drone Harmonica", ## A50
	'4e8353d1a447ac9ce47219d817df4e83': "FanWAVEia", ## A51
	'b8f8ff6f01734e22d3f435844fd66203': "Lyle's Solo", ## A52
	'ed2dd819ae0cf00195209bcc4d5550e4': "FM Bass1", ## A53
	'877a8445bb94a5b009e3da8571265847': "PWM Pad", ## A54
	'32fa19078cf974164f036a8f9fd4f73f': "Solina 1", ## A55
	'83511abb753d5c2f03c2ab5b856c7397': "808 Bass", ## A56
	'4d24e0f66abf9c3c6dd9807e9bd14a6f': "SYNC", ## A57
	'f33a384f130070b2f1958ffd9a2a2e42': "WAVE SeqSplit", ## A58
	'96dae82b52b6ffb5a1d5992b46b338ba': "Synth Cello", ## A59
	'ade221464da8bb06b82ab55348d6f43e': "SoundTrack", ## A60
	'f14ad47c9796947878255b1f6cab8b06': "Solead", ## A61
	'40f17c90d408d20213366d27e872cd69': "LA Strings", ## A62
	'ab60fa88fda88a84cad5165f7e8e28ea': "Crystal Waves", ## A63
	'20b6172e0f9a79697c9c3cb008606c7c': "80s Rig", ## A64
	'5c2ac39802bda20249aba5a2939bc79c': "JunPad & CV-Bass", ## A65
	'753a2c2d5f4d7115e221aab6e36b17a1': "Fendi Rhodos", ## A66
	'749ef5c9969d5130946f784b53c19817': "Funky Wurli", ## A67
	'5dcbee2042aaf4d0b9187a5b5bf0c652': "Arpeggio PAD", ## A68
	'270b3c1b5a170764415b99b98f977bb2': "Ding Ding", ## A69
	'2f8af4216062e407a39afdbb3ddddfa6': "Universal", ## A70
	'a63fd7230bcbf771b57873f98388e693': "Pipe Brass", ## A71
	'd7cc55ce4fae071d24265aa550e8fcf9': "Big Score", ## A72
	'7af3a26ad28baedeb679592628ab9f79': "Breathy Vox", ## A73
	'2fc5168b35f1cff7f065f361910552b4': "Hammer Harp", ## A74
	'ccde201b3cce0f6ea921f1b7b739e80d': "Spring Pad", ## A75
	'035ddbbafe3e284dc01c768e83c8efe0': "Octave Bell", ## A76
	'361f768fc10a7fbf79d5d37e01cfd6ec': "Yoga Temple", ## A77
	'9ba6760f9985d441be4f6d1f66eadf9f': "Steel Drum", ## A78
	'91542780e9ae59e082fbd4cf1b432d18': "Short Bell", ## A79
	'363ebceda9cd55b4cd9870909b22422f': "Square Arp", ## A80
	'25e53bbc6d2d78f73548392b4dc58b9e': "Reso Harp", ## A81
	'ff1cc6d259d316d443a1202ccd067a9a': "Organic Synth", ## A82
	'd304324258919d4524a85fe8c83d3db0': "Steel Organ", ## A83
	'b3cbe5d81703d8a640986aca77902d5a': "Robotic", ## A84
	'3d5b8d35d956b83b44062c042b0a77c0': "Reso Phase", ## A85
	'99fdc0fa22b2e912b26b2efdfb56f04c': "Arcade Pad", ## A86
	'9fe2bfe5f1b4cfa2c573d8e2a8dfe610': "PWM Solo", ## A87
	'd95abbaabd9eeeb1290eaa79726e3d8d': "VCF SSM2044", ## A88
	'e4bd9b297477c221ccd4b2cc13decfc3': "ARP Lead", ## A89
	'c636c4e291d3b4b9be3476381112b7e9': "Spring Pad", ## A90
	'432e3096284915ecb760024796e952ad': "Reso Dive Organ", ## A91
	'6a12ef19877c20e6f5da107aca897a8e': "ArpEratus", ## A92
	'0156e1c3f12db4f517c9b8fc4bf697b1': "EquiSAW", ## A93
	'69f43658a0954219b99781c3ad099c6c': "Woody SQU", ## A94
	'b2af709b1467fcdae87bd5d2d83c5947': "Wave Chase", ## A95
	'94980be58872a25dca7ae07bc9907033': "Poly Bass", ## A96
	'09a80897b762de76599d6ed8d66b0a24': "Wave Slide", ## A97
	'e4526d4a43108e10a2806e15a933c017': "Landscape", ## A98
	'79d05d8bea78604f41c7df50917c3486': "Formant Drop", ## A99

	## Factory Bank 1/B
	'd57ab491e8b05a38e937773dd17111b7': "WAVE Swirl", ## B00
	'6080e1e83a6238bed3cacf34ff232b0e': "Modulated Vibraphone", ## B01
	'9fd6c59b061dff4dbc353bd2258670c9': "Harmonic Glide", ## B02
	'4947e7471809678d244e831a14d7de44': "WAVE Tone 16", ## B03
	'6cdb4a0dbd6239b0943dc98f30a0123c': "Resonant Extended Synth", ## B04
	'fcff4144ed2ea927a28e595b2e27225c': "Belli Pad", ## B05
	'b41608a2cdff87d9f5aa1686b511dafe': "KW Organ", ## B06
	'0a895a20bba2d6c161081d50249e4c39': "Waow!", ## B07
	'7c4deb1a01aee62a50a7b041bcbdcc14': "Organ 1", ## B08
	'996166955ae6fb490b20ca458cbd01b0': "Organ 2", ## B09
	'6b7502b897d4f64624b152f10d36f69c': "Bellgan", ## B10
	'6f291c2016aa9524fbcafba2100a2c72': "Wave Organ", ## B11
	'52caabb2260e253be1bd2160d5c8ffb4': "Filter Mod", ## B12
	'e57bbcd7bbdb6bc5986a8d4784222d6c': "WAVE Sitar", ## B13
	'9a3d7f3892e46cb06ae0bbacd6d554a1': "Sawyer", ## B14
	'a933f245b554b8df1f4c6724c67c25e7': "StringPan", ## B15
	'74ff6b9b3c4218d4e63ff40f8d1e31e1': "Inspired by Fender Keys", ## B16
	'99f0ea9a70027efb76dc582321221a22': "Inspired by Electric Keys", ## B17
	'2a61dfbf9a69230fa393691a1a733ca1': "Env Sync", ## B18
	'7c6473b4159ecb8635f167bfe5607400': "Punchy with Quinte", ## B19
	'e69b5d2b5da957df23aa1ba9935c667c': "Wave Shaping", ## B20
	'3798f2e1644b81eaeed4452539afaa17': "Concert Piano", ## B21
	'd08ab43a3d8ad2c69a9f11238ee3deb1': "Brass Bel", ## B22
	'de671655e18b6d79b9ad46dcea2f77d7': "Punchy", ## B23
	'c9bc80c790fc14e74d0c0c39d5f94e94': "Dynamic Spinet", ## B24
	'153910f8d83e50f34169d958f2ce1592': "Bubble Square", ## B25
	'2eb3c0c76ec2b54a64c1ef3bcb4e217c': "Cosmic Spinet", ## B26
	'6e7104719cfb8fe907fa9d4ccdbc43e8': "Upright Piano", ## B27
	'd17380e4c56ef54636d10a609d6a2e83': "Citrus", ## B28
	'dd1b82f63f2481fbdfb2ee0a3f57536a': "Wave Bypass", ## B29
	'd8228edb117889d2dd4518bbe17c2f58': "Synth", ## B30
	'b8de9e27f32d3317bf914fd67c99e527': "Pulse Width Synth", ## B31
	'cbe76f75e226a202792107e6d7275be9': "Resonant Synth Attack", ## B32
	'cd74a332086332766b20b2968be0dbc6': "Tinkle", ## B33
	'8a5a83d9c11ef488b6df64faf97b47ce': "Sawtooth Extended Synth", ## B34
	'28f454803909f4b20277b8913d8ffe4a': "Punchy with Resonance", ## B35
	'bbd202839be86b3fe46407c8295d3a5e': "Punchy Metal", ## B36
	'655d233fa61b2335bbb6951ee48be029': "Punchy Chime", ## B37
	'a3ff81c555ed3a6cea1dd6180475e0e4': "Electric 12-String", ## B38
	'228ef739685cd51e9c26cb871c02cd43': "Chime", ## B39
	'2557dcae4a932377bc19a65033c2c77f': "Long Space Chime", ## B40
	'bebc3da16f9c0969c4030d3f7796a045': "Chime-like", ## B41
	'e5590084cc13678c825f49b9657bf346': "Delicate Punch", ## B42
	'a52e569d0784fcd4e6f146dc01ba70ac': "Mallet Keys", ## B43
	'67d02a087f2643fe3c110b9c4d7d1a10': "Vibrating Keys", ## B44
	'cfc2e961ea1780f0d4a6b7b86f4362be': "Punchy 3", ## B45
	'718de8285238911ad71614b440f4678d': "Saxophone", ## B46
	'b6216600782b0a9898086c404b2067e1': "Brass Section 1", ## B47
	'886ec5a588a00a74b79a7c008dfd4fb2': "Trumpet Section", ## B48
	'6442dacd1dafe1121b0e45643f0b1942': "Perc./Extended", ## B49
	'68761bcca8d22e9a6b51750b67bf793a': "Deep Brass", ## B50
	'75aebeab5085c312451c9471a8cc6315': "Trombone Section", ## B51
	'8e4e02a20d5ed856e4f094f78efb1a9b': "Brass Ensemble", ## B52
	'29f45b5ade7c81177b4fba0e979ded75': "Blended Brass", ## B53
	'4f27c688e1d6b910979a1499d290815a': "High Register Flute", ## B54
	'e9608b75226dd86ae2ddb85cb5301294': "Woodwind Flute", ## B55
	'221f6dc31d194fe4ce1b9ad7072d2187': "String Ensemble 1", ## B56
	'23bdb16d3fe56b44c1866a102dc11259': "String Ensemble 2", ## B57
	'309dd8fb9d4ee0acffea8ff8ffddc805': "Vox Ocean", ## B58
	'7ddb470136179a81172f695be523e834': "ChoirTasia", ## B59
	'ea1f57e9dbc243cb5a11c9971118fd89': "Ethereal Strings", ## B60
	'8be554788a08b440cd14f3a4c76072d0': "Ethereal Strings", ## B61
	'33b1b819c40d67a33e290ec5e85729ff': "Poly with Sync FX", ## B62
	'cedf8046f3ed99b67e5be0f2fa24c198': "Echo Bell Pad", ## B63
	'8406797a29e909f5face9f5e99d901aa': "Reed Instrument", ## B64
	'abd5f67b645adbb5861900c82adce3d2': "Concert Piano + Saxophone", ## B65
	'635b353b5c33a8b7b135b3c823cba90c': "Studio Organ", ## B66
	'e36b1a402bd0c7a5476bf150f3097b0c': "Cathedral Organ", ## B67
	'fef417c73fc5835e1e4c758203899a17': "Orgish Bell", ## B68
	'd04ba1f2adbf5e2984aec0df1ea5603b': "High Perc", ## B69
	'61287e81b5851d6dc1bd430dc03de68d': "Perc", ## B70
	'af177ebe6f713919cb5aaa298ef28197': "Organ 3", ## B71
	'd6e77c89dbbd735a9598d12a22f44d4c': "Organ 1 Redux", ## B72
	'2956dbb73161562f454bf78204e7366e': "Raw Choir", ## B73
	'12e6f3277f618a2cc737d34836842bf0': "Blended Choir", ## B74
	'53d21ccc82d3e0f40df0e1bff28fe6bb': "Ethereal Choir", ## B75
	'c9874957e3d2de22ff256abca3306725': "Ethereal Choir 2", ## B76
	'0df27bcedd7996ba407cf3ef4291483d': "FX", ## B77
	'93f6f9c8d37a35d6c7cfb7084680e881': "Space Poly", ## B78
	'01f05a251d550d9fa478b1827ee476e5': "FX Poly", ## B79
	'eae61ae2839d77bf5b6c19a733404934': "Sync-stortion", ## B80
	'cb070970a77a5fe002223a8f732b0abe': "Elec Piano", ## B81
	'b02b7bf474b585d706ff1e0143ee0f52': "Poly Pad", ## B82
	'3d475fef24dc987736602fdeb7652d87': "Funk Saw", ## B83
	'ec5db824336793ccfd03b8bcb4c01876': "Simple Organ", ## B84
	'b73b5e9ab327390c7b94a5ba7138b7ed': "Glockenspiel", ## B85
	'e747c06e6f556085033bc649053c4c7d': "Harp-une", ## B86
	'03fa15a1ddfd44961739fe2187b5107e': "Chain Saw Bass", ## B87
	'bcc2c57f60228ca46676f04dbd6e15bc': "Soundcard FM", ## B88
	'a0cec8fb7b223462a8df83e34378f358': "Full Keys", ## B89
	'd22a1ef11e34062087a10f5dda547471': "Concert Piano Strings", ## B90
	'8d6e802184ec569651de2dc4ad5a5665': "Concert Piano with Wave Tone", ## B91
	## dupe: 'abd5f67b645adbb5861900c82adce3d2': "Concert Piano with Sax", ## B92, duplicate of B65 "Concert Piano + Saxophone"
	'0eb865c073410889f42b1dff32ee7367': "Perc for Sequencer", ## B93
	'a6ec587530ebdd956f956f799222e275': "Delay", ## B94
	'5aa8917ab3b108b7743afba2d6a2a955': "Pitch Shift", ## B95
	'45d6d62b8e9a7e208ee6e90207cde7a1': "FX", ## B96
	'e080fd4a55936d7b39a57b22d17388ce': "Quirky", ## B97
	'ac6e23abe6461305a193e8f4cce2266e': "Delay Effect", ## B98
	'2e41ea0f4ddb1d1368ae1ff23ea47429': "Sample & Hold", ## B99

}


####  test harness  ####


## Test routine picked up by test_adaptation.py
def make_test_data():	## pylint: disable=too-many-statements, too-many-branches
	import random		## pylint: disable=import-outside-toplevel
	rand_bytes = list(random.randbytes(256))
	rand_byte = rand_bytes[0]
	rand_asc = ''.join([chr(random.randint(32, 126)) for _ in range(256)])

	## test byte conversion functions
	assert bytesFromStr(strFromBytes(rand_bytes)) == rand_bytes
	assert bytesFromHexstr(hexstrFromBytes(rand_bytes)) == rand_bytes
	assert byteFromHexstr(hexstrFromByte(rand_byte)) == rand_byte
	assert bytesFromBinstr(binstrFromBytes(rand_bytes)) == rand_bytes
	assert byteFromBinstr(binstrFromByte(rand_byte)) == rand_byte
	assert all(32 <= x <= 126 for x in bytesFromAsc(strFromBytes(rand_bytes))) is True
	assert all(32 <= x <= 126 for x in bytesFromAsc(hexstrFromBytes(rand_bytes))) is True
	assert all(32 <= x <= 126 for x in bytesFromAsc(binstrFromBytes(rand_bytes))) is True
	assert all(32 <= x <= 126 for x in bytesFromAsc((rand_asc))) is True

	assert friendlyBankName(0) == 'A'
	assert friendlyBankName(1) == 'B'
	assert friendlyProgramName(0) == 'A00'
	assert friendlyProgramName(1) == 'A01'
	assert friendlyProgramName(199) == 'B99'
	assert friendlyProgramName(-100) == '(Edit Buffer)'
	assert isDefaultName('(Edit Buffer)') is True
	assert isDefaultName('1111111111111111') is True
	assert isDefaultName('..lkajsldkfjkdlj') is True
	assert isDefaultName('asdf') is False
	assert isDefaultName('') is False

	## test msg_match_prefix
	assert msg_match_prefix([1,2,3,4,5], [1,2,3]) == [1,2,3]
	assert msg_match_prefix([1,2,3,4,5], [1,2,3], exact=True) is None
	assert msg_match_prefix([1,2,3,4,5], [1,2,4]) is None

	## test sysex msgs
	assert createDeviceDetectMessage(0) == bytesFromStr("f0 00 20 32 00 01 39 00 02 f7")
	assert channelIfValidDeviceResponse(bytesFromStr("f0 00 20 32 00 01 39 00")) == 0
	assert channelIfValidDeviceResponse([1,2,3]) == -1
	assert createProgramDumpRequest(0, 199) == bytesFromStr("f0 00 20 32 00 01 39 00 74 05 01 63 f7")

	## test some exceptions
	try:
		friendlyProgramName(200)
	except Exception as err:	## pylint: disable=broad-exception-caught
		print('Error: %s' % err)
		assert 'exceeds max' in str(err)
	else:
		assert False

	#def programs(data: testing.TestData) -> list[testing.ProgramTestData]:
	#def programs(data: testing.TestData) -> Generator[testing.ProgramTestData, Any, Any]:
	#def programs(data: testing.TestData) -> Generator[list[testing.ProgramTestData]]:
	#def programs(data: testing.TestData) -> testing.test_data.ProgramGenerator:
	#def programs(data: testing.TestData) -> Generator:
	def programs(data: testing.TestData) -> Generator[testing.ProgramTestData, Any, Any]:

		patchData = data.all_messages[0]

		## test parts of dump
		assert isSingleProgramDump(patchData) is True
		assert isEditBufferDump(patchData) is False
		assert isSingleProgramDump([1,2,3,4,5]) is False

		## test chksum validation
		temp_data = patchData
		assert validateChecksum(temp_data) is True
		new_data = convertToEditBuffer(0, temp_data)
		assert calculateChecksum(new_data) == calculateChecksum(temp_data)

		new_data[17] = 0xff
		assert calculateChecksum(new_data) != calculateChecksum(temp_data)
		assert validateChecksum(new_data) is False

		temp_data = patchData
		temp_data[-2] = 0
		assert validateChecksum(temp_data) is False

		## test patch/editbuf conversion, blankedOut, fingerprint
		temp_data = patchData
		editbuf_data = convertToEditBuffer(0, temp_data)
		assert blankedOut(temp_data) == blankedOut(editbuf_data)
		assert calculateFingerprint(temp_data) == calculateFingerprint(editbuf_data)
		OrmPatchNo = numberFromDump(temp_data)
		new_data = convertToProgramDump(0, editbuf_data, OrmPatchNo)
		assert new_data == temp_data

		## test petches
		yield testing.ProgramTestData(message=data.all_messages[0], name='..WAVE Swirl', number=100, friendly_number='B00')
		yield testing.ProgramTestData(message=data.all_messages[7], name='..Waow!', number=107, friendly_number='B07')
		sysexPatch1 = "f0002032000139007406002a00666f6f6261722020202020202020202011000003010101000c0c0c0c0c0c0c0c000034005800582800000000486c00007e4004387c480000000204000100040000000000000003010101000c0c0c0c0c0c0c0c00003430487e381600000000183418007e20044c7c6c0000000204000100010000000000000059f7"
		yield testing.ProgramTestData(message=sysexPatch1, name="foobar          ", number=42, friendly_number="A42")


	def edit_buffers(__data__: testing.TestData) -> Generator[testing.ProgramTestData, Any, Any]:

		editbufData = cast(list[int], knobkraft.load_sysex("testData/Behringer_Wave/modpatch.syx", as_single_list=True))
		assert nameFromDump(editbufData) == '..SpaceSweep 12/0'

		## test parts of dump
		assert isEditBufferDump(editbufData) is True
		assert isSingleProgramDump(editbufData) is False
		assert isEditBufferDump([1,2,3,4,5]) is False
		assert numberFromDump(editbufData) == -1

		## test conversion
		assert convertToEditBuffer(0, editbufData) == editbufData
		patch_data = convertToProgramDump(0, editbufData, 142)
		assert numberFromDump(patch_data) == 142
		assert convertToEditBuffer(0, convertToProgramDump(0, editbufData, 42)) == editbufData

		## test editbuf 1
		editbufData = cast(list[int], knobkraft.load_sysex("testData/Behringer_Wave/editbuf.syx", as_single_list=True))
		yield testing.ProgramTestData(message=editbufData, name='..Thingie', number=-1, friendly_number='(Edit Buffer)')

		## an init patch
		editbufData = cast(list[int], knobkraft.load_sysex("testData/Behringer_Wave/initpatch.syx", as_single_list=True))
		yield testing.ProgramTestData(message=editbufData, name='..INIT PATCH', number=-1, friendly_number='(Edit Buffer)')

		editBuffer1 = "f000203200013900740800666f6f6261722020202020202020202011000003010101000c0c0c0c0c0c0c0c000034005800582800000000486c00007e4004387c480200000204000100040000000000000003010101000c0c0c0c0c0c0c0c00003430487e381600000000183418007e20044c7c6c0000000204000100010000000000000032f7"
		yield testing.ProgramTestData(message=editBuffer1, name="foobar          ", friendly_number="(Edit Buffer)", number=-1)


	return testing.TestData(
		program_generator=programs,
		edit_buffer_generator=edit_buffers,
		device_detect_call="f0 00 20 32 00 01 39 00 02 f7",
		device_detect_reply=("f0 00 20 32 00 01 39 00", 0),
		program_dump_request=(0, 42, "f0 00 20 32 00 01 39 00 74 05 00 2a f7"),
		friendly_bank_name=(1, 'B'),
        sysex="testData/Behringer_Wave/bankB.syx",
		expected_patch_count=100,
	)

## pylint: disable=too-many-lines
