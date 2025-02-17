/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericPatch.h"

#include "GenericAdaptation.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/stl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace py = pybind11;

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace knobkraft {

	GenericPatch::GenericPatch(GenericAdaptation const* me, pybind11::module& adaptation_module, midikraft::Synth::PatchData const& data, DataType dataType) : midikraft::DataFile(dataType, data), me_(me), adaptation_(adaptation_module)
	{
		if (pythonModuleHasFunction(kNameFromDump)) {
			midikraft::globalCapabilityRegistry.registerCapability<midikraft::StoredPatchNameCapability>(this, new GenericStoredPatchNameCapability(this, me));
		}
		if (pythonModuleHasFunction(kIsDefaultName)) {
			midikraft::globalCapabilityRegistry.registerCapability<midikraft::DefaultNameCapability>(this, new GenericDefaultNameCapability(this));
		}
		if (pythonModuleHasFunction(kLayerName) && pythonModuleHasFunction(kNumberOfLayers)) {
			midikraft::globalCapabilityRegistry.registerCapability<midikraft::LayeredPatchCapability>(this, new GenericLayeredPatchCapability(this));
		}
		if (pythonModuleHasFunction(kGetStoredTags)) {
			midikraft::globalCapabilityRegistry.registerCapability<midikraft::StoredTagCapability>(this, new GenericStoredTagCapability(this));
		}
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
					// Check if we have extracted this name before
					std::string cachedName;
					if (adaptation_&& adaptation_->hasName(patch->data(), cachedName)) {
						spdlog::trace("name cache hit: {}", cachedName);
						return cachedName;
					}
					std::vector<int> v(patch->data().data(), patch->data().data() + patch->data().size());
					auto result = patch->callMethod(kNameFromDump, v);
					checkForPythonOutputAndLog();
					std::string extractedName = result.cast<std::string>();
					spdlog::trace("extracted name via Python: {}", extractedName);
					if (adaptation_) {
						adaptation_->insertName(patch->data(), extractedName);
					}
					return extractedName;
				}
				catch (py::error_already_set& ex) {
					std::string errorMessage = fmt::format("Error calling {}: {}", kNameFromDump, ex.what());
					ex.restore(); // Prevent a deadlock https://github.com/pybind/pybind11/issues/1490
					spdlog::error(errorMessage);
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
			spdlog::error("Adaptation[{}]: Error calling {}: {}", adaptionName, methodCopy, exceptionMessage);
		});
	}

	bool GenericStoredPatchNameCapability::changeNameStoredInPatch(std::string const &newName)
	{
		if (name() == newName) {
			// No need to change the name, as the current calculation already gives the correct result
			return true;
		}
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			// set name is an optional method - if it is not implemented, the name in the patch is never changed, the name displayed in the Librarian is
			if (!me_.lock()->pythonModuleHasFunction(kRenamePatch)) return false;

			// Very well, then try to change the name in the patch data
			try {
				std::vector<int> v(me_.lock()->data().data(), me_.lock()->data().data() + me_.lock()->data().size());
				py::object result = me_.lock()->callMethod(kRenamePatch, v, newName);
				auto intVector = result.cast<std::vector<int>>();
				std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(intVector);
				me_.lock()->setData(byteData);
				return true;
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
				spdlog::error("Adaptation[unknown]: Uncaught exception in {} of Patch of GenericAdaptation", kRenamePatch);
			}
		}
		return false;
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
				spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation", kIsDefaultName);
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
				spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation", kNumberOfLayers);
			}
		}
		return 1;
	}

	std::vector<std::string> GenericLayeredPatchCapability::layerTitles() const {
		py::gil_scoped_acquire acquire;
		if (!me_.expired()) {
			auto patch = me_.lock();
			if (patch->pythonModuleHasFunction(kLayerTitles)) {
				try {
					py::object result = patch->callMethod(kLayerTitles);
					return py::cast<std::vector<std::string>>(result);
				}
				catch (py::error_already_set& ex) {
					if (!me_.expired())
						me_.lock()->logAdaptationError(kLayerTitles, ex);
					ex.restore();
				}
				catch (std::exception& ex) {
					if (!me_.expired())
						me_.lock()->logAdaptationError(kLayerTitles, ex);
				}
				catch (...) {
					spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation", kLayerTitles);
				}
			}
		}
		return {};
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
				spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation",  kLayerName);
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
					spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation", kSetLayerName);
				}
			}
		}
		else {
			spdlog::warn("Adaptation did not implement setLayerName(), can't rename layer");
		}
	}

	bool GenericStoredTagCapability::setTags(std::set<midikraft::Tag> const& tags)
	{
		ignoreUnused(tags);
		spdlog::warn("Changing tags in the stored patch is not implemented yet!");
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
					spdlog::error("Uncaught exception in {} of Patch of GenericAdaptation", kGetStoredTags);
				}
			}
		}
		return {};
	}

}
