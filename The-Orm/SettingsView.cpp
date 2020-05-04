/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SettingsView.h"

#include "DataFileLoadCapability.h"
#include "MidiController.h"

#include "UIModel.h"

#include "GlobalSettingsCapability.h"


SettingsView::SettingsView(std::vector<midikraft::SynthHolder> const &synths) : synths_(synths), librarian_(synths), 
	buttonStrip_(3001, LambdaButtonStrip::Direction::Horizontal)
{
	LambdaButtonStrip::TButtonMap buttons = {
	{ "loadGlobals", { 0, "Load Globals", [this]() {
		loadGlobals();
	} } },
	};
	buttonStrip_.setButtonDefinitions(buttons);
	addAndMakeVisible(buttonStrip_);
	addAndMakeVisible(propertyEditor_);

	UIModel::instance()->currentSynth_.addChangeListener(this);
}

SettingsView::~SettingsView()
{
	UIModel::instance()->currentSynth_.removeChangeListener(this);
}

void SettingsView::resized()
{
	auto area = getLocalBounds();
	buttonStrip_.setBounds(area.removeFromBottom(60).reduced(8));
	auto editorArea = area.reduced(10);
	propertyEditor_.setBounds(editorArea.withSizeKeepingCentre(std::min(500, editorArea.getWidth()), editorArea.getHeight()));
}

void SettingsView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	auto gsc = dynamic_cast<midikraft::GlobalSettingsCapability *>(UIModel::currentSynth());
	if (gsc) {
		propertyEditor_.setProperties(gsc->getGlobalSettings());
	}
	else {
		propertyEditor_.clear();
	}
}

void SettingsView::loadGlobals() {
	auto synth = UIModel::currentSynth();
	auto gsc = dynamic_cast<midikraft::GlobalSettingsCapability *>(synth);
	if (gsc) {
		librarian_.startDownloadingSequencerData(midikraft::MidiController::instance()->getMidiOutput(synth->midiOutput()), gsc->loader(), gsc->settingsDataFileType(), nullptr,
			[this, gsc](std::vector<std::shared_ptr<midikraft::DataFile>> dataLoaded) {
			gsc->setGlobalSettingsFromDataFile(dataLoaded[0]);
			MessageManager::callAsync([this, gsc]() {
				// Kick off an update so the property editor can refresh itself
				auto settings = gsc->getGlobalSettings();
				propertyEditor_.setProperties(settings);
			});
		});
	}
}

