/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include <array>

class ADSRControl : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    static constexpr int kStageCount = 4;

    ADSRControl();

    void setStageAssignment(int stageIndex, const juce::String& parameterName, double normalisedValue, bool assigned);
    void clearStage(int stageIndex);
    void setStageValue(int stageIndex, double normalisedValue);
    void setHoveredStage(int stageIndex);
    int stageAtLocalPoint(juce::Point<float> localPoint) const;
private:
    void paint(juce::Graphics& g) override;
    void updateTooltip();

    struct StageInfo
    {
        juce::String shortName;
        juce::String parameterName;
        double normalisedValue = 0.0;
        bool assigned = false;
    };

    std::array<StageInfo, kStageCount> stages_;
    int hoveredStageIndex_ = -1;
};
