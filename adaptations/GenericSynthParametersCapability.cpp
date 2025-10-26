/*
   Copyright (c) 2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericSynthParametersCapability.h"

#include "GenericAdaptation.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

namespace py = pybind11;

namespace knobkraft {
	namespace {

		void assignPatchBytesToDataFile(py::handle object, std::shared_ptr<midikraft::DataFile> const& patch)
		{
			if (!patch || object.is_none()) {
				return;
			}
			if (py::isinstance<py::sequence>(object) && !py::isinstance<py::str>(object)) {
				auto bytes = object.cast<std::vector<int>>();
				midikraft::Synth::PatchData newData;
				newData.reserve(bytes.size());
				for (auto byte : bytes) {
					newData.push_back(static_cast<uint8>(std::clamp(byte, 0, 255)));
				}
				patch->setData(newData);
			}
		}

		juce::var pyObjectToVar(py::handle object)
		{
			if (object.is_none()) {
				return {};
			}
			if (py::isinstance<py::bool_>(object)) {
				return juce::var(py::cast<bool>(object));
			}
			if (py::isinstance<py::int_>(object)) {
				return juce::var(py::cast<int>(object));
			}
			if (py::isinstance<py::float_>(object)) {
				return juce::var(py::cast<double>(object));
			}
			if (py::isinstance<py::str>(object)) {
				return juce::var(py::cast<std::string>(object));
			}
			if (py::isinstance<py::list>(object) || py::isinstance<py::tuple>(object)) {
				juce::Array<juce::var> array;
				py::sequence seq = py::reinterpret_borrow<py::sequence>(object);
				for (auto entry : seq) {
					array.add(pyObjectToVar(entry));
				}
				return juce::var(array);
			}
			return juce::var(py::str(object).cast<std::string>());
		}

		py::object varToPyObject(juce::var const& value)
		{
			if (value.isVoid()) {
				return py::none();
			}
			if (value.isBool()) {
				return py::bool_(static_cast<bool>(value));
			}
			if (value.isInt64()) {
				return py::int_(static_cast<juce::int64>(value));
			}
			if (value.isInt()) {
				return py::int_(static_cast<int>(value));
			}
			if (value.isDouble()) {
				return py::float_(static_cast<double>(value));
			}
			if (value.isString()) {
				return py::str(value.toString().toStdString());
			}
			if (value.isArray()) {
				py::list list;
				auto* values = value.getArray();
				if (values != nullptr) {
					for (auto const& entry : *values) {
						list.append(varToPyObject(entry));
					}
				}
				return list;
			}
			return py::str(value.toString().toStdString());
		}

		midikraft::ParamType interpretParamType(py::handle object)
		{
			if (py::isinstance<py::int_>(object)) {
				int typeIndex = py::cast<int>(object);
				typeIndex = std::clamp(typeIndex, 0, 3);
				return static_cast<midikraft::ParamType>(typeIndex);
			}
			if (py::isinstance<py::str>(object)) {
				auto typeString = py::cast<std::string>(object);
				std::transform(typeString.begin(), typeString.end(), typeString.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (typeString == "choice") {
					return midikraft::ParamType::CHOICE;
				}
				if (typeString == "list" || typeString == "vector") {
					return midikraft::ParamType::LIST;
				}
				if (typeString == "choice_list" || typeString == "choicelist") {
					return midikraft::ParamType::CHOICE_LIST;
				}
			}
			return midikraft::ParamType::VALUE;
		}

		std::optional<int> optionalInt(py::handle object)
		{
			if (object.is_none()) {
				return std::nullopt;
			}
			if (py::isinstance<py::int_>(object)) {
				return py::cast<int>(object);
			}
			return std::nullopt;
		}

		std::optional<midikraft::ParamDef> parseParamDef(py::handle object)
		{
			midikraft::ParamDef def{};
			if (py::isinstance<py::dict>(object)) {
				py::dict dict = py::reinterpret_borrow<py::dict>(object);
				if (!dict.contains("param_id") || !dict.contains("name") || !dict.contains("param_type")) {
					return std::nullopt;
				}
				def.param_id = py::cast<int>(dict["param_id"]);
				def.name = dict.contains("name") ? py::cast<std::string>(dict["name"]) : "";
				def.description = dict.contains("description") ? py::cast<std::string>(dict["description"]) : "";
				def.param_type = interpretParamType(dict["param_type"]);
				if (dict.contains("values")) {
					def.values = pyObjectToVar(dict["values"]);
				}
				if (dict.contains("cc_number")) {
					def.cc_number = optionalInt(dict["cc_number"]);
				}
				if (dict.contains("nrpn_number")) {
					def.nrpn_number = optionalInt(dict["nrpn_number"]);
				}
				return def;
			}
			if (py::isinstance<py::sequence>(object) && !py::isinstance<py::str>(object)) {
				py::sequence seq = py::reinterpret_borrow<py::sequence>(object);
				if (seq.size() < 4) {
					return std::nullopt;
				}
				def.param_id = py::cast<int>(seq[0]);
				def.name = py::cast<std::string>(seq[1]);
				def.description = seq.size() > 2 ? py::cast<std::string>(seq[2]) : "";
				def.param_type = interpretParamType(seq[3]);
				if (seq.size() > 4) {
					def.values = pyObjectToVar(seq[4]);
				}
				if (seq.size() > 5) {
					def.cc_number = optionalInt(seq[5]);
				}
				if (seq.size() > 6) {
					def.nrpn_number = optionalInt(seq[6]);
				}
				return def;
			}
			return std::nullopt;
		}

		std::optional<midikraft::ParamVal> parseParamVal(py::handle object)
		{
			midikraft::ParamVal val{};
			if (py::isinstance<py::dict>(object)) {
				py::dict dict = py::reinterpret_borrow<py::dict>(object);
				if (!dict.contains("param_id") || !dict.contains("value")) {
					return std::nullopt;
				}
				val.param_id = py::cast<int>(dict["param_id"]);
				val.value = pyObjectToVar(dict["value"]);
				return val;
			}
			if (py::isinstance<py::sequence>(object) && !py::isinstance<py::str>(object)) {
				py::sequence seq = py::reinterpret_borrow<py::sequence>(object);
				if (seq.size() < 2) {
					return std::nullopt;
				}
				val.param_id = py::cast<int>(seq[0]);
				val.value = pyObjectToVar(seq[1]);
				return val;
			}
			return std::nullopt;
		}

		std::vector<int> patchToIntVector(std::shared_ptr<midikraft::DataFile> const& patch)
		{
			std::vector<int> result;
			if (!patch) {
				return result;
			}
			auto data = patch->data();
			result.reserve(data.size());
			for (auto byte : data) {
				result.push_back(static_cast<int>(byte));
			}
			return result;
		}
	}

	std::vector<midikraft::ParamDef> GenericSynthParametersCapability::getParameterDefinitions() const
	{
		py::gil_scoped_acquire acquire;
		std::vector<midikraft::ParamDef> result;
		if (!me_->pythonModuleHasFunction(kGetParameterDefinitions)) {
			return result;
		}
		try {
			py::object pythonResult = me_->callMethod(kGetParameterDefinitions);
			if (pythonResult.is_none()) {
				return result;
			}
			py::sequence definitions = py::reinterpret_borrow<py::sequence>(pythonResult);
			result.reserve(static_cast<size_t>(definitions.size()));
			for (auto entry : definitions) {
				if (auto parsed = parseParamDef(entry)) {
					result.push_back(std::move(*parsed));
				}
				else {
					spdlog::warn("GenericSynthParametersCapability: Ignoring invalid parameter definition entry returned from Python");
				}
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kGetParameterDefinitions, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kGetParameterDefinitions, ex);
		}
		return result;
	}

	std::vector<midikraft::ParamVal> GenericSynthParametersCapability::getParameterValues(std::shared_ptr<midikraft::DataFile> const patch, bool onlyActive) const
	{
		py::gil_scoped_acquire acquire;
		std::vector<midikraft::ParamVal> result;
		if (!patch || !me_->pythonModuleHasFunction(kGetParameterValues)) {
			return result;
		}
		try {
			auto parameters = patchToIntVector(patch);
			bool onlyActiveCopy = onlyActive;
			py::object pythonResult = me_->callMethod(kGetParameterValues, parameters, onlyActiveCopy);
			if (pythonResult.is_none()) {
				return result;
			}
			py::sequence values = py::reinterpret_borrow<py::sequence>(pythonResult);
			result.reserve(static_cast<size_t>(values.size()));
			for (auto entry : values) {
				if (auto parsed = parseParamVal(entry)) {
					result.push_back(std::move(*parsed));
				}
				else {
					spdlog::warn("GenericSynthParametersCapability: Ignoring invalid parameter value entry returned from Python");
				}
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kGetParameterValues, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kGetParameterValues, ex);
		}
		return result;
	}

	bool GenericSynthParametersCapability::setParameterValues(std::shared_ptr<midikraft::DataFile> patch, std::vector<midikraft::ParamVal> const& new_values) const
	{
		py::gil_scoped_acquire acquire;
		if (!me_->pythonModuleHasFunction(kSetParameterValues)) {
			return false;
		}
		try {
			py::list parameter_values;
			for (auto const& value : new_values) {
				py::dict entry;
				entry["param_id"] = value.param_id;
				entry["value"] = varToPyObject(value.value);
				parameter_values.append(entry);
			}
			auto patch_bytes = patchToIntVector(patch);
			py::object pythonResult = me_->callMethod(kSetParameterValues, patch_bytes, parameter_values);
			bool changed = false;
			if (py::isinstance<py::tuple>(pythonResult)) {
				py::tuple tupleResult = pythonResult;
				if (tupleResult.size() > 0) {
					changed = tupleResult[0].cast<bool>();
				}
				if (tupleResult.size() > 1) {
					assignPatchBytesToDataFile(tupleResult[1], patch);
				}
				return changed;
			}
			if (py::isinstance<py::sequence>(pythonResult) && !py::isinstance<py::str>(pythonResult)) {
				assignPatchBytesToDataFile(pythonResult, patch);
				return true;
			}
			return pythonResult.cast<bool>();
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kSetParameterValues, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kSetParameterValues, ex);
		}
		return false;
	}

	std::vector<MidiMessage> GenericSynthParametersCapability::createSetValueMessages(MidiChannel const channel, std::shared_ptr<midikraft::DataFile> const patch, std::vector<int> param_ids) const
	{
		py::gil_scoped_acquire acquire;
		std::vector<MidiMessage> result;
		if (!patch || !me_->pythonModuleHasFunction(kCreateSetValueMessages)) {
			return result;
		}
		try {
			int python_channel = channel.isValid() ? channel.toZeroBasedInt() : 0;
			auto patch_bytes = patchToIntVector(patch);
			py::list parameter_ids;
			for (auto param_id : param_ids) {
				parameter_ids.append(param_id);
			}
			py::object pythonResult = me_->callMethod(kCreateSetValueMessages, python_channel, patch_bytes, parameter_ids);
			if (!pythonResult.is_none()) {
				if (py::isinstance<py::tuple>(pythonResult)) {
					py::tuple tupleResult = pythonResult;
					if (tupleResult.size() >= 1) {
						auto midiData = tupleResult[0].cast<std::vector<int>>();
						result = GenericAdaptation::vectorToMessages(midiData);
					}
					if (tupleResult.size() >= 2) {
						assignPatchBytesToDataFile(tupleResult[1], patch);
					}
				}
				else if (py::isinstance<py::sequence>(pythonResult) && !py::isinstance<py::str>(pythonResult)) {
					auto midiData = pythonResult.cast<std::vector<int>>();
					result = GenericAdaptation::vectorToMessages(midiData);
				}
				else {
					auto midiData = pythonResult.cast<std::vector<int>>();
					result = GenericAdaptation::vectorToMessages(midiData);
				}
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kCreateSetValueMessages, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kCreateSetValueMessages, ex);
		}
		return result;
	}

	std::vector<float> GenericSynthParametersCapability::createFeatureVector(std::shared_ptr<midikraft::DataFile> const patch) const
	{
		py::gil_scoped_acquire acquire;
		std::vector<float> result;
		if (!patch || !me_->pythonModuleHasFunction(kCreateFeatureVector)) {
			return result;
		}
		try {
			auto parameters = patchToIntVector(patch);
			py::object pythonResult = me_->callMethod(kCreateFeatureVector, parameters);
			if (!pythonResult.is_none()) {
				result = pythonResult.cast<std::vector<float>>();
			}
		}
		catch (py::error_already_set& ex) {
			me_->logAdaptationError(kCreateFeatureVector, ex);
			ex.restore();
		}
		catch (std::exception& ex) {
			me_->logAdaptationError(kCreateFeatureVector, ex);
		}
		return result;
	}

}
