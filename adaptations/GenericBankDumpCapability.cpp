/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericBankDumpCapability.h"

#include "Sysex.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;

namespace knobkraft {
	namespace {
		bool isStrictBool(py::handle value) {
			return py::isinstance<py::bool_>(value);
		}

		bool isIntList(py::handle value) {
			if (!py::isinstance<py::list>(value)) {
				return false;
			}

			py::list values = py::reinterpret_borrow<py::list>(value);
			for (auto item : values) {
				if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item)) {
					return false;
				}
			}
			return true;
		}

		bool isReplyContainer(py::handle replyData) {
			if (replyData.is_none()) {
				return true;
			}

			if (!py::isinstance<py::list>(replyData)) {
				return false;
			}

			py::list resultList = py::reinterpret_borrow<py::list>(replyData);
			if (py::len(resultList) == 0) {
				return true;
			}

			if (py::isinstance<py::list>(resultList[0])) {
				for (auto item : resultList) {
					if (!isIntList(item)) {
						return false;
					}
				}
				return true;
			}

			return isIntList(replyData);
		}

		std::vector<MidiMessage> pythonReplyToMidiMessages(py::handle replyData) {
			std::vector<MidiMessage> allMessages;
			if (replyData.is_none()) {
				return allMessages;
			}

			if (py::isinstance<py::list>(replyData)) {
				py::list resultList = py::reinterpret_borrow<py::list>(replyData);
				if (py::len(resultList) > 0 && py::isinstance<py::list>(resultList[0])) {
					for (auto item : resultList) {
						auto msgVec = py::cast<std::vector<int>>(item);
						auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
						auto midiMessages = Sysex::vectorToMessages(byteData);
						allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
					}
				}
				else {
					auto msgVec = py::cast<std::vector<int>>(resultList);
					auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
					auto midiMessages = Sysex::vectorToMessages(byteData);
					allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
				}
			}
			else {
				auto msgVec = py::cast<std::vector<int>>(replyData);
				auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
				auto midiMessages = Sysex::vectorToMessages(byteData);
				allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
			}

			return allMessages;
		}

		template <typename Reply>
		Reply invalidBankDumpReply(char const* parserName, char const* reason) {
			spdlog::warn("Adaptation: {} returned malformed response: {}", parserName, reason);
			return { false, {} };
		}

		template <typename Reply>
		Reply parseBankDumpReply(py::object const& result, char const* parserName) {
			if (py::isinstance<py::tuple>(result)) {
				py::tuple resultTuple = py::reinterpret_borrow<py::tuple>(result);
				if (resultTuple.size() != 2) {
					return invalidBankDumpReply<Reply>(parserName, "expected a 2-element tuple");
				}
				if (!isStrictBool(resultTuple[0])) {
					return invalidBankDumpReply<Reply>(parserName, "tuple element 0 must be a bool");
				}
				if (!isReplyContainer(resultTuple[1])) {
					return invalidBankDumpReply<Reply>(parserName, "tuple element 1 must be None, a list of ints, or a list of int lists");
				}

				auto flag = resultTuple[0].cast<bool>();
				auto replies = pythonReplyToMidiMessages(resultTuple[1]);
				return { flag, replies };
			}

			if (!isStrictBool(result)) {
				return invalidBankDumpReply<Reply>(parserName, "expected a bool or a 2-element tuple");
			}

			return { result.cast<bool>(), {} };
		}

		midikraft::BankDumpCapability::HandshakeReply parseBankPartResponse(py::object const& result) {
			return parseBankDumpReply<midikraft::BankDumpCapability::HandshakeReply>(result, "isPartOfBankDump");
		}

		midikraft::BankDumpCapability::FinishedReply parseBankFinishedResponse(py::object const& result) {
			return parseBankDumpReply<midikraft::BankDumpCapability::FinishedReply>(result, "isBankDumpFinished");
		}

		std::vector<std::vector<int>> midiMessagesToNestedVector(GenericAdaptation* me, std::vector<MidiMessage> const& bankDump) {
			std::vector<std::vector<int>> vector;
			vector.reserve(bankDump.size());
			for (auto const& message : bankDump) {
				vector.push_back(me->messageToVector(message));
			}
			return vector;
		}
	}

	std::vector<juce::MidiMessage> GenericBankDumpRequestCapability::requestBankDump(MidiBankNumber bankNo) const
	{
		spdlog::debug("requestBankDump called for bank {}", bankNo.toZeroBased());
		py::gil_scoped_acquire acquire;
		try {
			int c = me_->channel().toZeroBasedInt();
			int bank = bankNo.toZeroBased();
			py::object result = me_->callMethod(kCreateBankDumpRequest, c, bank);

			std::vector<juce::MidiMessage> allMessages;

			if (py::isinstance<py::list>(result)) {
				py::list resultList = result;
				// Check if it's a list of lists or a list of ints
				if (py::len(resultList) > 0 && py::isinstance<py::list>(resultList[0])) {
					// List of lists (multi-message)
					for (auto item : resultList) {
						auto msgVec = py::cast<std::vector<int>>(item);
						auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
						auto midiMessages = Sysex::vectorToMessages(byteData);
						allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
					}
				} else {
					// Single message (list of ints)
					auto msgVec = py::cast<std::vector<int>>(resultList);
					auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
					auto midiMessages = Sysex::vectorToMessages(byteData);
					allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
				}
			} else {
				// Not a list, try to cast directly
				auto msgVec = py::cast<std::vector<int>>(result);
				auto byteData = GenericAdaptation::intVectorToByteVector(msgVec);
				auto midiMessages = Sysex::vectorToMessages(byteData);
				allMessages.insert(allMessages.end(), midiMessages.begin(), midiMessages.end());
			}
			spdlog::debug("requestBankDump returning {} messages", allMessages.size());
			return allMessages;
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kCreateBankDumpRequest, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kCreateBankDumpRequest, ex);
		}
		return {};
	}

	midikraft::BankDumpCapability::HandshakeReply GenericBankDumpCapability::isMessagePartOfBankDump(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto vector = me_->messageToVector(message);
			py::object result = me_->callMethod(kIsPartOfBankDump, vector);
			return parseBankPartResponse(result);
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsPartOfBankDump, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsPartOfBankDump, ex);
		}
		return { false, {} };
	}

	midikraft::BankDumpCapability::FinishedReply GenericBankDumpCapability::bankDumpFinishedWithReply(std::vector<MidiMessage> const& bankDump) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto vector = midiMessagesToNestedVector(me_, bankDump);
			py::object result = me_->callMethod(kIsBankDumpFinished, vector);
			return parseBankFinishedResponse(result);
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsBankDumpFinished, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsBankDumpFinished, ex);
		}
		return { false, {} };
	}

	midikraft::TPatchVector GenericBankDumpCapability::patchesFromSysexBank(std::vector<MidiMessage> const& messages) const
	{
		spdlog::debug("patchesFromSysexBank called with {} messages", messages.size());
		midikraft::TPatchVector patchesFound;
		py::gil_scoped_acquire acquire;
		if (me_->pythonModuleHasFunction(kExtractPatchesFromAllBankMessages)) {
			try {
				// This is the new interface for the bank dump capability - the Python function gets all bank dump messages handed at once and returns a vector
				// of messages (i.e. a list of lists)
				std::vector<std::vector<int>> vector;
				for (auto message : messages) {
					vector.push_back(me_->messageToVector(message));
				}
				py::object result = me_->callMethod(kExtractPatchesFromAllBankMessages, vector);
				int no = 0;
				auto patches = py::cast<py::list>(result);
				spdlog::info("Got bank result with {} patches", patches.size());
				for (auto patchData : patches)
				{
					auto allData = GenericAdaptation::intVectorToByteVector(patchData.cast<std::vector<int>>());
					std::vector<uint8> data(allData.data(), allData.data() + allData.size());
					auto patch = me_->patchFromPatchData(data, MidiProgramNumber::fromZeroBase(no++)); //TODO the no is ignored
					if (patch) {
						patchesFound.push_back(patch);
					}
					else {
						spdlog::warn("Adaptation: Could not create patch from data returned from {}", kExtractPatchesFromAllBankMessages);
					}
				}
			}
			catch (py::error_already_set& ex) {
				me_->logAdaptationError(kExtractPatchesFromAllBankMessages, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				me_->logAdaptationError(kExtractPatchesFromAllBankMessages, ex);
			}
		}
		else
		{
			// Old adaptation interface - the adaptation is handed one MIDI message at a time, but could extract a list of patches from it, but each patch 
			// needs to be in exactly one MIDI message. This was too restrictive.
			try {
				int no = 0;
				for (auto const& message : messages) {
					std::vector<int> vector = me_->messageToVector(message);
					py::object result = me_->callMethod(kExtractPatchesFromBank, vector);
					std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
					auto patches = Sysex::vectorToMessages(byteData);
					// Each of theses messages is supposed to represent a single patch (Kawai K3, Access Virus are examples for this)
					for (auto programDump : patches) {
						std::vector<uint8> data(programDump.getRawData(), programDump.getRawData() + programDump.getRawDataSize());
						auto patch = me_->patchFromPatchData(data, MidiProgramNumber::fromZeroBase(no++)); //TODO the no is ignored
						if (patch) {
							patchesFound.push_back(patch);
						}
						else {
							spdlog::warn("Adaptation: Could not create patch from data returned from {}", kExtractPatchesFromBank);
						}
					}
				}
			}
			catch (py::error_already_set& ex) {
				me_->logAdaptationError(kExtractPatchesFromBank, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				me_->logAdaptationError(kExtractPatchesFromBank, ex);
			}
		}
		spdlog::info("patchesFromSysexBank returning {} patches", patchesFound.size());
		return patchesFound;
	}

	std::vector<MidiMessage> GenericBankDumpSendCapability::createBankMessages(std::vector<std::vector<MidiMessage>> patches) {
		std::vector<MidiMessage> bankMessages;
		py::gil_scoped_acquire acquire;
		if (me_->pythonModuleHasFunction(kConvertPatchesToBankDump)) {
			try {
				std::vector<std::vector<int>> vector;
				for (auto messages : patches) {
					vector.push_back(GenericAdaptation::midiMessagesToVector(messages));
				}
				py::object result = me_->callMethod(kConvertPatchesToBankDump, vector);
				std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
				bankMessages = Sysex::vectorToMessages(byteData);
			}
			catch (py::error_already_set& ex) {
				me_->logAdaptationError(kConvertPatchesToBankDump, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				me_->logAdaptationError(kConvertPatchesToBankDump, ex);
			}
		}
		return bankMessages;
	}

}


