/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericHasBanksCapability.h"

#include "GenericAdaptation.h"
#include "Sysex.h"

#include <pybind11/embed.h>

namespace py = pybind11;

namespace knobkraft {

	int GenericHasBanksCapability::numberOfBanks() const
	{
		py::gil_scoped_acquire acquire;
		try {
			py::object result = me_->callMethod(kNumberOfBanks);
			return result.cast<int>();
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kNumberOfBanks, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kNumberOfBanks, ex);
		}
		return 1;
	}

	int GenericHasBanksCapability::numberOfPatches() const
	{
		py::gil_scoped_acquire acquire;
		try {
			py::object result = me_->callMethod(kNumberOfPatchesPerBank);
			return result.cast<int>();
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kNumberOfPatchesPerBank, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kNumberOfPatchesPerBank, ex);
		}
		return 0;
	}

	std::string GenericHasBanksCapability::friendlyBankName(MidiBankNumber bankNo) const
	{
		py::gil_scoped_acquire acquire;
		if (!me_->pythonModuleHasFunction(kFriendlyBankName)) {
			return fmt::format("Bank {}", bankNo.toOneBased());
		}
		try {
			int bankAsInt = bankNo.toZeroBased();
			py::object result = me_->callMethod(kFriendlyBankName, bankAsInt);
			return result.cast<std::string>();
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kFriendlyBankName, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kFriendlyBankName, ex);
		}
		return "invalid name";
	}

	std::vector<juce::MidiMessage> GenericHasBanksCapability::bankSelectMessages(MidiBankNumber bankNo) const {
		py::gil_scoped_acquire acquire;
		try {
			if (me_->pythonModuleHasFunction(kBankSelect)) {
				int c = me_->channel().toZeroBasedInt();
				int bankAsInt = bankNo.toZeroBased();
				py::object result = me_->callMethod(kBankSelect, c, bankAsInt);
				std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
				return Sysex::vectorToMessages(byteData);
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kBankSelect, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kBankSelect, ex);
		}
		return {};
	}


}
