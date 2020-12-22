/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"

#include "GenericAdaptation.h"

#include "Capability.h"
#include "StoredPatchNameCapability.h"

#include <pybind11/embed.h>

#include <boost/format.hpp>

namespace knobkraft {

	class GenericPatch;

	class GenericStoredPatchNameCapability : public midikraft::StoredPatchNameCapability {
	public:
		GenericStoredPatchNameCapability(GenericPatch *me) : me_(me) {}

		void setName(std::string const &name) override;
		virtual bool isDefaultName() const override;

	private:
		GenericPatch *me_;
	};

	class GenericPatch : public midikraft::DataFile, public midikraft::RuntimeCapability<midikraft::StoredPatchNameCapability>
	{
	public:
		enum DataType {
			PROGRAM_DUMP = 0,
			EDIT_BUFFER
		};

		GenericPatch(pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType);

		bool pythonModuleHasFunction(std::string const &functionName) const;

		template <typename ... Args>
		pybind11::object callMethod(std::string const &methodName, Args& ... args) const {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			if (!adaptation_) {
				return pybind11::none();
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

		// Runtime Capabilities
		bool hasCapability(std::shared_ptr<midikraft::StoredPatchNameCapability> &outCapability) const override;
		bool hasCapability(midikraft::StoredPatchNameCapability **outCapability) const override;

	private:
		std::shared_ptr<GenericStoredPatchNameCapability> genericStoredPatchNameCapabilityImpl_;

		pybind11::module &adaptation_;
		std::string name_;
	};


}

