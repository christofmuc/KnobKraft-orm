/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SetupView.h"

#include "Synth.h"
#include "MidiChannelEntry.h"
#include "SoundExpanderCapability.h"
#include "Logger.h"
#include "AutoDetection.h"

#include "UIModel.h"

#include <boost/format.hpp>

SetupView::SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/) :
	autoDetection_(autoDetection)/*, lights_(lights) */, functionButtons_(1501, LambdaButtonStrip::Direction::Horizontal)
{
	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		auto button = new TextButton(synth.device()->getName());
		button->setRadioGroupId(112);
		buttons_.add(button);
		addAndMakeVisible(button);

		auto label = new Label();
		labels_.add(label);
		addAndMakeVisible(label);

		auto color = new ColourSelector(ColourSelector::showColourspace);
		
		color->addChangeListener(this);
		colours_.add(color);
		addAndMakeVisible(color);
	}
	refreshData();

	// Define function buttons
	functionButtons_.setButtonDefinitions({
			{
			"synthDetection", {0, "Quick check", [this]() {
				auto currentSynths = UIModel::instance()->synthList_.activeSynths();
				autoDetection_->quickconfigure(currentSynths); // This rather should be synchronous!
				refreshData();
			} } },
			{
			"autoconfigure",{1, "Rerun Autoconfigure", [this]() {
				auto currentSynths = UIModel::instance()->synthList_.activeSynths();
				autoDetection_->autoconfigure(currentSynths, nullptr);
				refreshData();
			} } } 
		});
	addAndMakeVisible(functionButtons_);
}

void SetupView::resized() {
	Rectangle<int> area(getLocalBounds());

	functionButtons_.setBounds(area.removeFromBottom(40).reduced(8));

	juce::Grid grid;
	grid.setGap(20_px);
	using Track = juce::Grid::TrackInfo;
	for (int i = 0; i < buttons_.size(); i++) grid.templateRows.add(Track(1_fr));
	for (int i = 0; i < 3; i++) grid.templateColumns.add(Track(1_fr));
	for (int i = 0; i < buttons_.size(); i++) {
		grid.items.add(juce::GridItem(buttons_[i]));
		grid.items.add(juce::GridItem(labels_[i]));
		grid.items.add(juce::GridItem(colours_[i]));
	}
	// Height of grid - max 100 * synths
	int height = std::min(100 * buttons_.size(), area.getHeight() - 16);
	grid.performLayout(area.removeFromTop(height).reduced(8));
}

void SetupView::refreshData() {
	int i = 0;
	for (auto &synth : UIModel::instance()->synthList_.allSynths()) {
		if (!synth.device()) continue;
		labels_[i]->setText(midiSetupDescription(synth.device()), dontSendNotification);
		labels_[i]->setColour(Label::ColourIds::backgroundColourId, synth.device()->channel().isValid() ? Colours::darkgreen : Colours::darkgrey);
		//colours_[i]->setCurrentColour(synth.color(), dontSendNotification);
		i++;
	}
}

void SetupView::changeListenerCallback(ChangeBroadcaster* source)
{
	// Find out which of the color selectors sent this message
	for (int i = 0; i < colours_.size(); i++) {
		if (colours_[i] == source) {
			auto newColour = colours_[i]->getCurrentColour();
			// Set the colour to our lights
			//lights_->setStudioLight(HueLightState(newColour), 0);
			// Persist the new colour in the synth
			//synths_[i].setColor(newColour);
		}
	}
}

std::string SetupView::midiSetupDescription(std::shared_ptr<midikraft::SimpleDiscoverableDevice> synth) const
{
	if (synth->channel().isValid()) {
		return (boost::format("Input: %s\nOutput: %s\nChannel: %d") %
			synth->midiInput() % synth->midiOutput() % synth->channel().toOneBasedInt()).str();
	}
	else {
		return "Not detected";
	}
}

