/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ScriptedQuery.h"

#include "embedded_module.h"
#include "PyTschirpPatch.h"

#include "Logger.h"

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4100 )
#endif
#include <pybind11/embed.h>
#include <spdlog/spdlog.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace py = pybind11;
using namespace pybind11::literals;

std::vector<midikraft::PatchHolder> ScriptedQuery::filterByPredicate(std::string const &pythonPredicate, std::vector<midikraft::PatchHolder> const &input) const
{
	if (pythonPredicate.empty()) {
		return input;
	}

	std::vector<midikraft::PatchHolder> result;

	for (const auto& patch : input) {
		// Make sure that we have a PyTschirp object with the name of the synth
		std::string pythonClassName = findPyTschirpModuleForSynth(patch.synth()->getName());

		// Run some python code!
		auto pytschirpee = py::module::import("pytschirpee");

		// Create the patch in question using the PyTschirpPatch class
		PyTschirp pythonPatch(patch.patch(), patch.smartSynth());

		// Define the object to operate on
		py::object obj = py::cast(pythonPatch);
		auto locals = py::dict("p"_a = obj, **pytschirpee.attr("__dict__"));

		// Run the query
		try {
			auto queryResult = py::eval(pythonPredicate, py::globals(), locals);
			if (py::isinstance<py::bool_>(queryResult)) {
				if (py::bool_(queryResult)) {
					result.push_back(patch);
				}
			}
			else {
				// Abort with an error message
				spdlog::error("Error with scripted query - expression did not return True or False but {}", (std::string) py::str(queryResult));
				return input;
			}
		}
		catch (py::error_already_set &e) {
			spdlog::error("Error with scripted query: {}", e.what());
			return input;
		}
	}
	return result;
}
