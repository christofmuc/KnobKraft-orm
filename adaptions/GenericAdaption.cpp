/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericAdaption.h"

#include "Logger.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>
//#include <pybind11/pybind11.h>
#include <memory>

namespace py = pybind11;

#include <boost/format.hpp>

namespace knobkraft {

	std::unique_ptr<py::scoped_interpreter> sGenericAdaptionPythonEmbeddedGuard;

	class GenericAdaption::AdaptionImpl {
	public:
		AdaptionImpl(std::string const &filepath) : filepath_(filepath) {
			try {
				adaption_module = py::module::import(filepath_.c_str());
			}
			catch (py::error_already_set &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Failure loading python module: %s") % ex.what()).str());
			}
		}

		virtual ~AdaptionImpl() = default;

		int numberOfBanks() const {
			try {
				py::object result = adaption_module.attr("numberOfBanks")();
				return result.cast<int>();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling numberOfBanks: %s") % ex.what()).str());
				return 1;
			}
		}

		int numberOfPatches() const
		{
			try {
				py::object result = adaption_module.attr("numberOfPatchesPerBank")();
				return result.cast<int>();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling numberOfPatchesPerBank: %s") % ex.what()).str());
				return 0;
			}
		}

		juce::MidiMessage deviceDetect(int channel)
		{
			try {
				py::object result = adaption_module.attr("createDeviceDetectMessage")(channel);
				return vectorToMessage(result.cast<std::vector<int>>());
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling createDeviceDetectMessage: %s") % ex.what()).str());
				return MidiMessage();
			}
		}

		int deviceDetectSleepMS()
		{
			try
			{
				py::object result = adaption_module.attr("deviceDetectWaitMilliseconds")();
				return result.cast<int>();

			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling deviceDetectSleepMS: %s") % ex.what()).str());
				return 100;
			}
		}

		MidiChannel channelIfValidDeviceResponse(const MidiMessage &message)
		{
			try {
				py::object result = adaption_module.attr("channelIfValidDeviceResponse")(messageToVector(message));
				int intResult = result.cast<int>();
				if (intResult >= 0 && intResult < 16) {
					return MidiChannel::fromZeroBase(intResult);
				}
				else {
					return MidiChannel::invalidChannel();
				}
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling channelIfValidDeviceResponse: %s") % ex.what()).str());
				return MidiChannel::invalidChannel();
			}
		}

		bool needsChannelSpecificDetection()
		{
			try
			{
				py::object result = adaption_module.attr("needsChannelSpecificDetection")();
				return result.cast<bool>();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling needsChannelSpecificDetection: %s") % ex.what()).str());
				return true;
			}
		}

		std::string getName() const
		{
			try {
				py::object result = adaption_module.attr("name")();
				return result.cast<std::string>();
			}
			catch (std::exception &ex) {
				SimpleLogger::instance()->postMessage((boost::format("Error calling getName: %s") % ex.what()).str());
				return "Generic";
			}
		}



	private:
		std::vector<int> messageToVector(MidiMessage const &message) {
			return std::vector<int>(message.getRawData(), message.getRawData() + message.getRawDataSize());
		}

		MidiMessage vectorToMessage(std::vector<int> const &data) {
			return MidiMessage(data.data(), (int)data.size());
		}

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
		return impl->numberOfPatches();
	}

	std::string GenericAdaption::friendlyBankName(MidiBankNumber bankNo) const
	{
		//TODO could delegate this to the python code as well
		return (boost::format("Bank %d") % bankNo.toOneBased()).str();
	}

	std::shared_ptr<midikraft::DataFile> GenericAdaption::patchFromPatchData(const Synth::PatchData &data, MidiProgramNumber place) const
	{
		ignoreUnused(data, place);
		return {};
	}

	bool GenericAdaption::isOwnSysex(MidiMessage const &message) const
	{
		//TODO - if we delegate this to the python code, the "sniff synth" method of the Librarian can be used. But this is currently disabled anyway,
		// even if I forgot why
		ignoreUnused(message);
		return false;
	}

	juce::MidiMessage GenericAdaption::deviceDetect(int channel)
	{
		return impl->deviceDetect(channel);
	}

	int GenericAdaption::deviceDetectSleepMS()
	{
		return impl->deviceDetectSleepMS();
	}

	MidiChannel GenericAdaption::channelIfValidDeviceResponse(const MidiMessage &message)
	{
		return impl->channelIfValidDeviceResponse(message);
	}

	bool GenericAdaption::needsChannelSpecificDetection()
	{
		return impl->needsChannelSpecificDetection();
	}

	std::string GenericAdaption::getName() const
	{
		return impl->getName();
	}

}
