/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericPatch.h"

#include "GenericAdaptation.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <fmt/format.h>

namespace knobkraft {

	GenericPatch::GenericPatch(GenericAdaptation const *me, pybind11::module &adaptation_module, midikraft::Synth::PatchData const &data, DataType dataType) : midikraft::DataFile(dataType, data), me_(me), adaptation_(adaptation_module)
	{
	}

	bool GenericPatch::pythonModuleHasFunction(std::string const &functionName) const
	{
		py::gil_scoped_acquire acquire;
		if (!adaptation_) {
			return false;
		}
		return py::hasattr(*adaptation_, functionName.c_str());
	}

	std::string GenericStoredPatchNameCapability::name() const
	{
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			auto patch = me_.lock();
			if (patch->pythonModuleHasFunction(kNameFromDump)) {
				try {
					std::vector<int> v(patch->data().data(), patch->data().data() + patch->data().size());
					auto result = patch->callMethod(kNameFromDump, v);
					checkForPythonOutputAndLog();
					return result.cast<std::string>();
				}
				catch (py::error_already_set& ex) {
					std::string errorMessage = fmt::format("Error calling {}: {}", kNameFromDump, ex.what());
					ex.restore(); // Prevent a deadlock https://github.com/pybind/pybind11/issues/1490
					SimpleLogger::instance()->postMessage(errorMessage);
				}
				catch (std::exception& ex) {
					patch->logAdaptationError(kNameFromDump, ex);
				}
				return "invalid";
			}
			else
			{
				return "noname";
			}
		}
		else {
			return "invalid";
		}
	}

	void GenericPatch::logAdaptationError(const char *methodName, std::exception &ex) const
	{
		// This hoop is required to properly process Python created exceptions
		std::string exceptionMessage = ex.what();
		std::string adaptionName = me_->getName();
		std::string methodCopy(methodName, methodName + strlen(methodName));
		MessageManager::callAsync([adaptionName, methodCopy, exceptionMessage]() {
			SimpleLogger::instance()->postMessage(fmt::format("Adaptation[{}]: Error calling {}: {}", adaptionName, methodCopy, exceptionMessage));
		});
	}

	void GenericStoredPatchNameCapability::setName(std::string const &name)
	{
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			// set name is an optional method - if it is not implemented, the name in the patch is never changed, the name displayed in the Librarian is
			if (!me_.lock()->pythonModuleHasFunction(kRenamePatch)) return;

			// Very well, then try to change the name in the patch data
			try {
				std::vector<int> v(me_.lock()->data().data(), me_.lock()->data().data() + me_.lock()->data().size());
				py::object result = me_.lock()->callMethod(kRenamePatch, v, name);
				auto intVector = result.cast<std::vector<int>>();
				std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(intVector);
				me_.lock()->setData(byteData);
 			}
			catch (py::error_already_set &ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kRenamePatch, ex);
				ex.restore();
			}
			catch (std::exception &ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kRenamePatch, ex);
			}
			catch (...) {
				SimpleLogger::instance()->postMessage(fmt::format("Adaptation[unknown]: Uncaught exception in {} of Patch of GenericAdaptation", kRenamePatch));
			}
		}
	}

	bool GenericDefaultNameCapability::isDefaultName(std::string const &patchName) const
	{
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			auto patch = me_.lock();
			try {
				py::object result = patch->callMethod(kIsDefaultName, patchName);
				return py::cast<bool>(result);
			}
			catch (py::error_already_set &ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kIsDefaultName, ex);
				ex.restore();
			}
			catch (std::exception &ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kIsDefaultName, ex);
			}
			catch (...) {
				SimpleLogger::instance()->postMessage(fmt::format("Uncaught exception in {} of Patch of GenericAdaptation", kIsDefaultName));
			}
		}
		return false;
	}

	midikraft::LayeredPatchCapability::LayerMode GenericLayeredPatchCapability::layerMode() const
	{
		//TODO not sure what the UI different is here
		return LayeredPatchCapability::LayerMode::STACK;
	}

	int GenericLayeredPatchCapability::numberOfLayers() const
	{
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			auto patch = me_.lock();
			try {
				std::vector<int> v(me_.lock()->data().data(), me_.lock()->data().data() + me_.lock()->data().size());
				py::object result = patch->callMethod(kNumberOfLayers, v);
				return py::cast<int>(result);
			}
			catch (py::error_already_set& ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kNumberOfLayers, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kNumberOfLayers, ex);
			}
			catch (...) {
				SimpleLogger::instance()->postMessage(fmt::format("Uncaught exception in {} of Patch of GenericAdaptation", kNumberOfLayers));
			}
		}
		return 1;
	}

	std::string GenericLayeredPatchCapability::layerName(int layerNo) const
	{
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			auto patch = me_.lock();
			try {
				std::vector<int> v(me_.lock()->data().data(), me_.lock()->data().data() + me_.lock()->data().size());
				py::object result = patch->callMethod(kLayerName, v, layerNo);
				return py::cast<std::string>(result);
			}
			catch (py::error_already_set& ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kLayerName, ex);
				ex.restore();
			}
			catch (std::exception& ex) {
				if (!me_.expired())
					me_.lock()->logAdaptationError(kLayerName, ex);
			}
			catch (...) {
				SimpleLogger::instance()->postMessage(fmt::format("Uncaught exception in {} of Patch of GenericAdaptation",  kLayerName));
			}
		}
		return "Invalid";
	}

	void GenericLayeredPatchCapability::setLayerName(int layerNo, std::string const& layerName)
	{
		if (me_.lock()->pythonModuleHasFunction(kSetLayerName)) {
			py::gil_scoped_acquire acquire;
			if (!me_.expired()) {
				auto patch = me_.lock();
				try {
					std::vector<int> v(patch->data().data(), patch->data().data() + patch->data().size());
					py::object result = patch->callMethod(kSetLayerName, v, layerNo, layerName);
					auto intVector = result.cast<std::vector<int>>();
					std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(intVector);
					patch->setData(byteData);
				}
				catch (py::error_already_set& ex) {
					patch->logAdaptationError(kSetLayerName, ex);
					ex.restore();
				}
				catch (std::exception& ex) {
					if (!me_.expired())
						me_.lock()->logAdaptationError(kSetLayerName, ex);
				}
				catch (...) {
					SimpleLogger::instance()->postMessage(fmt::format("Uncaught exception in {} of Patch of GenericAdaptation", kSetLayerName));
				}
			}
		}
		else {
			SimpleLogger::instance()->postMessage("Adaptation did not implement setLayerName(), can't rename layer");
		}
	}

	bool GenericStoredTagCapability::setTags(std::set<midikraft::Tag> const& tags)
	{
		ignoreUnused(tags);
		SimpleLogger::instance()->postMessage("Changing tags in the stored patch is not implemented yet!");
		return false;
	}

	std::set<midikraft::Tag> GenericStoredTagCapability::tags() const
	{
		if (me_.lock()->pythonModuleHasFunction(kGetStoredTags)) {
			py::gil_scoped_acquire acquire;
			if (!me_.expired()) {
				auto patch = me_.lock();
				try {
					std::vector<int> v(me_.lock()->data().data(), me_.lock()->data().data() + me_.lock()->data().size());
					py::object result = patch->callMethod(kGetStoredTags, v);
					auto tagsFound = result.cast<std::vector<std::string>>();
					std::set<midikraft::Tag> resultSet;
					for (auto const& tag : tagsFound)
					{
						resultSet.insert(tag);
					}
					return resultSet;
				}
				catch (py::error_already_set& ex)
				{
					if (!me_.expired())
						me_.lock()->logAdaptationError(kGetStoredTags, ex);
				}
				catch (...) {
					SimpleLogger::instance()->postMessage(fmt::format("Uncaught exception in {} of Patch of GenericAdaptation", kGetStoredTags));
				}
			}
		}
		return {};
	}


	bool GenericPatch::hasCapability(std::shared_ptr<midikraft::StoredPatchNameCapability> &outCapability) const
	{
		midikraft::StoredPatchNameCapability *impl;
		if (hasCapability(&impl)) {
			if (!genericStoredPatchNameCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch *non_const = const_cast<GenericPatch *>(this);
				non_const->genericStoredPatchNameCapabilityImpl_ = std::make_shared<GenericStoredPatchNameCapability>(non_const->shared_from_this());
			}
			outCapability = genericStoredPatchNameCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(midikraft::StoredPatchNameCapability **outCapability) const
	{
		if (pythonModuleHasFunction(kRenamePatch)) {
			if (!genericStoredPatchNameCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch *non_const = const_cast<GenericPatch *>(this);
				non_const->genericStoredPatchNameCapabilityImpl_ = std::make_shared<GenericStoredPatchNameCapability>(non_const->shared_from_this());
			}
			*outCapability = genericStoredPatchNameCapabilityImpl_.get();
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(std::shared_ptr<midikraft::DefaultNameCapability> &outCapability) const
	{
		midikraft::DefaultNameCapability *impl;
		if (hasCapability(&impl)) {
			if (!genericDefaultNameCapabilityImp_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch *non_const = const_cast<GenericPatch *>(this);
				non_const->genericDefaultNameCapabilityImp_ = std::make_shared<GenericDefaultNameCapability>(non_const->shared_from_this());
			}
			outCapability = genericDefaultNameCapabilityImp_;
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(midikraft::DefaultNameCapability **outCapability) const
	{
		if (pythonModuleHasFunction(kIsDefaultName)) {
			if (!genericDefaultNameCapabilityImp_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch *non_const = const_cast<GenericPatch *>(this);
				non_const->genericDefaultNameCapabilityImp_ = std::make_shared<GenericDefaultNameCapability>(non_const->shared_from_this());
			}
			*outCapability = genericDefaultNameCapabilityImp_.get();
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(std::shared_ptr<midikraft::LayeredPatchCapability>& outCapability) const
	{
		midikraft::LayeredPatchCapability* impl;
		if (hasCapability(&impl)) {
			if (!genericLayeredPatchCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch* non_const = const_cast<GenericPatch*>(this);
				non_const->genericLayeredPatchCapabilityImpl_ = std::make_shared<GenericLayeredPatchCapability>(non_const->shared_from_this());
			}
			outCapability = genericLayeredPatchCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(midikraft::LayeredPatchCapability** outCapability) const
	{
		if (pythonModuleHasFunction(kLayerName) && pythonModuleHasFunction(kNumberOfLayers)) {
			if (!genericLayeredPatchCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch* non_const = const_cast<GenericPatch*>(this);
				non_const->genericLayeredPatchCapabilityImpl_ = std::make_shared<GenericLayeredPatchCapability>(non_const->shared_from_this());
			}
			*outCapability = genericLayeredPatchCapabilityImpl_.get();
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(std::shared_ptr<midikraft::StoredTagCapability >& outCapability) const
	{
		midikraft::StoredTagCapability* impl;
		if (hasCapability(&impl)) {
			if (!genericStoredTagCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch* non_const = const_cast<GenericPatch*>(this);
				non_const->genericStoredTagCapabilityImpl_ = std::make_shared<GenericStoredTagCapability>(non_const->shared_from_this());
			}
			outCapability = genericStoredTagCapabilityImpl_;
			return true;
		}
		return false;
	}

	bool GenericPatch::hasCapability(midikraft::StoredTagCapability** outCapability) const
	{
		if (pythonModuleHasFunction(kGetStoredTags)) {
			if (!genericStoredTagCapabilityImpl_) {
				// Lazy init allowed despite const-ness of method. Smell.
				GenericPatch* non_const = const_cast<GenericPatch*>(this);
				non_const->genericStoredTagCapabilityImpl_ = std::make_shared<GenericStoredTagCapability>(non_const->shared_from_this());
			}
			*outCapability = genericStoredTagCapabilityImpl_.get();
			return true;
		}
		return false;
	}

}

