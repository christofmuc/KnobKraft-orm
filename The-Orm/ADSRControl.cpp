/*
   Copyright (c) 2020-2025 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ADSRControl.h"

#include "LayoutConstants.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::array<const char*, ADSRControl::kStageCount> kStageShortNames = { "A", "D", "S", "R" };
constexpr std::array<const char*, ADSRControl::kStageCount> kStageFullNames = { "Attack", "Decay", "Sustain", "Release" };
constexpr std::array<double, ADSRControl::kStageCount> kDefaultStageValues = { 0.35, 0.45, 0.6, 0.4 };

inline double clamp01(double value)
{
    return juce::jlimit(0.0, 1.0, value);
}
} // namespace

ADSRControl::ADSRControl()
{
    for (int stage = 0; stage < kStageCount; ++stage)
    {
        stages_[stage].shortName = kStageShortNames[(size_t)stage];
        stages_[stage].normalisedValue = kDefaultStageValues[(size_t)stage];
        stages_[stage].assigned = false;
    }
    updateTooltip();
}

void ADSRControl::setStageAssignment(int stageIndex, const juce::String& parameterName, double normalisedValue, bool assigned)
{
    if (stageIndex < 0 || stageIndex >= kStageCount)
        return;

    stages_[(size_t)stageIndex].parameterName = parameterName;
    stages_[(size_t)stageIndex].normalisedValue = clamp01(normalisedValue);
    stages_[(size_t)stageIndex].assigned = assigned;
    updateTooltip();
    repaint();
}

void ADSRControl::clearStage(int stageIndex)
{
    if (stageIndex < 0 || stageIndex >= kStageCount)
        return;

    stages_[(size_t)stageIndex].parameterName.clear();
    stages_[(size_t)stageIndex].normalisedValue = kDefaultStageValues[(size_t)stageIndex];
    stages_[(size_t)stageIndex].assigned = false;
    updateTooltip();
    repaint();
}

void ADSRControl::setStageValue(int stageIndex, double normalisedValue)
{
    if (stageIndex < 0 || stageIndex >= kStageCount)
        return;

    stages_[(size_t)stageIndex].normalisedValue = clamp01(normalisedValue);
    repaint();
}

void ADSRControl::setHoveredStage(int stageIndex)
{
    if (hoveredStageIndex_ == stageIndex)
        return;

    hoveredStageIndex_ = stageIndex;
    repaint();
}

int ADSRControl::stageAtLocalPoint(juce::Point<float> localPoint) const
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0.0f)
        return -1;

    auto stageWidth = bounds.getWidth() / (float)kStageCount;
    auto relativeX = localPoint.x - bounds.getX();
    if (relativeX < 0.0f)
        return -1;

    int index = (int)std::floor(relativeX / stageWidth);
    return juce::jlimit(0, kStageCount - 1, index);
}

void ADSRControl::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
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

    const float stageWidth = graphArea.getWidth() / (float)kStageCount;

    for (int stage = 0; stage < kStageCount; ++stage)
    {
        auto stageRect = juce::Rectangle<float>(graphArea.getX() + stageWidth * (float)stage,
                                                graphArea.getY(),
                                                stageWidth,
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

    const double attackValue = stages_[0].assigned ? stages_[0].normalisedValue : kDefaultStageValues[0];
    const double decayValue = stages_[1].assigned ? stages_[1].normalisedValue : kDefaultStageValues[1];
    const double sustainValue = stages_[2].assigned ? stages_[2].normalisedValue : kDefaultStageValues[2];
    const double releaseValue = stages_[3].assigned ? stages_[3].normalisedValue : kDefaultStageValues[3];

    const double attackDuration = juce::jmap(attackValue, 0.0, 1.0, 0.12, 1.1);
    const double decayDuration = juce::jmap(decayValue, 0.0, 1.0, 0.12, 1.0);
    const double sustainDuration = juce::jmap(sustainValue, 0.0, 1.0, 0.35, 1.0);
    const double releaseDuration = juce::jmap(releaseValue, 0.0, 1.0, 0.12, 1.0);

    const double totalDuration = attackDuration + decayDuration + sustainDuration + releaseDuration;
    if (totalDuration > 0.0)
    {
        const double scale = graphArea.getWidth() / totalDuration;
        const float startX = graphArea.getX();
        const float startY = graphArea.getBottom();
        const float topY = graphArea.getY();
        const float sustainY = juce::jmap((float)clamp01(sustainValue), 0.0f, 1.0f, startY, topY + graphArea.getHeight() * 0.05f);

        const float attackX = startX + (float)(attackDuration * scale);
        const float decayX = attackX + (float)(decayDuration * scale);
        const float sustainEndX = decayX + (float)(sustainDuration * scale);
        const float releaseX = sustainEndX + (float)(releaseDuration * scale);

        juce::Path envelope;
        envelope.startNewSubPath(startX, startY);
        envelope.quadraticTo(startX + (attackX - startX) * 0.45f,
                             topY - graphArea.getHeight() * 0.12f,
                             attackX,
                             topY);
        envelope.quadraticTo(attackX + (decayX - attackX) * 0.45f,
                             sustainY - graphArea.getHeight() * 0.10f,
                             decayX,
                             sustainY);
        envelope.lineTo(sustainEndX, sustainY);
        envelope.quadraticTo(sustainEndX + (releaseX - sustainEndX) * 0.45f,
                             sustainY + graphArea.getHeight() * 0.10f,
                             releaseX,
                             startY);

        juce::Path filledEnvelope = envelope;
        filledEnvelope.lineTo(releaseX, startY);
        filledEnvelope.closeSubPath();

        juce::Colour envelopeColour = kAccentColour.brighter(0.12f);
        g.setColour(envelopeColour.withAlpha(0.18f));
        g.fillPath(filledEnvelope);

        g.setColour(envelopeColour.withAlpha(0.92f));
        g.strokePath(envelope, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    for (int stage = 0; stage < kStageCount; ++stage)
    {
        auto stageLabelBounds = juce::Rectangle<float>(labelArea.getX() + stageWidth * (float)stage,
                                                       labelArea.getY(),
                                                       stageWidth,
                                                       labelArea.getHeight());

        auto& info = stages_[(size_t)stage];

        g.setColour(juce::Colours::white.withAlpha(info.assigned ? 0.95f : 0.65f));
        g.setFont(juce::Font(juce::FontOptions(labelArea.getHeight() * 0.52f, juce::Font::bold)));
        g.drawFittedText(info.shortName,
                         stageLabelBounds.toNearestInt().removeFromTop(juce::roundToInt(labelArea.getHeight() * 0.55f)),
                         juce::Justification::centred,
                         1);

        g.setFont(juce::Font(juce::FontOptions(labelArea.getHeight() * 0.38f, juce::Font::plain)));
        auto text = info.assigned ? info.parameterName
                                  : juce::String(kStageFullNames[(size_t)stage]);
        g.drawFittedText(text,
                         stageLabelBounds.toNearestInt(),
                         juce::Justification::centred,
                         2);
    }
}

void ADSRControl::updateTooltip()
{
    juce::StringArray lines;
    for (int stage = 0; stage < kStageCount; ++stage)
    {
        auto& info = stages_[(size_t)stage];
        auto label = juce::String(kStageFullNames[(size_t)stage]) + ": ";
        if (info.assigned && !info.parameterName.isEmpty())
            label += info.parameterName;
        else
            label += "unassigned";
        lines.add(label);
    }
    setTooltip(lines.joinIntoString("\n"));
}
