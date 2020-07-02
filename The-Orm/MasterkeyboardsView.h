#pragma once

#include "JuceHeader.h"

#include "MasterkeyboardCapability.h"
#include "SoundExpanderCapability.h"
#include "SynthHolder.h"
#include "MidiChannelEntry.h"

#include <map>

class MidiController;

class MasterkeyboardsView : public Component,
	public ChangeListener
{
public:
	MasterkeyboardsView();
	~MasterkeyboardsView();

	virtual void resized() override;
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

private:
	void recreate();
	void refreshCheckmarks();
	std::shared_ptr<midikraft::SoundExpanderCapability> expanderWithName(std::string const &name);
	std::shared_ptr<midikraft::MasterkeyboardCapability> keyboardWithName(std::string const &name);

	Label header;
	OwnedArray<Label> keyboards_;
	OwnedArray<MidiChannelEntry> keyboardChannels_;
	OwnedArray<ToggleButton> keyboadLocalButtons_;
	OwnedArray<Label> expanders_;
	OwnedArray<MidiChannelEntry> expanderChannels_;
	OwnedArray<Button::Listener> listeners_;
	std::map<std::string, std::vector<ToggleButton *>> buttonsForExpander_;
};
