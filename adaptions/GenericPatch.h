/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchNumber.h"
#include "Patch.h"

#include <pybind11/embed.h>

#include <boost/format.hpp>

namespace knobkraft {

	class GenericPatchNumber : public midikraft::PatchNumber {
	public:
		GenericPatchNumber(MidiProgramNumber programNumber);
		std::string friendlyName() const override;

	private:
		MidiProgramNumber programNumber_;
	};

	class GenericPatch : public midikraft::Patch, public midikraft::StoredPatchNameCapability {
	public:
		enum DataType {
			PROGRAM_DUMP = 0,
			EDIT_BUFFER
		};

		GenericPatch(pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType);

		bool pythonModuleHasFunction(std::string const &functionName);

		template <typename ... Args>
		pybind11::object callMethod(std::string const &methodName, Args& ... args) const {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			if (!adaptation_) {
				return py::none();
			}
			if (pybind11::hasattr(*adaptation_, methodName.c_str())) {
				try {
					auto result = adaptation_.attr(methodName.c_str())(args...);
					checkForPythonOutputAndLog();
					return result;
				}
				catch (std::exception &ex) {
					throw ex;
				}
				catch (...) {
					throw std::runtime_error("Unhandled exception");
				}
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: method %s not found, fatal!") % methodName).str());
				return pybind11::none();
			}
		}


		std::string name() const override;

		std::shared_ptr<midikraft::PatchNumber> patchNumber() const override;
		void setPatchNumber(MidiProgramNumber patchNumber) override;

		void setName(std::string const &name) override;
		virtual bool isDefaultName() const override;

	private:
		pybind11::module &adaptation_;
		std::string name_;
		std::shared_ptr<midikraft::PatchNumber> patchNumber_;
	};


}

