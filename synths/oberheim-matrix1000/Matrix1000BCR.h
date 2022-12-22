/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "BCRDefinition.h"
#include "BCR2000.h"

#include "Matrix1000ParamDefinition.h"

#include <map>

class  Matrix1000BCRDefinition : public BCRdefinition {
public:
	Matrix1000BCRDefinition(BCRtype type, int encoderNumber, int virtualCC = -1) : BCRdefinition(type, encoderNumber), virtualCC_(virtualCC), flip_(false) {};
	std::string generateBCR(Matrix1000ParamDefinition const *param) const;

	std::string ledMode() const;
	int maxValue(Matrix1000ParamDefinition const *param) const;
	int minValue(Matrix1000ParamDefinition const *param) const;
	int defaultValue(Matrix1000ParamDefinition const *param) const;

	// Smell, this indicates I should rather move code into the subclasses
	void setFlipMinMax(bool flip) { flip_ = flip; };
	void setLedMode(BCRledMode ledMode) { ledMode_ = ledMode;  }

private:
	int maxValueImpl(Matrix1000ParamDefinition const *param) const;
	int minValueImpl(Matrix1000ParamDefinition const *param) const;

	int virtualCC_;
	BCRledMode ledMode_;
	bool flip_;
};

class Matrix1000Button : public Matrix1000BCRDefinition {
public:
	Matrix1000Button(int buttonNumber, int virtualCC = -1);
};

class Matrix1000Encoder : public Matrix1000BCRDefinition {
public:
	Matrix1000Encoder(int encoderNumber, BCRledMode ledMode = BAR, bool flip = false);
};

class Matrix1000BCR {
public:
	Matrix1000BCR(int midiChannel, bool useSysex = true);

	std::string generateBCR(std::string const &preset1, std::string const &preset2, int baseStoragePlace, bool includeHeaderAndFooter);
	static bool parameterHasControlAssigned(Matrix1000Param const param);
	static std::vector<MidiMessage> createNRPNforParam(Matrix1000ParamDefinition *param, Patch const &patch, int zeroBasedChannel);

private:
	std::string generateMapping(std::string const &presetName, std::map<Matrix1000Param, BCRdefinition *> &controllerSetup, int storagePlace);

	int channel_;
	bool useSysex_;
};

