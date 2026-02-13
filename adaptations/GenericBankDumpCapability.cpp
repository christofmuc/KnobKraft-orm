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

	std::vector<juce::MidiMessage> GenericBankDumpRequestCapability::requestBankDump(MidiBankNumber bankNo) const
	{
		spdlog::info("requestBankDump called for bank {}", bankNo.toZeroBased());
		py::gil_scoped_acquire acquire;
		try {
			int c = me_->channel().toZeroBasedInt();
			int bank = bankNo.toZeroBased();
			py::object result = me_->callMethod(kCreateBankDumpRequest, c, bank);
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
			return Sysex::vectorToMessages(byteData);
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

	bool GenericBankDumpCapability::isBankDump(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		try {
			auto vector = me_->messageToVector(message);
			py::object result = me_->callMethod(kIsPartOfBankDump, vector);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsPartOfBankDump, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsPartOfBankDump, ex);
		}
		return false;
	}

	bool GenericBankDumpCapability::isBankDumpFinished(std::vector<MidiMessage> const &bankDump) const
	{
		py::gil_scoped_acquire acquire;
		try {
			std::vector<std::vector<int>> vector;
			for (auto message : bankDump) {
				vector.push_back(me_->messageToVector(message));
			}
			py::object result = me_->callMethod(kIsBankDumpFinished, vector);
			return result.cast<bool>();
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kIsBankDumpFinished, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kIsBankDumpFinished, ex);
		}
		return false;
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


