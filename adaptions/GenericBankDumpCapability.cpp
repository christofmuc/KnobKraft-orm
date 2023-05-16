/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericBankDumpCapability.h"

#include "Sysex.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;

namespace knobkraft {

	std::vector<juce::MidiMessage> GenericBankDumpRequestCapability::requestBankDump(MidiBankNumber bankNo) const
	{
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

	midikraft::TPatchVector GenericBankDumpCapability::patchesFromSysexBank(const MidiMessage& message) const
	{
		py::gil_scoped_acquire acquire;
		try {
			midikraft::TPatchVector patchesFound;
			std::vector<int> vector = me_->messageToVector(message);
			py::object result = me_->callMethod(kExtractPatchesFromBank, vector);
			int no = 0;
			if (py::isinstance<py::tuple>(result)) {
				auto result_tuple = py::cast<py::tuple>(result);
				int numPatches = py::cast<int>(result_tuple[0]);
				spdlog::info("Got bank result with {} patches", numPatches);
				for (auto patchData : py::cast<py::list>(result_tuple[1]))
				{
					auto allData = GenericAdaptation::intVectorToByteVector(patchData.cast<std::vector<int>>());
					std::vector<uint8> data(allData.data(), allData.data() + allData.size());
					auto patch = me_->patchFromPatchData(data, MidiProgramNumber::fromZeroBase(no++)); //TODO the no is ignored
					if (patch) {
						patchesFound.push_back(patch);
					}
					else {
						spdlog::warn("Adaptation: Could not create patch from data returned from {}", kExtractPatchesFromBank);
					}
				}
			}
			else {			
				std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
				auto messages = Sysex::vectorToMessages(byteData);
				for (auto programDump : messages) {
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
			return patchesFound;
		}
		catch (py::error_already_set &ex) {
			me_->logAdaptationError(kExtractPatchesFromBank, ex);
			ex.restore();
		}
		catch (std::exception &ex) {
			me_->logAdaptationError(kExtractPatchesFromBank, ex);
		}
		return {};
	}

}


