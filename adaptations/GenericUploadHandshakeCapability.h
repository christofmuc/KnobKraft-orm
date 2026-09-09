/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "GenericAdaptation.h"
#include "UploadHandshakeCapability.h"

namespace knobkraft {

	class GenericUploadHandshakeCapability : public midikraft::UploadHandshakeCapability {
	public:
		explicit GenericUploadHandshakeCapability(GenericAdaptation* me) : me_(me) {}

		bool expectsUploadReply(const MidiMessage& sentMessage) const override;
		midikraft::UploadHandshakeReply isMessagePartOfUploadReply(
			const MidiMessage& message,
			const MidiMessage& sentMessage) const override;
		int uploadReplyTimeoutMs() const override;

	private:
		GenericAdaptation* me_;
	};

}
