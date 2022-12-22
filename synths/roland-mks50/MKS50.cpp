/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MKS50.h"

#include "MKS50_Patch.h"
#include "MKS50_Parameter.h"
#include "MidiHelpers.h"

#include "Logger.h"
#include "Sysex.h"

#include <boost/format.hpp>

namespace midikraft {

	// Definitions from the manual, see p. 62ff

	const uint8 ROLAND_ID = 0b01000001;
	const uint8 MKS50_ID = 0b00100011;

	enum class MKS50::MKS50_Operation_Code {
		INVALID = 0b00000000, // For error signaling
		APR = 0b00110101, // All parameters
		BLD = 0b00110111, // Bulk dump
		IPR = 0b00110110, // Individual parameter
		WSF = 0b01000000, // Want to send file
		RQF = 0b01000001, // Request file
		DAT = 0b01000010, // Data
		ACK = 0b01000011, // Acknowledge
		EOF_ = 0b01000101, // End of file [Who defined EOF as a macro??]
		ERR = 0b01001110, // Error
		RJC = 0b01001111, // Rejection
	};

	MKS50::MKS50() : channel_(MidiChannel::invalidChannel())
	{
	}

	int MKS50::numberOfBanks() const
	{
		return 2;
	}

	int MKS50::numberOfPatches() const
	{
		return 64;
	}

	std::string MKS50::friendlyBankName(MidiBankNumber bankNo) const
	{
		return bankNo.toZeroBased() <= 63 ? "Bank A" : "Bank B";
	}

	std::shared_ptr<DataFile> MKS50::patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const
	{
		// Banks are called group, as the first digit is called bank
		int group = place.toZeroBased() % 64;
		int bank = place.toZeroBased() % 8;
		int no = place.toZeroBased() / 8;
		std::string name = (boost::format("%c%d%d") % static_cast<char>((static_cast<int>('A') + group)) % bank % no).str();
		return std::make_shared<MKS50_Patch>(place, name, data);
	}

	bool MKS50::isOwnSysex(MidiMessage const& message) const
	{
		if (message.isSysEx()) {
			if (message.getSysExDataSize() > 3) {
				auto data = message.getSysExData();
				return data[0] == ROLAND_ID && data[3] == MKS50_ID;
			}
		}
		return false;
	}

	std::vector<MidiMessage> MKS50::deviceDetect(int channel)
	{
		// The MKS-50 cannot be actively detected, it has to send messages to be spotted on the network
		ignoreUnused(channel);
		return {};
	}

	int MKS50::deviceDetectSleepMS()
	{
		// Just a guess
		return 100;
	}

	MidiChannel MKS50::channelIfValidDeviceResponse(const MidiMessage& message)
	{
		if (isOwnSysex(message)) {
			if (message.getSysExDataSize() > 2) {
				return MidiChannel::fromZeroBase(message.getSysExData()[2] & 0x0f);
			}
		}
		return MidiChannel::invalidChannel();
	}

	bool MKS50::needsChannelSpecificDetection()
	{
		return false;
	}

	std::string MKS50::getName() const
	{
		return "Roland MKS-50";
	}

	std::vector<MidiMessage> MKS50::requestEditBufferDump() const
	{
		// This is actually an empty message - as we don't have a requestProgramDump, we will issue a program change before
		// sending this (non) message - and this will trigger an APR message by the MKS50 anyway. How minimalistic!
		return {};
	}

	bool MKS50::isEditBufferDump(const std::vector<MidiMessage>& message) const
	{
		if (message.size() == 1 && isOwnSysex(message[0])) {
			return getSysexOperationCode(message[0]) == MKS50_Operation_Code::APR;
		}
		return false;
	}

	std::shared_ptr<DataFile> MKS50::patchFromSysex(const std::vector<MidiMessage>& message) const
	{
		if (isEditBufferDump(message)) {
			switch (message[0].getSysExData()[4])
			{
			case 0b00100000: /* Level 1 */
				if (message[0].getSysExData()[5] == 1 /* Group ID*/) {
					return MKS50_Patch::createFromToneAPR(message[0]);
				}
				else {
					jassert(false);
					SimpleLogger::instance()->postMessage("ERROR - Group ID is not 1, probably corrupt file. Ignoring this APR package.");
				}
			case 0b00110000: /* Level 2 */
				SimpleLogger::instance()->postMessage("Warning - ignoring patch data for now, looking for tone data!");
				break;
			case 0b01000000: /* Level 3 */
				SimpleLogger::instance()->postMessage("Warning - ignoring chord data for now, looking for tone data!");
				break;
			default:
				jassert(false);
				SimpleLogger::instance()->postMessage("ERROR - unknown level in APR package, probably corrupt file. Ignoring this APR package.");
			}
		}
		return std::shared_ptr<MKS50_Patch>();
	}

	std::vector<juce::MidiMessage> MKS50::patchToSysex(std::shared_ptr<DataFile> patch) const
	{
		// It is not entirely clear what to do for the MKS50 - my working hypothesis is that I can send an APR package and it will overwrite the edit buffer?
		std::vector<uint8> syx({ ROLAND_ID, static_cast<uint8>(MKS50_Operation_Code::APR),
			static_cast<uint8>(channel_.isValid() ? channel_.toZeroBasedInt() : 0), MKS50_ID, 0b00100000 /* level */, 1 /* group */ });
		// Copy the tone data
		std::copy(patch->data().begin(), patch->data().end(), std::back_inserter(syx));
		// And now reverse map the 10 characters of the patch name into the bytes
		std::string name = patch->name();
		//TODO - this should be taken from the MKS50_Patch class
		const std::string kPatchNameChar = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -";
		for (int i = 0; i < 10; i++) {
			char charToFind;
			if (i >= name.size()) {
				jassert(false);
				charToFind = ' ';
			}
			else {
				charToFind = name[i];
			}
			bool found = false;
			for (int c = 0; c < kPatchNameChar.size(); c++) {
				if (kPatchNameChar[c] == charToFind) {
					syx.push_back((uint8) c);
					found = true;
				}
			}
			if (!found) {
				syx.push_back(62); // This should be blank
			}
		}
		return std::vector<MidiMessage>({ MidiHelpers::sysexMessage(syx) });
	}

	juce::MidiMessage MKS50::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		return {};
	}

	struct MKS50HandshakeState : public HandshakeLoadingCapability::ProtocolState {
		MKS50HandshakeState() : done(false), isBulkDump(false), numWSF(0), success(false), dataPackages(0) {}
		virtual bool isFinished() override { return done; }
		virtual bool wasSuccessful() override { return success; }
		virtual double progress() override { return dataPackages / 16.0; }

		bool done;
		MidiMessage previousMessage;
		bool isBulkDump;
		int numWSF;
		bool success;
		int dataPackages;
	};

	std::shared_ptr<HandshakeLoadingCapability::ProtocolState> MKS50::createStateObject()
	{
		return std::make_shared<MKS50HandshakeState>();
	}

	void MKS50::startDownload(std::shared_ptr<SafeMidiOutput> output, std::shared_ptr<ProtocolState> saveState)
	{
		ignoreUnused(output, saveState);
		// Nothing to be done, the download must be started by the user using the front panel of the MKS50...	
	}

	bool MKS50::isNextMessage(MidiMessage const& message, std::vector<MidiMessage>& answer, std::shared_ptr<ProtocolState> state)
	{
		auto s = std::dynamic_pointer_cast<MKS50HandshakeState>(state);
		if (isOwnSysex(message)) {
			// My MKS-50 tends to send each message twice... this is a bit weird, and I am not sure if I have a loop in my MIDI setup or is that this device.
			// For now, just check if this is the same message and if yes, drop it.
			if (MidiHelpers::equalSysexMessageContent(message, s->previousMessage)) {
				SimpleLogger::instance()->postMessage("Dropping suspicious duplicate MIDI message from the MKS-50");
				return false;
			}
			s->previousMessage = message;

			switch (getSysexOperationCode(message)) {
			case MKS50_Operation_Code::BLD:
				s->isBulkDump = true;
				// The user has selected the version with "unidirectional", no acknowledge or anything is required. Just count to 16
				s->dataPackages++;
				if (s->dataPackages == 16) {
					s->done = true;
					s->success = true;
				}
				return true;
			case MKS50_Operation_Code::WSF:
				if (s->numWSF > 2) {
					// This is more than 2 WSF, reject
					answer = { buildHandshakingMessage(MKS50_Operation_Code::RJC) };
					s->done = true;
					return false;
				}
				s->numWSF++;
				answer = { buildHandshakingMessage(MKS50_Operation_Code::ACK) };
				return false;
			case MKS50_Operation_Code::DAT:
				if (s->numWSF < 1) {
					// This is data without a WSF first, reject
					answer = { buildHandshakingMessage(MKS50_Operation_Code::RJC) };
					s->done = true;
					return false;
				}
				// This data package is part of the proper data, acknowledge and return true so the data is kept
				answer = { buildHandshakingMessage(MKS50_Operation_Code::ACK) };
				s->dataPackages++;
				return true;
			case MKS50_Operation_Code::RQF:
				// If RQF comes during a download, something is really wrong. 
				s->done = true;
				answer = { buildHandshakingMessage(MKS50_Operation_Code::RJC) };
				return false;
			case MKS50_Operation_Code::EOF_:
				// The MKS50 thinks it is done and wants an ACK for that
				answer = { buildHandshakingMessage(MKS50_Operation_Code::ACK) };
				s->done = true;
				s->success = s->dataPackages == 16;
				return false;
			case MKS50_Operation_Code::RJC:
			case MKS50_Operation_Code::ERR:
				// The MKS is unhappy and wants to abort. No need for us to send an answer
				s->done = true;
				return false;
			case MKS50_Operation_Code::APR:
			case MKS50_Operation_Code::IPR:
				// These are valid messages by the MKS50, but have no meaning in the context of the handshake protocol. Very unlikely that they will come in, 
				// but be safe and ignore them
				return false;
			}
		}
		return false;
	}

	TPatchVector MKS50::loadSysex(std::vector<MidiMessage> const& sysexMessages)
	{
		// Now, the MKS50 has three different formats: The BLD format from single-way dumps, the DAT format from two-way dumps, and the APR format 
		TPatchVector result;
		int datPackageCounter = 0;
		for (auto message : sysexMessages) {
			if (isOwnSysex(message)) {
				switch (getSysexOperationCode(message)) {
				case MKS50_Operation_Code::BLD:
					// This is a bulk dump message
					switch (message.getSysExData()[4]) {
					case 0b00100000:
						// Level 1, Tone Dump
						if (message.getSysExData()[5] == 0x01 /* Required group 1 */) {
							if (message.getSysExData()[6] == 0x00 /* Documentation says "program number extension" and requires it to be 0? */) {
								SimpleLogger::instance()->postMessage((boost::format("Found tone data block starting at #%d") % (int)message.getSysExData()[7]).str());
								for (int patch = 0; patch < 4; patch++) {
									std::vector<uint8> patchData;
									for (int i = 0; i < 64; i += 2) {
										int index = 8 + patch * 64 + i;
										uint8 nibble1 = message.getSysExData()[index];
										uint8 nibble2 = message.getSysExData()[index + 1];
										patchData.push_back(nibble1 | (nibble2 << 4));
									}
									auto newPatch = MKS50_Patch::createFromToneBLD(MidiProgramNumber::fromZeroBase(message.getSysExData()[7] + patch), patchData);
									result.push_back(newPatch);
									SimpleLogger::instance()->postMessage("Found tone " + newPatch->name());
								}
							}
							else {
								SimpleLogger::instance()->postMessage("Error - Program Number extension is not 0");
								return TPatchVector();
							}
						}
						else {
							SimpleLogger::instance()->postMessage("Error - Group is not set to 1");
							return TPatchVector();
						}
						break;
					case 0b00110000:
						// Level 2, Patch Dump, MKS-50 only (no Alpha Juno)
						if (message.getSysExData()[5] == 0x01 /* Required group 1 */) {
							if (message.getSysExData()[6] == 0x00 /* Documentation says "program number extension" and requires it to be 0? */) {
								SimpleLogger::instance()->postMessage((boost::format("Found patch data block starting at #%d") % (int)message.getSysExData()[7]).str());
								for (int patch = 0; patch < 4; patch++) {
									std::vector<uint8> patchData;
									for (int i = 0; i < 64; i += 2) {
										int index = 8 + patch * 64 + i;
										uint8 nibble1 = message.getSysExData()[index];
										uint8 nibble2 = message.getSysExData()[index + 1];
										patchData.push_back(nibble1 | (nibble2 << 4));
									}
									//TODO - not loading patch data for now, all I am interested in is whether the program name is also blanked out with AAAAAAAAAA
									std::string patchName;
									for (int i = 11; i < 20; i++) {
										//TODO use global variable
										const std::string kPatchNameChar = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -";
										patchName.push_back(kPatchNameChar[patchData[i] & 0b00111111]);
									}
									SimpleLogger::instance()->postMessage("Found patch data for tone " + patchName);
								}
							}
							else {
								SimpleLogger::instance()->postMessage("Error - Program Number extension is not 0");
								return TPatchVector();
							}
						}
						else {
							SimpleLogger::instance()->postMessage("Error - Group is not set to 1");
							return TPatchVector();
						}
						break;
					case 0b01000000:
						// Level 3, Chord Memory Dump, MKS-50 only and for now ignored
						break;
					default:
						SimpleLogger::instance()->postMessage("Error - unknown Level in BLD package");
						return TPatchVector();
					}
					break;
				case MKS50_Operation_Code::DAT:
					// This is a DAT message, part of a bulk dump created with handshake. Very similar to the BLD message.
					switch (message.getSysExDataSize()) {
					case 256 + 5: {
						// This is either a tone or a patch block - which one, we can only figure out via context in the message stream!
						// In this mode there is a checksum!
						uint8 checksum = 0;
						for (int i = 4; i < 256 + 4; i++) {
							checksum = (checksum + message.getSysExData()[i]) & 0x7f;
						}
						if (((checksum + message.getSysExData()[256 + 4]) & 0x7f) != 0) {
							Sysex::saveSysex("failed_checksum.bin", { message });
							jassert(false);
							SimpleLogger::instance()->postMessage("Checksum error, aborting!");
							return result;
						}

						if (datPackageCounter < 16) {
							// Must be a tone block
							for (int patch = 0; patch < 4; patch++) {
								std::vector<uint8> patchData;
								for (int i = 0; i < 64; i += 2) {
									int index = 4 + patch * 64 + i;
									uint8 nibble1 = message.getSysExData()[index];
									uint8 nibble2 = message.getSysExData()[index + 1];
									patchData.push_back(nibble1 | (nibble2 << 4));
								}
								auto newPatch = MKS50_Patch::createFromToneDAT(MidiProgramNumber::fromZeroBase(datPackageCounter * 4 + patch), patchData);
								if (newPatch) {
									if (newPatch->name() == "AAAAAAAAAA") {
										// This is the only indicator we have that you are actually trying to load patch data instead of tone data. The engineers must have found this problem
										// only late in the game, because it doesn't make any sense. There is a tip from the internet which now completely makes sense:
										//
										// There is also an undocumented shortcut to quickly transfer all of the Tone names in Tone Group 'b' to Patch Group 'B' however, it will erase all of the Tones in Tone Group 'a' and restore them to the factory defaults
										// 1) Load a bank of Tones into Tone Group 'b' then hold the[4] + [8] buttons during the next power - up
										// 2) All Group 'b' Tone names will overwrite all Group 'B' Patch names leaving all the Tone Group 'b' data intact
										//
										// This is what you will need to do if you used the handshake mode to transfer data from synth A to B
										SimpleLogger::instance()->postMessage("ERROR - this is actually patch data, not tone data. Make sure to use the Bulk Dump [T-a] function and not [P-A]. Aborting!");
										return result;
									}
									else {
										result.push_back(newPatch);
										SimpleLogger::instance()->postMessage("Found tone " + newPatch->name());
									}
								}
								else {
									jassert(false);
								}
							}
						}
						else {
							// Must be a patch block
							//TODO - this is not correct, in case you have saved patch and tone data into different files...
							SimpleLogger::instance()->postMessage("Ignoring patch definition part of patch dump (for now)");
						}
						datPackageCounter++;
						break;
					}
					case 192 + 5:
						// This is a chord memory block
						SimpleLogger::instance()->postMessage("Ignoring chord memory definition part of patch dump");
						break;
					default:
						jassert(false);
						SimpleLogger::instance()->postMessage("Warning - ignoring DAT block of irregular length");
						break;
					}
					break;
				case MKS50_Operation_Code::APR:
					// APR packages are the default and I call them "editBuffer", because it behaves like one.
					auto newPatch = patchFromSysex({ message });
					if (newPatch) {
						result.push_back(newPatch);
						SimpleLogger::instance()->postMessage("Found tone " + newPatch->name());
					}
				}
			}
		}
		return result;
	}

	std::vector<std::shared_ptr<midikraft::SynthParameterDefinition>> MKS50::allParameterDefinitions() const
	{
		return MKS50_Parameter::allParameterDefinitions;
	}

	MidiMessage MKS50::buildHandshakingMessage(MKS50_Operation_Code code) const {
		return MidiHelpers::sysexMessage({ ROLAND_ID, static_cast<uint8>(code), static_cast<uint8>(channel_.isValid() ? channel_.toZeroBasedInt() : 0), MKS50_ID });
	}

	MKS50::MKS50_Operation_Code MKS50::getSysexOperationCode(MidiMessage const& message) const {
		if (isOwnSysex(message)) {
			return static_cast<MKS50_Operation_Code>(message.getSysExData()[1]);
		}
		jassert(false);
		return MKS50_Operation_Code::INVALID;
	}

}
