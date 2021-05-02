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
		GenericStoredPatchNameCapability(std::shared_ptr<GenericPatch> me) : me_(me) {}

		void setName(std::string const &name) override;

	private:
		std::weak_ptr<GenericPatch> me_;
	};

	class GenericDefaultNameCapability : public midikraft::DefaultNameCapability {
	public:
		GenericDefaultNameCapability(std::shared_ptr<GenericPatch> me) : me_(me) {}

		virtual bool isDefaultName(std::string const &patchName) const override;

	private:
		std::weak_ptr<GenericPatch> me_;
	};

	class GenericPatch : public midikraft::DataFile, public midikraft::RuntimeCapability<midikraft::StoredPatchNameCapability>
		, public midikraft::RuntimeCapability<midikraft::DefaultNameCapability>
		, public std::enable_shared_from_this<GenericPatch>
	{
	public:
		enum DataType {
			PROGRAM_DUMP = 0,
			EDIT_BUFFER
		};

		GenericPatch(GenericAdaptation const *me, pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType);

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
				catch (py::error_already_set &ex) {
					logAdaptationError(methodName.c_str(), ex);
					ex.restore();
					return pybind11::none();
				}
				catch (std::exception &ex) {
					logAdaptationError(methodName.c_str(), ex);
					return pybind11::none();
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

		// For error handling
		void logAdaptationError(const char *methodName, std::exception &e) const;

		// Runtime Capabilities
		bool hasCapability(std::shared_ptr<midikraft::StoredPatchNameCapability> &outCapability) const override;
		bool hasCapability(midikraft::StoredPatchNameCapability **outCapability) const override;
		bool hasCapability(std::shared_ptr<midikraft::DefaultNameCapability> &outCapability) const override;
		bool hasCapability(midikraft::DefaultNameCapability **outCapability) const override;

	private:
		std::shared_ptr<GenericStoredPatchNameCapability> genericStoredPatchNameCapabilityImpl_;
		std::shared_ptr<GenericDefaultNameCapability> genericDefaultNameCapabilityImp_;

		GenericAdaptation const *me_;
		pybind11::module &adaptation_;
	};


}

