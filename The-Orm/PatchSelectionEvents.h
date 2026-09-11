/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Capability.h"
#include "MidiController.h"
#include "PatchEventCapability.h"
#include "Synth.h"
#include "UploadSequence.h"

// Retain the selection's channel and bytes across an asynchronous upload, without
// keeping the synth (which owns the upload and its completion callback) alive.
class PatchSelectionEvents {
public:
	PatchSelectionEvents(std::shared_ptr<midikraft::Synth> const& synth, MidiChannel channel, std::vector<uint8> data)
		: synth_(synth), channel_(channel), data_(std::move(data)) {}

	void selected() const { dispatch(false); }
	void sent(const midikraft::UploadResult& result) const {
		if (result.successful()) dispatch(true);
	}

private:
	void dispatch(bool wasSent) const {
		if (auto synth = synth_.lock()) {
			if (auto events = midikraft::Capability::hasCapability<midikraft::PatchEventCapability>(synth)) {
				auto messages = wasSent ? events->onPatchSent(channel_, data_) : events->onPatchSelected(channel_, data_);
				if (!messages.empty()) midikraft::MidiController::instance()->sendToSecondaryMidiOut(messages);
			}
		}
	}

	std::weak_ptr<midikraft::Synth> synth_;
	MidiChannel channel_;
	std::vector<uint8> data_;
};
