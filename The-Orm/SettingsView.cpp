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
	addAndMakeVisible(errorMessageInstead_);

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
	if (errorMessageInstead_.getText().isNotEmpty()) {
		int width = std::min(area.getWidth(), 600);
		errorMessageInstead_.setBounds(area.removeFromTop(100).withSizeKeepingCentre(width, 100).reduced(8));
		errorMessageInstead_.setVisible(true);
		propertyEditor_.setVisible(false);
	}
	else {
		// No error, show the property editor
		errorMessageInstead_.setVisible(false);
		propertyEditor_.setVisible(true);
		propertyEditor_.setBounds(editorArea.withSizeKeepingCentre(std::min(500, editorArea.getWidth()), editorArea.getHeight()));
	}
}

void SettingsView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);
	auto gsc = midikraft::Capability::hasCapability<midikraft::GlobalSettingsCapability>(UIModel::instance()->currentSynth_.smartSynth());
	if (gsc) {
		propertyEditor_.setProperties(gsc->getGlobalSettings());
		errorMessageInstead_.setText("", dontSendNotification);
		resized();
	}
	else {
		propertyEditor_.clear();
		if (UIModel::currentSynth()) {
			errorMessageInstead_.setText("The " + String(UIModel::currentSynth()->getName()) + " implementation does not support editing the global settings of the synth, sorry!", dontSendNotification);
		}
		else {
			errorMessageInstead_.setText("No Synth is selected. Please use the Setup tab to configure at least one synth");
		}
		resized();
	}
}

void SettingsView::loadGlobals() {
	auto synth = UIModel::instance()->currentSynth_.smartSynth();
	auto midiLocation = midikraft::Capability::hasCapability<midikraft::MidiLocationCapability>(synth);
	auto gsc = midikraft::Capability::hasCapability<midikraft::GlobalSettingsCapability>(synth);
	if (gsc && midiLocation) {
		errorMessageInstead_.setText("", dontSendNotification);
		librarian_.startDownloadingSequencerData(midikraft::MidiController::instance()->getMidiOutput(midiLocation->midiOutput()), gsc->loader(), gsc->settingsDataFileType(), 0, nullptr,
			[this, gsc](std::vector<midikraft::PatchHolder> dataLoaded) {
			gsc->setGlobalSettingsFromDataFile(dataLoaded[0].patch());
			MessageManager::callAsync([this, gsc]() {
				// Kick off an update so the property editor can refresh itself
				auto settings = gsc->getGlobalSettings();
				propertyEditor_.setProperties(settings);
				resized();
			});
		});
	}
}

