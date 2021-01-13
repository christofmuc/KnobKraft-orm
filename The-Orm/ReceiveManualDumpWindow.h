/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "MidiLogView.h"


class ReceiveManualDumpWindow : public ThreadWithProgressWindow {
public:
	ReceiveManualDumpWindow(std::shared_ptr<midikraft::Synth> synth);

	virtual void run() override;

	std::vector<MidiMessage> result() const;

private:
	std::shared_ptr<midikraft::Synth> synth_;
	std::unique_ptr<MidiLogView> midiLog_;

	std::vector<MidiMessage> receivedMessages_;
};


