#pragma once

#include "BCRDefinition.h"
#include "MKS80_Parameter.h"

#include <vector>

class MKS80_BCR2000_Encoder : public BCRStandardDefinitionWithName {
public:
	MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, MKS80_Parameter::PatchParameter patchParameter, BCRtype type, int encoderNumber, BCRledMode ledMode = ONE_DOT);
	
	MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, MKS80_Parameter::ToneParameter toneParameter, BCRtype type, int encoderNumber, BCRledMode ledMode = ONE_DOT);
	
	MKS80_BCR2000_Encoder(MKS80_Parameter::ParameterType paramType, MKS80_Parameter::SynthSection section, int parameterIndex, BCRtype type, int encoderNumber, BCRledMode ledMode);

	std::shared_ptr<MKS80_Parameter> parameterDef() const;

	std::string generateBCR(int channel) const override;

private:
	std::shared_ptr<MKS80_Parameter> param_;
	MKS80_Parameter::SynthSection section_;
	BCRledMode ledMode_;
};


class MKS80_BCR2000 {
public:
	static std::string generateBCL(std::string const &presetName, int channel, int storePreset = -1, int recallPreset = -1);
	static std::vector<BCRStandardDefinition *> BCR2000_setup(MKS80_Parameter::SynthSection section);
};


