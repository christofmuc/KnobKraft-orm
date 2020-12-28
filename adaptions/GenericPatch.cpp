/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericPatch.h"

#include "GenericAdaptation.h"

namespace py = pybind11;

#include <boost/format.hpp>

namespace knobkraft {

	GenericPatch::GenericPatch(pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType) : midikraft::DataFile(dataType, data), adaptation_(adaptation_module)
	{
	}

	bool GenericPatch::pythonModuleHasFunction(std::string const &functionName) const
	{
		ScopedLock lock(GenericAdaptation::multiThreadGuard);
		if (!adaptation_) {
			return false;
		}
		return py::hasattr(*adaptation_, functionName.c_str());
	}

	std::string GenericPatch::name() const
	{
		try {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			std::vector<int> v(data().data(), data().data() + data().size());
			auto result = adaptation_.attr(kNameFromDump)(v);
			checkForPythonOutputAndLog();
			return result.cast<std::string>();
		}
		catch (py::error_already_set &ex) {
			std::string errorMessage = (boost::format("Error calling %s: %s") % kNameFromDump % ex.what()).str();
			ex.restore(); // Prevent a deadlock https://github.com/pybind/pybind11/issues/1490
			SimpleLogger::instance()->postMessage(errorMessage);
		}
		catch (...) {
			SimpleLogger::instance()->postMessage("Uncaught exception in name() of Patch of GenericAdaptation");
		}
		return "invalid";
	}

	void GenericStoredPatchNameCapability::setName(std::string const &name)
	{
		// set name is an optional method - if it is not implemented, the name in the patch is never changed, the name displayed in the Librarian is
		if (!me_->pythonModuleHasFunction(kRenamePatch)) return;

		// Very well, then try to change the name in the patch data
		try {
			ScopedLock lock(GenericAdaptation::multiThreadGuard);
			std::vector<int> v(me_->data().data(), me_->data().data() + me_->data().size());
			py::object result = me_->callMethod(kRenamePatch, v, name);
			auto intVector = result.cast<std::vector<int>>();
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(intVector);
			me_->setData(byteData);
		}
		catch (py::error_already_set &ex) {
			std::string errorMessage = (boost::format("Error calling %s: %s") % kRenamePatch % ex.what()).str();
			SimpleLogger::instance()->postMessage(errorMessage);
		}
		catch (...) {
			SimpleLogger::instance()->postMessage((boost::format("Uncaught exception in %s of Patch of GenericAdaptation") % kRenamePatch).str());
		}
	}

	bool GenericStoredPatchNameCapability::isDefaultName() const
	{
		// Not implemented yet
		return false;
	}

	bool GenericPatch::hasCapability(std::shared_ptr<midikraft::StoredPatchNameCapability> &outCapability) const
	{
		midikraft::StoredPatchNameCapability *impl;
		if (hasCapability(&impl)) {
			outCapability = genericStoredPatchNameCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(midikraft::StoredPatchNameCapability **outCapability) const
	{
		if (pythonModuleHasFunction(kRenamePatch)) {
			*outCapability = genericStoredPatchNameCapabilityImpl_.get();
			return true;
		}
		return false;
	}

}

