#   Yamaha SY77/TG77 voice adaptation contributed by Giovanni Candiago.
#   Based on the KnobKraft Yamaha reface DX adaptation.
#   Live MIDI workflows were tested on a TG77.
import hashlib

# English: Model ID and specific headers for the SY/TG77 series
# Italiano: Model ID e intestazioni specifiche per la serie SY/TG77
TG77_MODEL_ID = 0x7A
TG77_LM_HEADER = [0x4C, 0x4D, 0x20, 0x20, 0x38, 0x31, 0x30, 0x31]  # ASCII "LM  8101"

# English: Mapping of TG77 Voice Types from index 32 (byte 33, F0 included)
# Italiano: Mappatura dei tipi di voce TG77 dall'indice 32 (byte 33, F0 incluso)
TG77_VOICE_TYPES = {
    0x00: "1AFM mono",
    0x01: "2AFM mono",
    0x02: "4AFM mono",
    0x03: "1AFM",
    0x04: "2AFM",
    0x05: "1AWM",
    0x06: "2AWM",
    0x07: "4AWM",
    0x08: "1AFM+1AWM",
    0x09: "2AFM+2AWM",
    0x0A: "Drum set"
}

# English: Header of a VOICE bulk dump, indices 6-15: ASCII "LM  8101VC".
# English: Other bulk dumps (Multi MU, Pan PN, Micro tuning MT, System SY, Additional multi "8104MU", Program change
# English: table "8104PC") share the same layout but have a different header: they must NOT be taken for voices.
# Italiano: Intestazione di un bulk dump VOICE, indici 6-15: ASCII "LM  8101VC".
# Italiano: Gli altri bulk dump (Multi MU, Pan PN, Micro tuning MT, System SY, Multi aggiuntivo "8104MU", Tabella
# Italiano: program change "8104PC") hanno la stessa struttura ma un'intestazione diversa: NON vanno scambiati per voci.
TG77_VOICE_DUMP_ID = TG77_LM_HEADER + [0x56, 0x43]   # "LM  8101" + "VC"
TG77_SYSTEM_DUMP_ID = TG77_LM_HEADER + [0x53, 0x59]  # "LM  8101" + "SY" (reply to the device detect)

# English: Voice types that can ONLY be stored in bank D (slots 0x30-0x3F, voices 49-64).
# English: 0x02=4AFM mono, 0x07=4AWM, 0x09=2AFM+2AWM (the three "large" voice types, confirmed by the user)
# English: and 0x0A=Drum set (MIDI DATA FORMAT, voice bulk dump table: Memory# $30-$3F).
# English: In the edit buffer any voice type can be loaded.
# Italiano: Tipi voce salvabili SOLO nel banco D (slot 0x30-0x3F, voci 49-64).
# Italiano: 0x02=4AFM mono, 0x07=4AWM, 0x09=2AFM+2AWM (le tre voci "grandi", confermato dall'utente)
# Italiano: e 0x0A=Drum set (MIDI DATA FORMAT, tabella voice bulk dump: Memory# $30-$3F).
# Italiano: Nell'edit buffer si puo caricare qualsiasi tipo di voce.
TG77_DBANK_ONLY_TYPES = {0x02, 0x07, 0x09, 0x0A}

# English: Bank D starts at slot 0x30 (voice 49). Larger voices are restricted to these slots.
# Italiano: Il banco D inizia allo slot 0x30 (voce 49). Le voci grandi possono essere salvate solo qui.
TG77_BANK_D_START = 0x30


# ---------------------------------------------------------------------------
# Internal helpers  (prefix _ = not called by KnobKraft directly)
# ---------------------------------------------------------------------------

def _slotToLabel(slot):
    # English: Convert raw slot 0x00-0x3F to TG77 display label e.g. 0x00->"A01", 0x2F->"C16"
    # English: Single 64-voice internal memory divided in sub-banks:
    # English:   A = 0x00-0x0F  voices 01-16
    # English:   B = 0x10-0x1F  voices 17-32
    # English:   C = 0x20-0x2F  voices 33-48
    # English:   D = 0x30-0x3F  voices 49-64
    # Italiano: Converte slot grezzo 0x00-0x3F in etichetta display TG77 es. 0x00->"A01"
    bank_letter = "ABCD"[slot >> 4]   # nibble alto 0-3 -> A B C D
    position    = (slot & 0x0F) + 1   # nibble basso 0-15 -> 1-16
    return f"{bank_letter}{position:02d}"

def _isBulkDump(message, dump_id, minimum_size):
    return (
        len(message) >= minimum_size
        and message[0] == 0xF0
        and message[1] == 0x43
        and 0 <= message[2] <= 0x0F
        and message[3] == TG77_MODEL_ID
        and message[-1] == 0xF7
        and list(message[6:16]) == dump_id
        and (message[4] << 7) + message[5] == len(message) - 8
        and all(0 <= byte < 0x80 for byte in message[1:-1])
        and message[-2] == calculateChecksum(message[6:-2])
    )


def _isVoiceDump(message):
    # English: True only for a TG77/SY77 VOICE bulk dump: F0 43 0n 7A, byte count, "LM  8101VC".
    # English: Memory type (index 30) is checked by the callers: 0x7F edit buffer, 0x00 internal memory.
    # Italiano: Vero solo per un bulk dump VOICE TG77/SY77: F0 43 0n 7A, byte count, "LM  8101VC".
    # Italiano: Il tipo memoria (indice 30) lo controllano i chiamanti: 0x7F edit buffer, 0x00 memoria interna.
    return _isBulkDump(message, TG77_VOICE_DUMP_ID, 45)


# The system dump reports the voice receive channel for the responding SysEx device number.
_voice_receive_channels = {}


def _deviceNibble(channel):
    # English: KnobKraft channel is 0-based (0..15) = Device number 1..16 = low nibble of the Device ID byte.
    # English: Anything invalid falls back to device number 1 (nibble 0).
    # Italiano: Il canale di KnobKraft e 0-based (0..15) = Device number 1..16 = nibble basso del byte Device ID.
    # Italiano: Un valore non valido torna al device number 1 (nibble 0).
    if channel is not None and 0 <= channel <= 15:
        return channel & 0x0F
    return 0


# ---------------------------------------------------------------------------
# KnobKraft required functions
# ---------------------------------------------------------------------------

def name():
    return "Yamaha SY77-TG77"

def setupHelp():
    return (
        "Voice support was tested on the TG77. SY77 hardware and writes to internal memory still need testing. "
        "For Program change send mode on the TG77, set Program change mode to Normal. "
        "Run detection again after changing the Voice Receive Channel. "
        "To write to internal memory, turn off Bulk Protect on the synth. "
        "4AFM mono, 4AWM, 2AFM+2AWM and Drum Set voices require a slot from D01 to D16."
    )


def createDeviceDetectMessage(channel):
    return [0xF0, 0x43, 0x20 | _deviceNibble(channel), TG77_MODEL_ID] + TG77_SYSTEM_DUMP_ID + [0] * 16 + [0xF7]


def needsChannelSpecificDetection():
    return True


def generalMessageDelay():
    # English: Pause in ms between the messages of ONE call (edit buffer request: EDIT mode + dump request +
    # English: VOICE mode). Tested on the TG77: the biggest voice type (4AFM, 1564 bytes, about 0.66 s to be
    # English: transmitted) needs at least 600 ms; with 400 ms the instrument stays in the edit window.
    # English: 800 = tested value + margin. Calls that return a single message (program dump request,
    # English: program dump, edit buffer send) are not affected, so the 64 voices download is not slower.
    # Italiano: Pausa in ms tra i messaggi di UNA chiamata (richiesta edit buffer: modo EDIT + dump request +
    # Italiano: modo VOICE). Provato sul TG77: la voce piu grande (4AFM, 1564 byte, circa 0,66 s di trasmissione)
    # Italiano: richiede almeno 600 ms; con 400 ms lo strumento resta nella finestra edit.
    # Italiano: 800 = valore provato + margine. Le chiamate con un solo messaggio (richiesta program dump,
    # Italiano: program dump, invio edit buffer) non sono toccate, quindi il download delle 64 voci non rallenta.
    return 800

def channelIfValidDeviceResponse(message):
    # English: The reply to the identity request is the System bulk dump "LM  8101SY".
    # English: Device ID byte (index 2) is 0x0n with n = Device number - 1, so the channel is its low nibble.
    # English: Returns a 0-based channel; KnobKraft adds 1 internally.
    # Italiano: La risposta alla identity request e il bulk dump System "LM  8101SY".
    # Italiano: Il byte Device ID (indice 2) e 0x0n con n = Device number - 1, quindi il canale e il suo nibble basso.
    # Italiano: Restituisce un canale 0-based; KnobKraft aggiunge 1 internamente.
    if not _isBulkDump(message, TG77_SYSTEM_DUMP_ID, 83):
        return -1
    device_number = message[2]
    voice_channel = message[80]
    # 16 means omni. Fall back to the detected device number in that case.
    _voice_receive_channels[device_number] = voice_channel if voice_channel < 16 else device_number
    return device_number


def createCustomProgramChange(channel, patchNo):
    # English: KnobKraft "Program change" send mode: recalls a voice already in the TG77 internal memory.
    # English: Program change 0-63 = voices 1-64 (TG77 "Program change mode" must be Normal). It goes on the
    # English: voice receive channel read at detect (VRCH), NOT on the device number channel; if unknown or omni,
    # English: the detected channel is used.
    # Italiano: Modo di invio "Program change" di KnobKraft: richiama una voce gia presente nella memoria interna del TG77.
    # Italiano: Program change 0-63 = voci 1-64 (il "Program change mode" del TG77 deve essere Normal). Va sul canale di
    # Italiano: ricezione voci letto al rilevamento (VRCH), NON sul canale del device number; se sconosciuto o omni
    # Italiano: si usa il canale rilevato.
    device_number = _deviceNibble(channel)
    midi_channel = _voice_receive_channels.get(device_number, device_number)
    return [0xC0 | midi_channel, patchNo & 0x3F]

def numberOfBanks():
    # English: One logical bank = the single 64-voice internal memory
    # Italiano: Un banco logico = la memoria interna unica a 64 voci
    return 1

def numberOfPatchesPerBank():
    return 64

def friendlyBankName(bank):
    # English: Only one bank (numberOfBanks() == 1): the internal memory, shown as A01-D16 in friendlyProgramName()
    # Italiano: Un solo banco (numberOfBanks() == 1): la memoria interna, mostrata come A01-D16 in friendlyProgramName()
    return "Internal"

def numberFromDump(message):
    # English: Memory number (index 31) of an internal memory dump. Bit 6 is ignored by the TG77 on reception.
    # English: -1 for anything that is not a program dump (e.g. the edit buffer has no memory number).
    # Italiano: Numero di memoria (indice 31) di un dump di memoria interna. Il bit 6 e ignorato dal TG77 in ricezione.
    # Italiano: -1 per tutto cio che non e un program dump (es. l'edit buffer non ha numero di memoria).
    if isSingleProgramDump(message):
        return message[31] & 0x3F
    return -1

def friendlyProgramName(programNo):
    if programNo == 0x7F:
        return "Edit voice"
    slot  = programNo & 0x3F
    label = _slotToLabel(slot)
    return f"{programNo + 1:02d}-{label}"

def friendlyEditBufferName():
    return "Edit voice"

def isDefaultName(patch_name):
    clean_name = patch_name.strip()
    return clean_name == "Init Voice" or clean_name == ""

def nameFromDump(message):
    # English: 10 ASCII name bytes at indices 33-42 (bytes 34-43 counting from F0)
    # Italiano: 10 byte ASCII nome agli indici 33-42 (byte 34-43 contando da F0)
    if len(message) >= 43:
        name_bytes = message[33:43]
        return "".join([chr(x) if 32 <= x <= 126 else " " for x in name_bytes]).strip()
    return "New Patch"

def renamePatch(message, new_name):
    data = list(message)
    if len(data) > 44:
        for i in range(10):
            character = new_name[i] if i < len(new_name) else " "
            data[33 + i] = ord(character) if " " <= character <= "~" else ord(" ")
        data[-2] = calculateChecksum(data[6:-2])
    return data

def calculateChecksum(data_block):
    # English: Yamaha checksum: (128 - (sum_of_bytes & 0x7F)) & 0x7F
    # English: Sum covers index 6 up to (not including) the checksum byte
    # Italiano: Checksum Yamaha: (128 - (somma_byte & 0x7F)) & 0x7F
    # Doc ref section 5
    s = sum(data_block) & 0x7F
    return (128 - s) & 0x7F

def createEditBufferRequest(channel):
    # English: "Enter Edit Mode" command + request current Edit Buffer voice
    # Italiano: Comando "Entra in modalita Edit" + richiesta voce Edit Buffer corrente
    nibble  = _deviceNibble(channel)
    id_edit = 0x10 | nibble
    id_dump = 0x20 | nibble
    enter_edit_mode = [0xF0, 0x43, id_edit, 0x34, 0x0D, 0x00, 0x00, 0x05, 0x00, 0x7F, 0xF7]
    request_dump    = [0xF0, 0x43, id_dump, 0x7A,
                       0x4C, 0x4D, 0x20, 0x20, 0x38, 0x31, 0x30, 0x31, 0x56, 0x43,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0xF7]
    enter_voice_mode = [0xF0, 0x43, id_edit, 0x34, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF7]
    return enter_edit_mode + request_dump + enter_voice_mode

def isEditBufferDump(message):
    # English: Voice dump ("VC") with memory type 0x7F at index 30 and memory number 0x00 at index 31
    # Italiano: Dump voice ("VC") con tipo memoria 0x7F all'indice 30 e numero memoria 0x00 all'indice 31
    if _isVoiceDump(message):
        return message[30] == 0x7F and message[31] == 0x00
    return False

def isSingleProgramDump(message):
    # English: Voice dump ("VC") of the internal memory: memory type 0x00 at index 30, slot 0x00-0x3F at index 31
    # English: Mutually exclusive with isEditBufferDump (which has 0x7F at index 30)
    # English: Other bulk dumps (Multi, Pan, Micro tuning, System, PC table) are rejected by the "VC" check.
    # Italiano: Dump voice ("VC") della memoria interna: tipo memoria 0x00 all'indice 30, slot 0x00-0x3F all'indice 31
    # Italiano: Mutuamente esclusivo con isEditBufferDump (che ha 0x7F all'indice 30)
    # Italiano: Gli altri bulk dump (Multi, Pan, Micro tuning, System, tabella PC) sono scartati dal controllo "VC".
    # Doc ref section 6.a: bytes 31-32 = "00 01" for slot 2
    # English: ALSO accepted: the TG77 front panel sends a single voice with memory type 0x7F and the memory
    # English: number it was recalled from (e.g. 0x2E = C15). With number 0x00 it stays an edit buffer dump.
    # Italiano: ACCETTATO ANCHE: il pannello del TG77 invia una voce singola con tipo memoria 0x7F e il numero
    # Italiano: della memoria da cui e stata richiamata (es. 0x2E = C15). Con numero 0x00 resta un dump edit buffer.
    if _isVoiceDump(message):
        return message[30] == 0x00 or (message[30] == 0x7F and message[31] != 0x00)
    return False

def isOwnSysex(message):
    # English: Any TG77 SysEx: F0 + Yamaha 0x43 + TG77 model 0x7A at index 3
    # Italiano: Qualsiasi SysEx TG77: F0 + Yamaha 0x43 + modello TG77 0x7A all'indice 3
    return len(message) > 5 and message[1] == 0x43 and message[3] == 0x7A

def createProgramDumpRequest(channel, patch_no):
    # English: Bulk dump request = Device ID 0x2n, n = Device number - 1 (0x20 for device number 1)
    # Italiano: Richiesta bulk dump = Device ID 0x2n, n = Device number - 1 (0x20 per device number 1)
    req = [0xF0, 0x43, 0x20 | _deviceNibble(channel), 0x7A,
           0x4C, 0x4D, 0x20, 0x20, 0x38, 0x31, 0x30, 0x31, 0x56, 0x43,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           patch_no & 0x3F,
           0xF7]
    return req

def convertToEditBuffer(channel, data):
    # English: Set memory type 0x7F (edit buffer) and number 0x00 at indices 30-31; recalc checksum
    # Italiano: Imposta tipo memoria 0x7F (edit buffer) e numero 0x00 agli indici 30-31; ricalc checksum
    new_data = list(data)
    if len(new_data) > 35:
        new_data[30] = 0x7F
        new_data[31] = 0x00
        new_data[2] = 0x00 | _deviceNibble(channel)
        new_data[-2] = calculateChecksum(new_data[6:-2])
    return new_data

def convertToProgramDump(channel, data, patch_no):
    if not _isVoiceDump(data) or not 0 <= patch_no < 64:
        print("Yamaha SY77-TG77: expected a voice dump and an internal slot from A01 to D16.")
        return []

    voice_type = data[32]
    if voice_type in TG77_DBANK_ONLY_TYPES and patch_no < TG77_BANK_D_START:
        print(f"Yamaha SY77-TG77: {TG77_VOICE_TYPES[voice_type]} cannot be stored at "
              f"{_slotToLabel(patch_no)}. Choose a slot from D01 to D16.")
        return []

    new_data = list(data)
    new_data[30] = 0x00
    new_data[31] = patch_no
    new_data[2] = _deviceNibble(channel)
    new_data[-2] = calculateChecksum(new_data[6:-2])
    return new_data


def calculateFingerprint(message):
    # English: MD5 of the sound parameters only: voice type (index 32) + everything from index 43 (EFMODE,
    # English: first effect parameter) up to the checksum. Skipped on purpose: device ID (2), byte count (4-5),
    # English: header, memory type/number (30-31), name (33-42) and checksum, so renaming a voice or storing
    # English: it in another memory does not change the fingerprint.
    # Italiano: MD5 dei soli parametri del suono: tipo voce (indice 32) + tutto dall'indice 43 (EFMODE,
    # Italiano: primo parametro effetti) fino al checksum. Esclusi di proposito: device ID (2), byte count (4-5),
    # Italiano: intestazione, tipo/numero memoria (30-31), nome (33-42) e checksum, cosi rinominare una voce o
    # Italiano: salvarla in un'altra memoria non cambia il fingerprint.
    if len(message) > 44:
        return hashlib.md5(bytearray(list(message[32:33]) + list(message[43:-2]))).hexdigest()
    return "empty"

def changeChannelInMessage(new_channel, message):
    # English: Update Device ID at index 2: keep upper nibble (0x0n data / 0x1n parameter / 0x2n request),
    # English: set the lower nibble to the 0-based channel (Device number - 1)
    # Italiano: Aggiorna Device ID all'indice 2: mantieni nibble alto (0x0n dati / 0x1n parametro / 0x2n richiesta),
    # Italiano: imposta il nibble basso al canale 0-based (Device number - 1)
    new_msg = list(message)
    if len(new_msg) > 2:
        new_msg[2] = (new_msg[2] & 0xF0) | _deviceNibble(new_channel)
    return new_msg

def isPartOfSingleProgramDump(message):
    # English: A TG77 voice dump is one single MIDI message, so it is the same test as isSingleProgramDump
    # Italiano: Un dump voice TG77 e un solo messaggio MIDI, quindi il test e lo stesso di isSingleProgramDump
    return isSingleProgramDump(message)

def make_test_data():
    # English: Feeds the generic KnobKraft adaptation test suite. The file testData/Yamaha_SY77-TG77/TG77_test_voices.syx
    # English: holds 75 single-voice dumps (memory type 0x00): messages 0-63 = the 64 internal voices (number = index),
    # English: messages 64-74 = the same voice in all 11 voice modes (TG77_11_VOICE_MODE, stored D01-D11 = numbers 48-58).
    # English: Samples: one voice per type from the 64 + the two PD:PUFF AT voices that differ only in EFMODE (2 vs 1)
    # English: + all 11 modes (1AFM/2AFM/4AFM mono, 1AFM/2AFM, 1AWM/2AWM/4AWM, 1AFM+1AWM, 2AFM+2AWM, Drum set).
    # Italiano: Alimenta la suite di test generica delle adaptation KnobKraft. Il file testData/Yamaha_SY77-TG77/TG77_test_voices.syx
    # Italiano: contiene 75 dump di voce singola (tipo memoria 0x00): messaggi 0-63 = le 64 voci interne (numero = indice),
    # Italiano: messaggi 64-74 = la stessa voce in tutti gli 11 modi (TG77_11_VOICE_MODE, salvate D01-D11 = numeri 48-58).
    # Italiano: Campioni: una voce per tipo dalle 64 + le due voci PD:PUFF AT che differiscono solo per EFMODE (2 contro 1)
    # Italiano: + tutti gli 11 modi (1AFM/2AFM/4AFM mono, 1AFM/2AFM, 1AWM/2AWM/4AWM, 1AFM+1AWM, 2AFM+2AWM, Drum set).
    import testing

    # (index in file, expected memory number, expected name, description for the comment)
    samples = [
        (12, 12, "PD:GB AGE",  "1AFM"),
        (2,  2,  "PD:WARM",    "2AFM"),
        (6,  6,  "WN:PICCOLO", "1AWM"),
        (4,  4,  "PD:CHRSTRG", "2AWM"),
        (0,  0,  "PD:PUFF AT", "1AFM+1AWM, EFMODE 2"),
        (19, 19, "PD:PUFF AT", "1AFM+1AWM, EFMODE 1"),
        (48, 48, "PD:OPET",    "2AFM+2AWM"),
        (64, 48, "1AFM mono",  "1AFM mono"),
        (65, 49, "2AFM mono",  "2AFM mono"),
        (66, 50, "4AFM mono",  "4AFM mono (1564 bytes)"),
        (67, 51, "1AFM poly",  "1AFM"),
        (68, 52, "2AFM poly",  "2AFM"),
        (69, 53, "1AWM poly",  "1AWM"),
        (70, 54, "2AWM poly",  "2AWM"),
        (71, 55, "4AWM poly",  "4AWM"),
        (72, 56, "1AFM&1AWM",  "1AFM+1AWM"),
        (73, 57, "2AFM&2AWM",  "2AFM+2AWM"),
        (74, 58, "Drum Set",   "Drum set"),
    ]

    def programs(test_data):
        for index, number, name, _description in samples:
            yield testing.ProgramTestData(message=test_data.all_messages[index], name=name,
                                          friendly_number=f"{number + 1:02d}-{_slotToLabel(number)}",
                                          rename_name="NEW NAME", number=number, target_no=number)

    def edit_buffers(test_data):
        # English: No original edit buffer file: built from the program dumps with convertToEditBuffer (type 0x7F, number 0)
        # Italiano: Nessun file originale dell'edit buffer: ricavato dai program dump con convertToEditBuffer (tipo 0x7F, numero 0)
        for index, number, name, _description in samples:
            yield testing.ProgramTestData(message=convertToEditBuffer(0, test_data.all_messages[index]),
                                          name=name, rename_name="NEW NAME", target_no=number)

    return testing.TestData(sysex="testData/Yamaha_SY77-TG77/TG77_test_voices.syx",
                            expected_patch_count=75, program_generator=programs,
                            edit_buffer_generator=edit_buffers)


# English: End of Yamaha SY77-TG77 Adaptation
# Italiano: Fine dell'Adattamento per Yamaha SY77-TG77
