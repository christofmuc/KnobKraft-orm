/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "EnvelopeControl.h"

#include "LayoutConstants.h"

#include <algorithm>
#include <cmath>

namespace
{
inline double clamp01(double value)
{
    return juce::jlimit(0.0, 1.0, value);
}

using StageSpec = EnvelopeControl::StageSpecification;
using StageRole = StageSpec::Role;
using StageTarget = StageSpec::TargetType;

StageSpec makeTimeStage(const char* id,
                        const char* shortName,
                        const char* displayName,
                        double defaultWeight,
                        StageTarget targetType,
                        double absoluteLevel = 0.0,
                        const char* targetStageId = "")
{
    StageSpec spec;
    spec.id = id;
    spec.shortName = shortName;
    spec.displayName = displayName;
    spec.defaultNormalisedValue = defaultWeight;
    spec.role = StageRole::Time;
    spec.targetType = targetType;
    spec.absoluteLevel = absoluteLevel;
    spec.targetStageId = targetStageId;
    return spec;
}

StageSpec makeLevelStage(const char* id, const char* shortName, const char* displayName, double defaultLevel)
{
    StageSpec spec;
    spec.id = id;
    spec.shortName = shortName;
    spec.displayName = displayName;
    spec.defaultNormalisedValue = defaultLevel;
    spec.role = StageRole::Level;
    spec.targetType = StageTarget::Hold;
    spec.absoluteLevel = defaultLevel;
    return spec;
}

EnvelopeControl::Specification makeDefaultSpecification()
{
    EnvelopeControl::Specification spec;
    spec.id = "default";
    spec.displayName = "Envelope";
    spec.stages = {
        makeTimeStage("attack", "A", "Attack", 0.35, StageTarget::Absolute, 1.0),
        makeTimeStage("decay", "D", "Decay", 0.45, StageTarget::Stage, 0.0, "sustain"),
        makeLevelStage("sustain", "S", "Sustain", 0.60),
        makeTimeStage("release", "R", "Release", 0.40, StageTarget::Absolute, 0.0),
    };
    return spec;
}

float valueToY(float value, juce::Rectangle<float> const& area)
{
    auto clamped = static_cast<float>(clamp01(value));
    return juce::jmap(clamped, 0.0f, 1.0f, area.getBottom(), area.getY() + area.getHeight() * 0.04f);
}
}

EnvelopeControl::EnvelopeControl()
{
    setSpecification(makeDefaultSpecification());
}

void EnvelopeControl::setSpecification(const Specification& specification)
{
    specification_ = specification;
    stages_.clear();
    stages_.reserve(specification_.stages.size());
    for (auto const& stageSpec : specification_.stages)
    {
        StageState state;
        state.spec = stageSpec;
        state.normalisedValue = clamp01(stageSpec.defaultNormalisedValue);
        state.assigned = false;
        stages_.push_back(std::move(state));
    }
    hoveredStageIndex_ = -1;
    updateTooltip();
    repaint();
}

void EnvelopeControl::setStageAssignment(int stageIndex,
                                         const juce::String& parameterName,
                                         double normalisedValue,
                                         bool assigned)
{
    if (stageIndex < 0 || stageIndex >= stageCount())
        return;

    auto& stage = stages_[(size_t)stageIndex];
    stage.parameterName = parameterName;
    stage.normalisedValue = clamp01(normalisedValue);
    stage.assigned = assigned;
    updateTooltip();
    repaint();
}

void EnvelopeControl::clearStage(int stageIndex)
{
    if (stageIndex < 0 || stageIndex >= stageCount())
        return;

    auto& stage = stages_[(size_t)stageIndex];
    stage.parameterName.clear();
    stage.normalisedValue = clamp01(stage.spec.defaultNormalisedValue);
    stage.assigned = false;
    updateTooltip();
    repaint();
}

void EnvelopeControl::setStageValue(int stageIndex, double normalisedValue)
{
    if (stageIndex < 0 || stageIndex >= stageCount())
        return;

    stages_[(size_t)stageIndex].normalisedValue = clamp01(normalisedValue);
    updateTooltip();
    repaint();
}

void EnvelopeControl::setHoveredStage(int stageIndex)
{
    if (hoveredStageIndex_ == stageIndex)
        return;

    hoveredStageIndex_ = stageIndex;
    repaint();
}

int EnvelopeControl::stageAtLocalPoint(juce::Point<float> localPoint) const
{
    auto bounds = getLocalBounds().toFloat();
    auto count = stageCount();
    if (bounds.getWidth() <= 0.0f || count <= 0)
        return -1;

    auto stageWidth = bounds.getWidth() / (float)count;
    auto relativeX = localPoint.x - bounds.getX();
    if (relativeX < 0.0f)
        return -1;

    int index = (int)std::floor(relativeX / stageWidth);
    return juce::jlimit(0, juce::jmax(0, count - 1), index);
}

void EnvelopeControl::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto count = stageCount();
    if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f || count == 0)
        return;

    const float outlineRadius = juce::jmin(kCornerRadius, juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f);
    juce::Path backgroundPath;
    backgroundPath.addRoundedRectangle(bounds.reduced(0.5f), outlineRadius);

    juce::ColourGradient backgroundGradient(kPaletteFillHover.darker(0.15f),
                                            bounds.getTopLeft(),
                                            kPaletteFill,
                                            bounds.getBottomRight(),
                                            false);
    g.setGradientFill(backgroundGradient);
    g.fillPath(backgroundPath);

    g.setColour(kPaletteOutline.withAlpha(0.85f));
    g.strokePath(backgroundPath, juce::PathStrokeType(1.2f));

    auto contentBounds = bounds.reduced(6.0f);
    const float labelHeight = juce::jlimit(18.0f, 28.0f, contentBounds.getHeight() * 0.22f);
    auto graphArea = contentBounds.withTrimmedBottom(labelHeight + 6.0f);
    juce::Rectangle<float> labelArea(contentBounds.getX(),
                                     graphArea.getBottom() + 4.0f,
                                     contentBounds.getWidth(),
                                     labelHeight);

    const float stageWidthVisual = graphArea.getWidth() / (float)count;

    for (int stage = 0; stage < count; ++stage)
    {
        auto stageRect = juce::Rectangle<float>(graphArea.getX() + stageWidthVisual * (float)stage,
                                                graphArea.getY(),
                                                stageWidthVisual,
                                                graphArea.getHeight());

        if (stage == hoveredStageIndex_)
        {
            g.setColour(kAccentColour.withAlpha(0.22f));
            g.fillRoundedRectangle(stageRect, 6.0f);
        }
        else if (stages_[(size_t)stage].assigned)
        {
            g.setColour(kAccentColour.withAlpha(0.10f));
            g.fillRoundedRectangle(stageRect, 6.0f);
        }

        if (stage > 0)
        {
            g.setColour(kPaletteOutline.withAlpha(0.25f));
            g.drawLine(stageRect.getX(), stageRect.getY() + 2.0f, stageRect.getX(), stageRect.getBottom() - 2.0f);
        }
    }

    const auto toY = [&](float level) { return valueToY(level, graphArea); };
    const auto resolveStageLevel = [this](const juce::String& stageId, float fallback) -> float {
        if (stageId.isEmpty())
            return fallback;
        for (auto const& state : stages_)
            if (state.spec.id.equalsIgnoreCase(stageId))
                return juce::jlimit(0.0f, 1.0f, (float)state.normalisedValue);
        return fallback;
    };

    const float minWeight = 0.02f;
    std::vector<float> weights(stages_.size(), 0.0f);
    float weightSum = 0.0f;
    for (size_t i = 0; i < stages_.size(); ++i)
    {
        if (stages_[i].spec.role == StageRole::Time)
        {
            float weight = minWeight + static_cast<float>(stages_[i].normalisedValue) * (1.0f - minWeight);
            weights[i] = weight;
            weightSum += weight;
        }
    }
    if (weightSum <= 0.0f)
        weightSum = 1.0f;

    juce::Path envelopePath;
    juce::Path filledPath;

    float currentX = graphArea.getX();
    float currentLevel = 0.0f;
    auto startY = toY(currentLevel);
    envelopePath.startNewSubPath(currentX, startY);
    filledPath.startNewSubPath(graphArea.getX(), graphArea.getBottom());
    filledPath.lineTo(currentX, startY);

    for (size_t i = 0; i < stages_.size(); ++i)
    {
        const auto& state = stages_[i];
        const auto& spec = state.spec;
        if (spec.role == StageRole::Time)
        {
            float proportion = weights[i] / weightSum;
            currentX += proportion * graphArea.getWidth();

            float targetLevel = currentLevel;
            switch (spec.targetType)
            {
            case StageTarget::Hold:
                break;
            case StageTarget::Absolute:
                targetLevel = juce::jlimit(0.0f, 1.0f, (float)spec.absoluteLevel);
                break;
            case StageTarget::Stage:
                targetLevel = resolveStageLevel(spec.targetStageId, currentLevel);
                break;
            }

            auto nextY = toY(targetLevel);
            envelopePath.lineTo(currentX, nextY);
            filledPath.lineTo(currentX, nextY);
            currentLevel = targetLevel;
        }
        else if (spec.role == StageRole::Level)
        {
            // Keep level information for time segments that reference this stage.
        }
    }

    const float endX = graphArea.getRight();
    if (currentX < endX - 0.5f)
    {
        auto endY = toY(currentLevel);
        envelopePath.lineTo(endX, endY);
        filledPath.lineTo(endX, endY);
    }

    filledPath.lineTo(endX, graphArea.getBottom());
    filledPath.closeSubPath();

    juce::Colour envelopeColour = kAccentColour.brighter(0.12f);
    g.setColour(envelopeColour.withAlpha(0.18f));
    g.fillPath(filledPath);

    g.setColour(envelopeColour.withAlpha(0.92f));
    g.strokePath(envelopePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int stage = 0; stage < count; ++stage)
    {
        auto stageLabelBounds = juce::Rectangle<float>(labelArea.getX() + stageWidthVisual * (float)stage,
                                                       labelArea.getY(),
                                                       stageWidthVisual,
                                                       labelArea.getHeight());

        auto& state = stages_[(size_t)stage];

        g.setColour(juce::Colours::white.withAlpha(state.assigned ? 0.95f : 0.65f));
        g.setFont(juce::Font(juce::FontOptions(labelArea.getHeight() * 0.52f, juce::Font::bold)));
        g.drawFittedText(state.spec.shortName,
                         stageLabelBounds.toNearestInt().removeFromTop(juce::roundToInt(labelArea.getHeight() * 0.55f)),
                         juce::Justification::centred,
                         1);

        juce::String labelText = state.spec.displayName;
        if (state.assigned && !state.parameterName.isEmpty())
            labelText = state.parameterName;
        else
            labelText += state.spec.role == StageRole::Time ? " (time)" : " (level)";

        g.setFont(juce::Font(juce::FontOptions(labelArea.getHeight() * 0.38f, juce::Font::plain)));
        g.drawFittedText(labelText,
                         stageLabelBounds.toNearestInt(),
                         juce::Justification::centred,
                         2);
    }
}

void EnvelopeControl::updateTooltip()
{
    juce::StringArray lines;
    for (auto const& state : stages_)
    {
        juce::String line = state.spec.displayName + (state.spec.role == StageRole::Time ? " (time)" : " (level)");
        line += ": ";
        if (state.assigned && !state.parameterName.isEmpty())
            line += state.parameterName;
        else
            line += "unassigned";
        lines.add(line);
    }
    setTooltip(lines.joinIntoString("\n"));
}
