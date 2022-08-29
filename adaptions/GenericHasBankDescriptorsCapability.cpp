/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "GenericHasBankDescriptorsCapability.h"

#include "GenericAdaptation.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

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
				bank.bank  = MidiBankNumber::fromZeroBase(py::cast<int>(descriptor["bank"]));
				bank.name  = py::cast<std::string>(descriptor["name"]);
				bank.size  = py::cast<int>(descriptor["size"]);
				if (descriptor.contains("isROM")) {
					bank.isROM = py::cast<bool>(descriptor["isROM"]);
				}
				if (descriptor.contains("type")) {
					bank.type = py::cast<std::string>(descriptor["type"]);
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

}
