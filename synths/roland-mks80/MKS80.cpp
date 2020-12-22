/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MKS80.h"

#include "MKS80_Patch.h"
#include "MKS80_Parameter.h"
#include "MKS80_LegacyBankLoader.h"
#include "MidiHelpers.h"

//#include "BCR2000.h"
//#include "MKS80_BCR2000.h"

//#include "BCR2000_Component.h"
//#include "BCR2000_Presets.h"

#include "Logger.h"
#include "Sysex.h"

#include <boost/format.hpp>

namespace midikraft {

	MKS80::MKS80()
	{
	}

	int MKS80::numberOfBanks() const
	{
		return 1; // The manual sees the MKS80 to have 8 banks of 8 patches each, but as you can only load all 64 patches in bulk, and that is quick, let's assume it is only one bank.
	}

	int MKS80::numberOfPatches() const
	{
		return 64;
	}

	std::string MKS80::friendlyProgramName(MidiProgramNumber programNo) const
	{
		int bank = programNo.toZeroBased() / 8;
		int patch = programNo.toZeroBased() % 8;
		return (boost::format("%d%d") % (bank + 1) % (patch + 1)).str();
	}

	std::string MKS80::friendlyBankName(MidiBankNumber bankNo) const
	{
		//TODO needs to match the definitions above
		return bankNo.toZeroBased() == 0 ? "Bank A" : "Bank B";
	}

	std::shared_ptr<DataFile> MKS80::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		return std::make_shared<MKS80_Patch>(place, data);
	}

	bool MKS80::isOwnSysex(MidiMessage const &message) const
	{
		if (message.isSysEx()) {
			if (message.getSysExDataSize() > 3) {
				auto data = message.getSysExData();
				return data[0] == ROLAND_ID && data[3] == MKS80_ID;
			}
		}
		return false;
	}

	std::vector<juce::MidiMessage> MKS80::deviceDetect(int channel)
	{
		//TODO Really?. I could send a WSF and it should reply with an ACK
		// The MKS-80 cannot be actively detected, it has to send messages to be spotted on the network
		return { buildHandshakingMessage(MKS80_Operation_Code::WSF, MidiChannel::fromZeroBase(channel)) };
	}

	int MKS80::deviceDetectSleepMS()
	{
		// Just a guess
		return 800;
	}

	MidiChannel MKS80::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		if (isOwnSysex(message)) {
			// If we are successful, we will either get an ACK (protect is OFF) or a RJC (protect is ON). Anyway, the MKS80 did reveal itself
			if (message.getSysExDataSize() == 4 &&
				(getSysexOperationCode(message) == MKS80_Operation_Code::ACK || getSysexOperationCode(message) == MKS80_Operation_Code::RJC)) {
				return MidiChannel::fromZeroBase(message.getSysExData()[2] & 0x0f);
			}
		}
		return MidiChannel::invalidChannel();
	}

	bool MKS80::needsChannelSpecificDetection()
	{
		// When we use the WSF message to detect the MKS-80, we need to do that channel by channel according to the documentation.
		// But the real device replies on all channels with a channel specific message? That would be bad, because we then cannot find out what the real reply would be.
		return true;
	}

	bool MKS80::endDeviceDetect(MidiMessage &endMessageOut) const
	{
		// Send an EOF_ message so the device gets back into normal mode. It will say "Load complete" on its display, but that will disappear quickly
		endMessageOut = buildHandshakingMessage(MKS80_Operation_Code::EOF_);
		return true;
	}

	std::string MKS80::getName() const
	{
		return "Roland MKS-80";
	}

	MidiMessage MKS80::requestEditBufferDump()
	{
		// This is actually an empty message - as we don't have a requestProgramDump, we will issue a program change before
		// sending this (non) message - and this will trigger an PGR and 4 APR messages by the MKS80 anyway. How minimalistic!
		return MidiMessage();
	}

	bool MKS80::isEditBufferDump(const MidiMessage& message) const
	{
		//TODO - problematic, the MKS80 needs 5 messages for an edit buffer dump?
		if (isOwnSysex(message)) {
			return getSysexOperationCode(message) == MKS80_Operation_Code::APR;
		}
		return false;
	}

	std::shared_ptr<Patch> MKS80::patchFromSysex(const MidiMessage& message) const
	{
		//TODO - not sure if this is proper. It seems the patch needs multiple MIDI messages just like in the Yamaha Reface DX
		if (isEditBufferDump(message)) {
			switch (message.getSysExData()[4])
			{
			case 0b00100000: /* Level 1 */
				if (message.getSysExData()[5] == 1 /* Group ID*/) {
					//return MKS50_Patch::createFromToneAPR(message);
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
		return std::shared_ptr<Patch>();
	}

	std::vector<juce::MidiMessage> MKS80::patchToSysex(const Patch &patch) const
	{
		// The MKS80 edit buffer can be modified by sending 4 APR messages, one each for upper, lower * tone, patch
		// Let's assume we do not need to send a PGR message, as we don't want to store this program?
		std::vector<MidiMessage> result;
		auto mks80Patch = dynamic_cast<const MKS80_Patch &>(patch);
		result.push_back(mks80Patch.buildAPRMessage(MKS80_Patch::APR_Section::PATCH_UPPER, channel()));
		result.push_back(mks80Patch.buildAPRMessage(MKS80_Patch::APR_Section::PATCH_LOWER, channel()));
		result.push_back(mks80Patch.buildAPRMessage(MKS80_Patch::APR_Section::TONE_UPPER, channel()));
		result.push_back(mks80Patch.buildAPRMessage(MKS80_Patch::APR_Section::TONE_LOWER, channel()));
		return result;
	}

	MidiMessage MKS80::saveEditBufferToProgram(int programNumber)
	{
		ignoreUnused(programNumber);
		// This is not possible with the MKS80
		return MidiMessage();
	}

	struct MKS80HandshakeState : public HandshakeLoadingCapability::ProtocolState {
		MKS80HandshakeState() : done(false), numWSF(0), success(false), dataPackages(0) {}
		virtual bool isFinished() override { return done; }
		virtual bool wasSuccessful() override { return success; }
		virtual double progress() override { return dataPackages / 16.0; }

		bool done;
		MidiMessage previousMessage;
		int numWSF;
		bool success;
		int dataPackages;
	};

	std::shared_ptr<HandshakeLoadingCapability::ProtocolState> MKS80::createStateObject()
	{
		return std::make_shared<MKS80HandshakeState>();
	}

	void MKS80::startDownload(std::shared_ptr<SafeMidiOutput> output, std::shared_ptr<ProtocolState> saveState)
	{
		// Request the file from the MKS80
		output->sendMessageNow(buildHandshakingMessage(MKS80_Operation_Code::RQF));
	}

	bool MKS80::isNextMessage(MidiMessage const &message, std::vector<MidiMessage> &answer, std::shared_ptr<ProtocolState> state)
	{
		auto s = std::dynamic_pointer_cast<MKS80HandshakeState>(state);
		if (isOwnSysex(message)) {
			if (MidiHelpers::equalSysexMessageContent(message, s->previousMessage)) {
				//TODO Is this an issue with the MKS-80?
				jassert(false);
				SimpleLogger::instance()->postMessage("Dropping suspicious duplicate MIDI message from the MKS-80");
				return false;
			}
			s->previousMessage = message;

			switch (getSysexOperationCode(message)) {
			case MKS80_Operation_Code::WSF:
				if (s->numWSF > 2) {
					//TODO - shouldn't it be more than 1?
					// This is more than 2 WSF, reject
					answer = { buildHandshakingMessage(MKS80_Operation_Code::RJC) };
					s->done = true;
					return false; // No need to store this message in the librarian
				}
				s->numWSF++;
				answer = { buildHandshakingMessage(MKS80_Operation_Code::ACK) };
				return false;
			case MKS80_Operation_Code::DAT:
				// The documentation says this would happen when we send a RQF, so this isn't an error at all

				// This data package is part of the proper data, acknowledge and return true so the data is kept
				answer = { buildHandshakingMessage(MKS80_Operation_Code::ACK) };
				s->dataPackages++;
				s->success = s->dataPackages == 16;
				if (s->success) {
					// We need to answer with an EOF message in case we got all 16 packages, in addition to the ACK message
					answer.push_back(buildHandshakingMessage(MKS80_Operation_Code::EOF_));
				}
				return true;
			case MKS80_Operation_Code::RQF:
				// If RQF comes during a download, something is really wrong. 
				jassert(false);
				s->done = true;
				answer = { buildHandshakingMessage(MKS80_Operation_Code::RJC) };
				return false;
			case MKS80_Operation_Code::EOF_:
				// The MKS80 thinks it is done and wants an ACK for that
				// This does happen only in SAVE mode (initiated from the device), and not in the RQF mode
				answer = { buildHandshakingMessage(MKS80_Operation_Code::ACK) };
				s->done = true;
				s->success = s->dataPackages == 16;
				return false;
			case MKS80_Operation_Code::RJC:
				// fall through
			case MKS80_Operation_Code::ERR:
				// The MKS is unhappy and wants to abort. No need for us to send an answer
				s->done = true;
				return false;
			case MKS80_Operation_Code::APR:
				// fall through
			case MKS80_Operation_Code::IPR:
				// These are valid messages by the MKS80, but have no meaning in the context of the handshake protocol. Very unlikely that they will come in, 
				// but be safe and ignore them
				return false;
			case MKS80_Operation_Code::ACK:
				// An ACK would come from the MKS80 in RQF mode when we have sent our EOF message, this ending the transfer
				s->done = true;
				return false;
			default:
				jassert(false);
				SimpleLogger::instance()->postMessage("Ignoring unknown operation code during handshake transfer with MKS-80");
			}
		}
		return false;
	}

	bool MKS80::canChangeInputChannel() const
	{
		// This is a clear omission on Roland's side.
		return false;
	}

	void MKS80::changeInputChannel(MidiController *controller, MidiChannel channel, std::function<void()> onFinished)
	{
		ignoreUnused(controller, channel, onFinished);
		throw new std::runtime_error("Invalid state, method not implemented");
	}

	MidiChannel MKS80::getInputChannel() const
	{
		return channel();
	}

	bool MKS80::hasMidiControl() const
	{
		return false;
	}

	bool MKS80::isMidiControlOn() const
	{
		// Would be useless if not
		return true;
	}

	void MKS80::setMidiControl(MidiController *controller, bool isOn)
	{
		ignoreUnused(controller, isOn);
		throw new std::runtime_error("Invalid state, method not implemented");
	}

	struct SysexLoadingState {
		bool valid = false;
		bool isAPR = false; // We have two modes - either we load PGR and APR messages, or we do load DAT messages
		int patchCounter = 0;
		std::unique_ptr<MidiProgramNumber> currentPatch;
		std::map<MKS80_Patch::APR_Section, std::vector<uint8>> data;
		std::vector<std::vector<uint8>> datBlocks;
	};

	TPatchVector MKS80::loadSysex(std::vector<MidiMessage> const &sysexMessages)
	{
		// Now, the MKS80 has two different formats: The DAT format from two-way handshake dumps, and the APR format 
		TPatchVector result;
		SysexLoadingState state;
		for (auto message : sysexMessages) {
			if (isOwnSysex(message)) {
				switch (getSysexOperationCode(message)) {
				case MKS80_Operation_Code::DAT: {
					if (state.valid && state.isAPR) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Warning - ignoring DAT block embedded into PGR/APR stream, flaky file!");
						break;
					}
					state.valid = true;
					state.isAPR = false; // We now accept this list of messages as a DAT stream

					// This is a DAT message, part of a bulk dump created with handshake. It carries 248 bytes of data.
					// Each DAT message contains 4 patches consisting of one tone data block and one patch data block
					if (message.getSysExDataSize() != 253) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Warning - ignoring DAT block of irregular length");
						break;
					}

					// In this mode, there is a checksum!
					uint8 checksum = 0;
					for (int i = 4; i < 248 + 4; i++) {
						checksum = (checksum + message.getSysExData()[i]) & 0x7f;
					}
					if ((128 - checksum) != message.getSysExData()[248 + 4]) {
						Sysex::saveSysex("failed_checksum.bin", { message });
						jassert(false);
						SimpleLogger::instance()->postMessage("Checksum error, aborting!");
						return result;
					}

					// All good, we can construct 4 partial patches (layers) now
					for (int block = 0; block < 4; block++) {
						state.datBlocks.emplace_back();
						size_t startOfBlock = 4 + block * 62;
						std::copy(&message.getSysExData()[startOfBlock], &message.getSysExData()[startOfBlock + 62], std::back_inserter(state.datBlocks.back()));
						state.patchCounter++;
					}
					break;
				}
				case MKS80_Operation_Code::PGR: {
					// This starts a PGR section followed by 4 APR messages. Only accept this in case we are not in DAT mode, though
					if (state.valid && !state.isAPR) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Ignoring PGR message embedded into DAT stream, flaky file?");
						break;
					}
					state.valid = true;
					state.isAPR = true;
					const uint8 *data = message.getSysExData();
					if (message.getSysExDataSize() == 9 && data[4] == 0x02 /* Level */ && data[5] == 0x00 /* dummy */ && data[6] == 0x00 /* patch number following */ && data[8] == 0x00 /* NOP */) {
						state.currentPatch = std::make_unique<MidiProgramNumber>(MidiProgramNumber::fromZeroBase(data[7]));
						state.data.clear();
						SimpleLogger::instance()->postMessage((boost::format("Found PGR message starting new patch %s") % friendlyProgramName(*state.currentPatch)).str());
					}
					else {
						state.currentPatch.reset();
						SimpleLogger::instance()->postMessage("Wrong PGR message format, can't determine patch number");
					}
					break;
				}
				case MKS80_Operation_Code::APR: {
					if (state.valid && !state.isAPR) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Ignoring APR message embedded into DAT stream, flaky file?");
						break;
					}
					if (!state.currentPatch) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Ignoring APR message not preceded by proper PGR message");
						break;
					}
					const uint8 *data = message.getSysExData();
					int sectionInt = (data[5] | data[6]);
					if (!MKS80_Patch::isValidAPRSection(sectionInt)) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Invalid level group combination in APR message, ignoring it!");
						break;
					}
					auto section = static_cast<MKS80_Patch::APR_Section>(sectionInt);
					if (state.data.find(section) != state.data.end()) {
						jassert(false);
						SimpleLogger::instance()->postMessage("Warning - got duplicate APR section, ignoring it");
						break;
					}
					state.data[section] = std::vector<uint8>();
					// Copy the useful data bytes
					std::copy(&data[6], &data[message.getSysExDataSize()], std::back_inserter(state.data[section]));

					// Are we finished?
					if (state.data.size() == 4) {
						// We got 4 sections of valid APR data, so we now can create a patch that is standalone and has two layers (both tone and patch!)
						SimpleLogger::instance()->postMessage("Successfully loaded patch from APR format!");
						result.push_back(std::make_shared<MKS80_Patch>(*state.currentPatch, state.data));
					}
					break;
				}
				}
			}
		}

		if (state.valid && !state.isAPR) {
			// We need to convert the 64 DAT blocks into 64 patches - this is so complicated because the MKS80 has only 64 tone memories, but 64 patches with dual layers.
			if (state.datBlocks.size() != 64) {
				SimpleLogger::instance()->postMessage("Got less than 64 patches from DAT stream, failure!");
				return result;
			}
			std::vector<std::vector<uint8>> toneData;
			std::vector<std::vector<uint8>> patchData;
			for (int i = 0; i < state.datBlocks.size(); i++) {
				// First, extract the tone data stored in the dat block!
				toneData.emplace_back(MKS80_Patch::toneFromDat(state.datBlocks[i]));
				// Now extract upper and lower patch definition, in this case into one array of 30 bytes
				patchData.emplace_back(MKS80_Patch::patchesFromDat(state.datBlocks[i]));
			}
			return patchesFromAPRs(toneData, patchData);
		}
		return result;
	}

	std::string MKS80::additionalFileExtensions()
	{
		return "*.m80;*.mks80;*";
	}

	bool MKS80::supportsExtension(std::string const &filename)
	{
		File file(filename);
		return file.getFileExtension().toUpperCase() == ".M80" || file.getFileExtension().toUpperCase() == ".MKS80" || file.getFileExtension() == "";
	}

	midikraft::TPatchVector MKS80::load(std::string const &fullpath, std::vector<uint8> const &fileContent)
	{
		ignoreUnused(fullpath);
		// Just try the different formats...
		auto result = MKS80_LegacyBankLoader::loadM80File(fileContent);
		if (result.empty()) {
			result = MKS80_LegacyBankLoader::loadMKS80File(fileContent);
		}
		return result;
	}

	TPatchVector MKS80::patchesFromAPRs(std::vector<std::vector<uint8>> const &toneData, std::vector<std::vector<uint8>> const &patchData) {
		TPatchVector result;

		// Now, build up 64 standalone patches that ignore the complexity of where the tone data is stored in RAM
		for (int i = 0; i < toneData.size(); i++) {
			std::map<MKS80_Patch::APR_Section, std::vector<uint8>> patch;
			patch[MKS80_Patch::APR_Section::PATCH_UPPER] = std::vector<uint8>(patchData[i].begin(), patchData[i].begin() + 15); //TODO - no range check done here...
			patch[MKS80_Patch::APR_Section::PATCH_LOWER] = std::vector<uint8>(patchData[i].begin() + 15, patchData[i].end());
			int upperTone = patch[MKS80_Patch::APR_Section::PATCH_UPPER][MKS80_Parameter::TONE_NUMBER];
			patch[MKS80_Patch::APR_Section::TONE_UPPER] = toneData[upperTone];
			int lowerTone = patch[MKS80_Patch::APR_Section::PATCH_LOWER][MKS80_Parameter::TONE_NUMBER];
			//jassert(lowerTone== i); // If this is not guaranteed, we might not archive the whole data because a patch might refer to "outside" tone data, leaving tone data unused
			patch[MKS80_Patch::APR_Section::TONE_LOWER] = toneData[lowerTone];
			result.push_back(std::make_shared<MKS80_Patch>(MidiProgramNumber::fromZeroBase(i), patch));
		}

		return result;
	}

	/*std::vector<std::string> MKS80::presetNames()
	{
		// The MKS80 needs two BCR2000 presets - one for lower, one for upper
		auto name = presetName();
		return { (boost::format(name) % 0).str(), (boost::format(name) % 1).str() };
	}

	std::string MKS80::presetName() {
		return (boost::format("Knobkraft MKS80 %%d %d") % channel().toOneBasedInt()).str();
	}

	void MKS80::setupBCR2000(MidiController *controller, BCR2000 &bcr, SimpleLogger *logger)
	{
		if (bcr.wasDetected() && wasDetected() && channel().isValid()) {
			// Make sure to bake the current channel of the synth into the setup for the BCR
			auto bcl = MKS80_BCR2000::generateBCL(presetName(), channel().toZeroBasedInt(), MKS80_LOWER, MKS80_LOWER);
			auto syx = bcr.convertToSyx(bcl);
			bcr.sendSysExToBCR(controller->getMidiOutput(bcr.midiOutput()), syx, *controller, logger);
		}
	}

	void MKS80::syncDumpToBCR(MidiProgramNumber programNumber, MidiController *controller, BCR2000 &bcr, SimpleLogger *logger)
	{
		//TOOO
	}*/

	MidiMessage MKS80::buildHandshakingMessage(MKS80_Operation_Code code) const {
		return buildHandshakingMessage(code, channel());
	}

	MidiMessage MKS80::buildHandshakingMessage(MKS80_Operation_Code code, MidiChannel channel) const {
		switch (code) {
		case MKS80_Operation_Code::WSF:
			// fall through
		case MKS80_Operation_Code::RQF:
		{
			std::vector<uint8> message({ ROLAND_ID, static_cast<uint8>(code), static_cast<uint8>(channel.isValid() ? channel.toZeroBasedInt() : 0), MKS80_ID, 'M', 'K', 'S', '-', '8', '0' });
			message.push_back(rolandChecksum(message.begin() + 4, message.end()));
			return MidiHelpers::sysexMessage(message);
		}
		case MKS80_Operation_Code::ACK:
			// fall through
		case MKS80_Operation_Code::EOF_:
			// fall through
		case MKS80_Operation_Code::ERR:
			// fall through
		case MKS80_Operation_Code::RJC:
			// These messages are simpler in that they do not contain the "MKS-80" ASCII string and a checksum
			return MidiHelpers::sysexMessage({ ROLAND_ID, static_cast<uint8>(code), static_cast<uint8>(channel.isValid() ? channel.toZeroBasedInt() : 0), MKS80_ID });
		default:
			jassert(false);
			// This message type should be generated by a different function?
			return MidiMessage();
		}
	}

	uint8 MKS80::rolandChecksum(std::vector<uint8>::iterator start, std::vector<uint8>::iterator end) {
		uint8 sum = 0;
		uint8 checksum = 0;
		for (auto di = start; di != end; di++) {
			checksum = (checksum - *di) & 0x7f;
			sum += *di;
		}
		jassert(((sum & 0x7f) + checksum) == 0); // This is the definition from the manual p. 50
		return checksum;
	}

	MKS80_Operation_Code MKS80::getSysexOperationCode(MidiMessage const &message) const {
		if (isOwnSysex(message)) {
			return static_cast<MKS80_Operation_Code>(message.getSysExData()[1]);
		}
		jassert(false);
		return MKS80_Operation_Code::INVALID;
	}

	/*void MKS80::setupBCR2000View(BCR2000_Component &view)
	{
		// Iterate over our definition and set the labels on the view to show the layout
		for (auto def : MKS80_BCR2000::BCR2000_setup(MKS80_Parameter::LOWER)) {
			auto encoder = dynamic_cast<MKS80_BCR2000_Encoder*>(def);
			if (encoder) {
				switch (encoder->type()) {
				case BUTTON:
					view.setButtonParam(encoder->encoderNumber(), encoder->parameterDef()->name());
					break;
				case ENCODER:
					view.setRotaryParam(encoder->encoderNumber(), encoder->parameterDef().get()); // TODO, should propage shared_ptr
					break;
				}
			}
			else {
				auto simpleDef = dynamic_cast<BCRStandardDefinitionWithName *>(def);
				if (simpleDef) {
					int button = simpleDef->type() == BUTTON ? simpleDef->encoderNumber() : -1;
					if (button != -1) view.setButtonParam(button, simpleDef->name());
				}
			}
		}
	}*/

	//void MKS80::setupBCR2000Values(BCR2000_Component &view, Patch *patch)
	//{
		// Iterate over our definition and set the labels on the view to show the layout
		/*for (auto def : k3Setup) {
			auto k3def = dynamic_cast<KawaiK3BCR2000Definition *>(def);
			if (k3def) {
				KawaiK3Parameter *k3param = KawaiK3Parameter::findParameter(k3def->param());
				if (k3param) {
					int value;
					if (k3param->valueInPatch(*patch, value)) {
						int encoder = encoderNumber(k3def);
						if (encoder != -1) view.setRotaryParamValue(encoder, value);
					}
				}
			}
		}*/
	//}

}
