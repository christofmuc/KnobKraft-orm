/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SynthParameterDefinition.h"

namespace midikraft {

	enum BCRtype { ENCODER, BUTTON };

	enum BCRledMode {
		OFF, ONE_DOT, ONE_DOT_OFF, ONETWO_DOT, ONETWO_DOT_OFF, BAR, BAR_OFF, SPREAD, PAN, QUAL, CUT, DAMP
	};

	// Hmm, the language is actually called BCL. But the device is the BCR.
	class BCRdefinition {
	public:
		BCRdefinition(BCRtype type, int encoderNumber);
		virtual ~BCRdefinition() = default;

		BCRtype type() const { return type_; }
		int encoderNumber() const { return number_; }

		static std::string ledMode(BCRledMode ledmode);

	protected:
		BCRtype type_;
		int number_;
	};

	class BCRStandardDefinition : public BCRdefinition {
	public:
		BCRStandardDefinition(BCRtype type, int encoderNumber) : BCRdefinition(type, encoderNumber) {
		}

		virtual std::string generateBCR(int channel) const = 0;
	};

	class BCRNamedParameterCapability {
	public:
		virtual std::string name() = 0;
	};

	class BCRGetParameterCapability {
	public:
		virtual std::shared_ptr<SynthParameterDefinition> parameter() = 0;
	};

}
