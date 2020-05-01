/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericAdaption.h"

#include <pybind11/embed.h>
#include <memory>

namespace py = pybind11;

namespace knobkraft {

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptionPythonEmbeddedGuard;

	class GenericAdaption::AdaptionImpl {
	public:
		AdaptionImpl(std::string const &filepath) : filepath_(filepath) {
			adaption_module = py::module::import(filepath_.c_str());
		}

		virtual ~AdaptionImpl() = default;

		int numberOfBanks() const {
			py::object result = adaption_module.attr("numberOfBanks")();
			return result.cast<int>();
		}

	private:
		py::module adaption_module;
		std::string filepath_;
	};

	GenericAdaption::GenericAdaption(std::string const &pythonModuleFilePath)
	{
		impl = std::make_unique<AdaptionImpl>(pythonModuleFilePath);
	}

	void GenericAdaption::startupGenericAdaption()
	{
		sGenericAdaptionPythonEmbeddedGuard = std::make_unique<py::scoped_interpreter>();
		py::exec("import sys\nsys.path.append(\"d:/Development/github/KnobKraft-Orm/adaptions\")\n");
	}


	int GenericAdaption::numberOfBanks() const
	{
		return impl->numberOfBanks();
	}

	int GenericAdaption::numberOfPatches() const
	{
		return 1;
	}

	std::string GenericAdaption::friendlyBankName(MidiBankNumber bankNo) const
	{
		ignoreUnused(bankNo);
		return "invalid Bank";
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaption::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		ignoreUnused(data, place);
		return {};
	}

	bool GenericAdaption::isOwnSysex(MidiMessage const &message) const
	{
		ignoreUnused(message);
		return false;
	}

	juce::MidiMessage GenericAdaption::deviceDetect(int channel)
	{
		ignoreUnused(channel);
		return false;
	}

	int GenericAdaption::deviceDetectSleepMS()
	{
		return 100;
	}

	MidiChannel GenericAdaption::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		ignoreUnused(message);
		return MidiChannel::invalidChannel();
	}

	bool GenericAdaption::needsChannelSpecificDetection()
	{
		return true;
	}


	std::string GenericAdaption::getName() const
	{
		return "Generic";
	}

}
