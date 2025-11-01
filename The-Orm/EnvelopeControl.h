/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include <vector>

class EnvelopeControl : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    struct StageSpecification
    {
        enum class Role
        {
            Time,
            Level,
        };

        enum class TargetType
        {
            Hold,
            Absolute,
            Stage,
        };

        juce::String id;
        juce::String shortName;
        juce::String displayName;
        double defaultNormalisedValue = 0.0;
        Role role { Role::Time };
        TargetType targetType { TargetType::Hold };
        double absoluteLevel = 0.0;
        juce::String targetStageId;
    };

    struct Specification
    {
        juce::String id;
        juce::String displayName;
        std::vector<StageSpecification> stages;
    };

    EnvelopeControl();

    void setSpecification(const Specification& specification);
    const Specification& specification() const { return specification_; }
    int stageCount() const { return static_cast<int>(stages_.size()); }

    void setStageAssignment(int stageIndex, const juce::String& parameterName, double normalisedValue, bool assigned);
    void clearStage(int stageIndex);
    void setStageValue(int stageIndex, double normalisedValue);
    void setHoveredStage(int stageIndex);
    int stageAtLocalPoint(juce::Point<float> localPoint) const;

private:
    void paint(juce::Graphics& g) override;
    void updateTooltip();

    struct StageState
    {
        StageSpecification spec;
        juce::String parameterName;
        double normalisedValue = 0.0;
        bool assigned = false;
    };

    Specification specification_;
    std::vector<StageState> stages_;
    int hoveredStageIndex_ = -1;
};
