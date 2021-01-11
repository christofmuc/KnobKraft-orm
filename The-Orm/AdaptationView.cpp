/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "AdaptationView.h"

#include "Capability.h"

#include "EditBufferCapability.h"
#include "ProgramDumpCapability.h"
#include "BankDumpCapability.h"
#include "DataFileLoadCapability.h"

#include <boost/format.hpp>

namespace knobkraft {

	AdaptationView::AdaptationView()
	{
		addAndMakeVisible(adaptationInfo_);
	}

	void AdaptationView::setupForAdaptation(std::shared_ptr<GenericAdaptation> adaptationSynth)
	{
		std::string infoText;

		infoText = (boost::format("Implementation information for the adaptation for the '%s':\n\n") % adaptationSynth->getName()).str();

		bool failure = false;
		for (auto functionName : kMinimalRequiredFunctionNames) {
			if (!adaptationSynth->pythonModuleHasFunction(functionName)) {
				infoText += (boost::format("Error: Required function %s has not been implemented yet\n") % functionName).str();
				failure = true;
			}
		}
		if (!failure) {
			infoText += "All required functions have been implemented\n";
		} 
		infoText += "\n";

		auto hasEditBuffer = midikraft::Capability::hasCapability<midikraft::EditBufferCapability>(adaptationSynth);
		auto hasProgramDump = midikraft::Capability::hasCapability<midikraft::ProgramDumpCabability>(adaptationSynth);
		auto hasBankDump = midikraft::Capability::hasCapability<midikraft::BankDumpCapability>(adaptationSynth);
		auto hasDataFileLoad = midikraft::Capability::hasCapability<midikraft::DataFileLoadCapability>(adaptationSynth);

		infoText += (boost::format("Edit Buffer Capability has %sbeen implemented\n") % (hasEditBuffer ? "" : "not ")).str();
		infoText += (boost::format("Program Dump Capability has %sbeen implemented\n") % (hasProgramDump? "" : "not ")).str();
		infoText += (boost::format("Bank Dump Capability has %sbeen implemented\n") % (hasBankDump ? "" : "not ")).str();
		infoText += (boost::format("Data File Load Capability has %sbeen implemented\n") % (hasDataFileLoad? "" : "not ")).str();

		infoText += "\n\nImplemented functions:\n\n";
		for (auto functionName : kAdapatationPythonFunctionNames) {
			if (adaptationSynth->pythonModuleHasFunction(functionName)) {
				infoText += (boost::format("def %s()\n") % functionName).str();
			}
		}
		infoText += "\n\nNot implemented functions:\n\n";
		for (auto functionName : kAdapatationPythonFunctionNames) {
			if (!adaptationSynth->pythonModuleHasFunction(functionName)) {
				infoText += (boost::format("def %s()\n") % functionName).str();
			}
		}

		adaptationInfo_.setText(infoText, false);
	}

	void AdaptationView::resized()
	{
		auto area = getLocalBounds();
		int width = std::min(area.getWidth(), 600);
		int height = std::min(area.getHeight(), 800);
		adaptationInfo_.setBounds(area.removeFromTop(height).withTrimmedBottom(8).withSizeKeepingCentre(width, height).reduced(8));
	}

}