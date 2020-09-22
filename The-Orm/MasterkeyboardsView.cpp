#include "MasterkeyboardsView.h"

#include "SimpleDiscoverableDevice.h"

#include "MidiController.h"
#include "AutoDetection.h"
#include "UIModel.h"

#include <boost/format.hpp>
#include "MidiClockCapability.h"

class ButtonClickLambda : public Button::Listener {
public:
	ButtonClickLambda(std::function<void(Button *)> clickHandler)
		: clickHandler_(clickHandler)
	{
	}

	void buttonClicked(Button *button)
	{
		clickHandler_(button);
	}

private:
	std::function<void(Button *)> clickHandler_;
};

MasterkeyboardsView::MasterkeyboardsView(midikraft::AutoDetection &autoDetection)
{
	recreate();

	// Subscribe to updates 
	UIModel::instance()->synthList_.addChangeListener(this);
	autoDetection.addChangeListener(this);
}


MasterkeyboardsView::~MasterkeyboardsView()
{
	UIModel::instance()->synthList_.removeChangeListener(this);
}

Component *addClockTypes(std::shared_ptr<midikraft::MidiClockCapability> clocks) {
	
	StringArray choices;
	Array<var> values;
	for (midikraft::MidiClockCapability::ClockMode v : clocks->getSupportedClockModes()) {
		switch (v) {
		case midikraft::MidiClockCapability::ClockMode::OFF:
			choices.add("Off"); values.add(static_cast<int>(v));
			break;
		case midikraft::MidiClockCapability::ClockMode::MASTER:
			choices.add("Master"); values.add(static_cast<int>(v));
			break;
		case midikraft::MidiClockCapability::ClockMode::SLAVE:
			choices.add("Slave"); values.add(static_cast<int>(v));
			break;
		case midikraft::MidiClockCapability::ClockMode::SLAVE_NO_START_STOP:
			choices.add("Slave no start/stop"); values.add(static_cast<int>(v));
			break;
		case midikraft::MidiClockCapability::ClockMode::SLAVE_THROUGH:
			choices.add("Slave through"); values.add(static_cast<int>(v));
			break;
		}
	}
	return new ChoicePropertyComponent(clocks->getMidiClockModeValue(), "Clock", choices, values);
}

void MasterkeyboardsView::recreate() {
	auto synths = UIModel::instance()->synthList_.activeSynths();


	// Remove previous UI
	keyboards_.clear();
	keyboardChannels_.clear();
	expanderClockMode_.clear();
	keyboadLocalButtons_.clear();
	expanders_.clear();
	expanderChannels_.clear();
	for (auto checkmarks : buttonsForExpander_) {
		for (auto button : checkmarks.second) {
			delete button;
		}
	}
	buttonsForExpander_.clear();

	// Recreate UI
	for (auto keyboard : synths) {
		auto masterkeyboard = std::dynamic_pointer_cast<midikraft::MasterkeyboardCapability>(keyboard);
		if (masterkeyboard) {
			// Yep, add one keyboard
			auto name = new Label("nameComponent", keyboard->getName());
			keyboards_.add(name);
			addAndMakeVisible(name);
			auto channelEntry = new MidiChannelEntry([masterkeyboard, this](MidiChannel newChannel) {
				masterkeyboard->changeOutputChannel(midikraft::MidiController::instance(), newChannel, [masterkeyboard, newChannel]() {
					// Should be simple discoverable?
					auto device = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(masterkeyboard);
					if (device) {
						midikraft::AutoDetection::persistSetting(device.get());
					}
				});
				
				refreshCheckmarks();
			});
			keyboardChannels_.add(channelEntry);
			addAndMakeVisible(channelEntry);
			ToggleButton *keyboardLocalButton = nullptr;
			if (masterkeyboard->hasLocalControl()) {
				keyboardLocalButton = new ToggleButton("Local control");
				addAndMakeVisible(keyboardLocalButton);
				keyboardLocalButton->addListener(new ButtonClickLambda([masterkeyboard, this](Button *button) {
					masterkeyboard->setLocalControl(midikraft::MidiController::instance(), button->getToggleState());
					refreshCheckmarks();
				}));
			}
			keyboadLocalButtons_.add(keyboardLocalButton);
		}
	}

	for (auto synth: synths) {
		auto expander = std::dynamic_pointer_cast<midikraft::SoundExpanderCapability>(synth);
		if (expander) {
			auto name = new Label("nameComponent", expander->getName());
			expanders_.add(name);
			addAndMakeVisible(name);
			auto channelEntry = new MidiChannelEntry([&expander, this](MidiChannel newChannel) {
				expander->changeInputChannel(midikraft::MidiController::instance(), newChannel, [this]() {
					refreshCheckmarks();
				});
			});
			expanderChannels_.add(channelEntry);
			addAndMakeVisible(channelEntry);
		}
	}

	// Now add checkmarks for each expander that will show which master keyboard controls it
	for (auto expander : expanders_) {
		auto expanderCap = expanderWithName(expander->getText().toStdString());

		auto midiClockCap = std::dynamic_pointer_cast<midikraft::MidiClockCapability>(expanderCap);
		if (midiClockCap) {
			auto clocks = addClockTypes(midiClockCap);
			expanderClockMode_.add(clocks);
			addAndMakeVisible(clocks);
		}
		else {
			expanderClockMode_.add(new Label());
		}

		buttonsForExpander_[expander->getText().toStdString()] = std::vector<ToggleButton *>();
		for (auto keyboard : keyboards_) {
			ignoreUnused(keyboard);
			auto button = new ToggleButton("");
			button->setEnabled(false);
			addAndMakeVisible(button);

			// Create a button that will switch the sound expander to the channel of the keyboard
			/*auto handler = new ButtonClickLambda(midiController, 
				expanderCap, 
				keyboardWithName(keyboard->getText().toStdString()),
				[this]() { refreshCheckmarks();  });
			listeners_.add(handler);
			button->addListener(handler);*/
			buttonsForExpander_[expander->getText().toStdString()].push_back(button);
		}

		ToggleButton *offButton = nullptr;
		if (expanderCap->hasMidiControl()) {
			offButton = new ToggleButton("MIDI Control");
			addAndMakeVisible(offButton);
			auto handler = new ButtonClickLambda(
				[this, expanderCap](Button *button) { 
				// Toggle MIDI Control 
				expanderCap->setMidiControl(midikraft::MidiController::instance(), button->getToggleState());
				refreshCheckmarks();  
			});
			offButton->addListener(handler);
		}
		buttonsForExpander_[expander->getText().toStdString()].push_back(offButton);
	}

	addAndMakeVisible(header);
	
	refreshCheckmarks();
	resized();
}

void MasterkeyboardsView::resized()
{
	juce::Grid grid;
	grid.setGap(20_px);
	using Track = juce::Grid::TrackInfo;
	for (int i = 0; i < expanders_.size() + 3; i++) grid.templateRows.add(Track(1_fr));
	for (int i = 0; i < keyboards_.size() + 4; i++) grid.templateColumns.add(Track(1_fr));

	// Create header row
	grid.items.add(juce::GridItem(header));
	grid.items.add(GridItem());
	grid.items.add(GridItem());
	for (auto keyboard : keyboards_) {
		grid.items.add(juce::GridItem(keyboard));
	}
	grid.items.add(juce::GridItem());

	// Row with keyboard channels
	grid.items.add(GridItem());
	grid.items.add(GridItem());
	grid.items.add(GridItem());
	for (auto keyboard : keyboardChannels_) {
		grid.items.add(juce::GridItem(keyboard));
	}
	grid.items.add(GridItem());

	// Row with MIDI local control switches
	grid.items.add(GridItem());
	grid.items.add(GridItem());
	grid.items.add(GridItem());
	for (auto keyboard : keyboadLocalButtons_) {
		grid.items.add(juce::GridItem(keyboard));
	}
	grid.items.add(GridItem());

	// Create one row per synth expander/sound-producing module
	for (int e = 0; e < expanders_.size(); e++) {
		grid.items.add(juce::GridItem(expanders_[e]));
		grid.items.add(juce::GridItem(expanderClockMode_[e]));
		grid.items.add(GridItem(expanderChannels_[e]));
		for (int i = 0; i < keyboards_.size() + 1; i++) {
			auto button = buttonsForExpander_[expanders_[e]->getText().toStdString()][i];
			grid.items.add(juce::GridItem(button));
		}
	}

	grid.justifyContent = Grid::JustifyContent::center;
	grid.performLayout(getLocalBounds().reduced(10));
}

void MasterkeyboardsView::changeListenerCallback(ChangeBroadcaster* source)
{
	ignoreUnused(source);

	// That's called when the synth setup changed
	recreate();
}

void MasterkeyboardsView::refreshCheckmarks() {
	// See which check marks need to be set - those where the MIDI channel of the keyboard matches the expander
	for (int row = 0; row < expanders_.size(); row++) {
		auto expander = expanders_[row];
		auto expanderName = expander->getText().toStdString();
		auto e = expanderWithName(expanderName);
		expanderChannels_[row]->setValue(e->getInputChannel());
		expanderChannels_[row]->setEnabled(e->getInputChannel().isValid() && e->canChangeInputChannel());
		expanderClockMode_[row]->setEnabled(e->getInputChannel().isValid());
		auto mc = std::dynamic_pointer_cast<midikraft::MidiClockCapability>(e);
		if (mc) {
			((ChoicePropertyComponent *)expanderClockMode_[row])->setIndex(static_cast<int>(mc->getMidiClockMode()));
		}
		for (int col = 0; col < keyboards_.size(); col++) {
			auto keyboardName = keyboards_[col]->getText().toStdString();
			auto keyboard = keyboardWithName(keyboardName);

			if (row == 0) {
				keyboardChannels_[col]->setValue(keyboard->getOutputChannel());
				keyboardChannels_[col]->setEnabled(keyboard->getOutputChannel().isValid());
				if (keyboard->hasLocalControl()) {
					keyboadLocalButtons_[col]->setToggleState(keyboard->getLocalControl(), dontSendNotification);
					keyboadLocalButtons_[col]->setEnabled(keyboard->getOutputChannel().isValid());
				}
			}

			bool isSet;
			if (keyboardName == expanderName) {
				// Now, if the names of expander and keyboard are equal, then they we need to respect the local off feature
				isSet = keyboard->getLocalControl();
			}
			else {
				// Else it we need to check that the device has external midi control on (or doesn't have that feature, then it is always on)
				isSet = e->getInputChannel().isValid() && (e->getInputChannel().toZeroBasedInt() == keyboard->getOutputChannel().toZeroBasedInt());
				isSet = isSet && (!e->hasMidiControl() || e->isMidiControlOn());
			}

			// Set the button
			buttonsForExpander_[expanderName][col]->setToggleState(isSet, dontSendNotification);
		}

		// Set the "MIDI Control" column
		if (e->hasMidiControl()) {
			auto button = buttonsForExpander_[expanderName][keyboards_.size()];
			button->setToggleState(e->isMidiControlOn(), dontSendNotification);
			button->setEnabled(e->getInputChannel().isValid());
		}
	}
}

std::shared_ptr<midikraft::SoundExpanderCapability> MasterkeyboardsView::expanderWithName(std::string const &name)
{
	for (auto synth : UIModel::instance()->synthList_.allSynths()) {
		if (synth.soundExpander()) {
			if (synth.soundExpander()->getName() == name) {
				return synth.soundExpander();
			}
		}
	}
	return nullptr;
}

std::shared_ptr <midikraft::MasterkeyboardCapability> MasterkeyboardsView::keyboardWithName(std::string const &name)
{
	for (auto synth : UIModel::instance()->synthList_.allSynths()) {
		auto keyboard = std::dynamic_pointer_cast<midikraft::MasterkeyboardCapability>(synth.device());
		if (keyboard) {
			if (synth.device()->getName() == name) {
				return keyboard;
			}
		}
	}
	return nullptr;
}

