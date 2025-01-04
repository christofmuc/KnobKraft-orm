/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericHasBankDescriptorsCapability.h"

#include "GenericAdaptation.h"
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

namespace py = pybind11;

namespace knobkraft {

	std::vector<midikraft::BankDescriptor> GenericHasBankDescriptorsCapability::bankDescriptors() const
	{
		py::gil_scoped_acquire acquire;
		try {
			py::object result = me_->callMethod(kBankDescriptors);
			std::vector<midikraft::BankDescriptor> banks;

			auto d = py::cast<std::vector<py::dict>>(result);
			for (auto const& descriptor : d)
			{
				midikraft::BankDescriptor bank;
				bank.size = py::cast<int>(descriptor["size"]);
				bank.bank  = MidiBankNumber::fromZeroBase(py::cast<int>(descriptor["bank"]), bank.size);
				bank.name  = py::cast<std::string>(descriptor["name"]);
				if (descriptor.contains("isROM")) {
					bank.isROM = py::cast<bool>(descriptor["isROM"]);
				}
				else {
					bank.isROM = false;
				}
				if (descriptor.contains("type")) {
					bank.type = py::cast<std::string>(descriptor["type"]);
				}
				else {
					bank.type = "Patch";
				}
				banks.emplace_back(bank);
			}

			return banks;
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kBankDescriptors, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kBankDescriptors, ex);
		}
		return {};
	}

	std::vector<juce::MidiMessage> GenericHasBankDescriptorsCapability::bankSelectMessages(MidiBankNumber bankNo) const {
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
