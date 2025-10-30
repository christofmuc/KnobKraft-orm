/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RotaryWithLabel.h"

#include "LayoutConstants.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include "SpdLogJuce.h"

RotaryWithLabel::RotaryWithLabel()
{
	addAndMakeVisible(slider, 1);
	slider.setSliderStyle(Slider::SliderStyle::RotaryHorizontalDrag);
	slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	addAndMakeVisible(label, 2);
	label.setJustificationType(Justification::centred);
	label.setInterceptsMouseClicks(false, false);
}

void RotaryWithLabel::resized()
{
	// They overlap
	slider.setBounds(getLocalBounds());
	label.setBounds(getLocalBounds());
}

void RotaryWithLabel::setUnused() {
	slider.setRange(0, 1, 0);
	valueToText_ = [](double) { return ""; };
	label.setText("", dontSendNotification);
}

void RotaryWithLabel::setSynthParameter(TypedNamedValue *param)
{
	boundParam_ = param;
	auto currentValue = param->value().getValue();
	double numericValue = valueToSliderPosition(currentValue);
	double minRange = static_cast<double>(param->minValue());
	double maxRange = static_cast<double>(param->maxValue());
	if (numericValue < minRange || numericValue > maxRange) {
		spdlog::warn("Value {} for {} out of range [{}, {}], clamping", numericValue, param->name().toStdString(), minRange, maxRange);
		numericValue = juce::jlimit(minRange, maxRange, numericValue);
	}
	spdlog::info("Assigning {} to rotary, resolved numeric value {}", param->name().toStdString(), numericValue);

	// Connect the slider to the value
	auto& valueObject = slider.getValueObject();
	valueObject.removeListener(this);
	valueObject.referTo(unboundValue_);

	slider.setRange(minRange, maxRange, 1.0);
	switch (param->valueType()) {
	case ValueType::List:
		// fall through
	case ValueType::Lookup:
		valueToText_ = [param](double value) -> std::string {
			int v = (int)trunc(value);
			auto lookup = param->lookup();
			if (lookup.find(v) != lookup.end()) {
				return fmt::format("{}:\n{}", param->name(), lookup[v]);
			}
			else {
				return fmt::format("{}:\n{}", param->name(), v);
			}
		};
		break;
	case ValueType::Bool:
	case ValueType::Integer:
		valueToText_ = [param](double value) {
			return fmt::format("{}:\n{}", param->name(),  (int)trunc(value));
		};
		break;
	case ValueType::String:
    case ValueType::Filename:
    case ValueType::Pathname:
	case ValueType::Color:
		jassertfalse; // A rotary dial for a string property doesn't make sense?
		break;

	}
	label.setText(param->name(), dontSendNotification);
	slider.setValue(numericValue, dontSendNotification);
	valueObject.referTo(param->value());
	valueObject.addListener(this);
	label.setText(valueToText_(numericValue), dontSendNotification);
}

void RotaryWithLabel::setValue(int value) {
	slider.setValue(value, dontSendNotification);
	label.setText(valueToText_(value), dontSendNotification);
}

void RotaryWithLabel::valueChanged(Value& value)
{
	auto numeric = valueToSliderPosition(value.getValue());
	spdlog::debug("Value change for {} -> {} ({})", boundParam_ ? boundParam_->name().toStdString() : "unbound", value.getValue().toString().toStdString(), numeric);
	label.setText(valueToText_(numeric), dontSendNotification);
}

double RotaryWithLabel::valueToSliderPosition(const juce::var& value) const
{
	if (value.isDouble()) {
		return static_cast<double>(value);
	}
	if (value.isInt() || value.isInt64()) {
		return static_cast<double>((int)value);
	}
	if (value.isBool()) {
		return static_cast<bool>(value) ? 1.0 : 0.0;
	}
	if (value.isString() && boundParam_) {
		int index = boundParam_->indexOfValue(value.toString().toStdString());
		if (index < boundParam_->minValue()) {
			return static_cast<double>(boundParam_->minValue());
		}
		return static_cast<double>(index);
	}
	return boundParam_ ? static_cast<double>(boundParam_->minValue()) : 0.0;
}

RotaryWithLabelAndButtonFunction::RotaryWithLabelAndButtonFunction()
{
	addAndMakeVisible(buttonLabel_, 2);
	buttonLabel_.setJustificationType(Justification::centred);
}

void RotaryWithLabelAndButtonFunction::resized()
{
	// Override the behavior of the super class, but take its implementation as a base
	RotaryWithLabel::resized();

	auto area = getLocalBounds();
	buttonLabel_.setBounds(area.removeFromBottom(30));
}

void RotaryWithLabelAndButtonFunction::setButtonSynthParameter(std::string const &text)
{
	buttonLabel_.setText(text, dontSendNotification);
}


ModernRotaryLookAndFeel::ModernRotaryLookAndFeel()
{
    setColour(juce::Slider::trackColourId, kAccentColour);
    setColour(juce::Slider::thumbColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.92f));
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

void ModernRotaryLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider) 
{
    auto sliderBounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    auto innerBounds = sliderBounds.reduced((float)LAYOUT_INSET_NORMAL);
    auto size = juce::jmin(innerBounds.getWidth(), innerBounds.getHeight());
    innerBounds = innerBounds.withSizeKeepingCentre(size, size);

    const auto centre = innerBounds.getCentre();
    const float radius = innerBounds.getWidth() * 0.5f;
    const float minTrackWidth = 4.0f;
    const float maxTrackWidth = juce::jmax(minTrackWidth, radius * 0.32f);
    const float desiredTrackWidth = radius * 0.22f;
    const float trackWidth = juce::jlimit(minTrackWidth, maxTrackWidth, desiredTrackWidth);
    const float arcRadius = radius - trackWidth * 0.5f;
    const float reducedRadius = juce::jmax(arcRadius - trackWidth * 0.65f, arcRadius * 0.35f);

    auto dialBounds = innerBounds.reduced(trackWidth * 0.5f);
    juce::ColourGradient dialGradient(kKnobFaceHighlight,
        centre.x,
        dialBounds.getY(),
        kKnobFaceShadow,
        centre.x,
        dialBounds.getBottom(),
        false);
    g.setGradientFill(dialGradient);
    g.fillEllipse(dialBounds);

    juce::Path baseArc;
    baseArc.addCentredArc(centre.x,
        centre.y,
        arcRadius,
        arcRadius,
        0.0f,
        rotaryStartAngle,
        rotaryEndAngle,
        true);
    g.setColour(kKnobTrackColour.withAlpha(slider.isEnabled() ? 0.9f : 0.3f));
    g.strokePath(baseArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x,
        centre.y,
        arcRadius,
        arcRadius,
        0.0f,
        rotaryStartAngle,
        angle,
        true);
    auto accent = slider.isEnabled() ? kAccentColour : kAccentColourInactive;
    g.setColour(accent);
    g.strokePath(valueArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float indicatorGap = juce::jmax(trackWidth * 0.5f, 5.0f);
    const float indicatorRadius = juce::jmax(trackWidth * 0.35f, 3.0f);
    const float indicatorDistance = reducedRadius - indicatorGap;
    const float indicatorAngle = angle - juce::MathConstants<float>::halfPi;
    auto indicatorDirection = juce::Point<float>(std::cos(indicatorAngle), std::sin(indicatorAngle));
    juce::Point<float> indicatorPosition(centre.x + indicatorDirection.x * indicatorDistance,
        centre.y + indicatorDirection.y * indicatorDistance);

    juce::Rectangle<float> indicatorBounds(indicatorRadius * 2.0f, indicatorRadius * 2.0f);
    indicatorBounds = indicatorBounds.withCentre(indicatorPosition);

    g.setColour(juce::Colours::black.withAlpha(slider.isEnabled() ? 0.3f : 0.2f));
    g.fillEllipse(indicatorBounds.translated(0.0f, indicatorRadius * 0.25f));

    g.setColour(juce::Colours::white.withAlpha(slider.isEnabled() ? 0.95f : 0.5f));
    g.fillEllipse(indicatorBounds);
}

void ModernRotaryLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) 
{
    if (label.isBeingEdited())
    {
        juce::LookAndFeel_V4::drawLabel(g, label);
        return;
    }

    auto area = label.getLocalBounds();
    auto background = label.findColour(juce::Label::backgroundColourId);
    if (!background.isTransparent())
    {
        g.setColour(background);
        g.fillRoundedRectangle(area.toFloat(), 4.0f);
    }

    auto text = label.getText();
    if (text.isEmpty())
        return;

    auto font = getLabelFont(label);
    g.setFont(font);

    auto textArea = area.reduced(2);
    int lineLimit = 1;
    int searchPosition = 0;
    while ((searchPosition = text.indexOfChar('\n', searchPosition)) >= 0)
    {
        ++lineLimit;
        ++searchPosition;
    }

    lineLimit = juce::jmax(1, lineLimit);

    auto shadowAlpha = label.isEnabled() ? 0.7f : 0.4f;
    g.setColour(juce::Colours::black.withAlpha(shadowAlpha));
    constexpr juce::Point<int> offsets[] = {
        { 0, 1 },  { 1, 0 },   { -1, 0 },  { 0, -1 },
        { 1, 1 },  { -1, 1 },  { 1, -1 },  { -1, -1 },
    };
    for (auto offset : offsets)
        g.drawFittedText(text, textArea.translated(offset.x, offset.y), label.getJustificationType(), lineLimit, 0.9f);
    constexpr juce::Point<int> softOffsets[] = {
        { 0, 2 }, { 2, 0 }, { -2, 0 }, { 0, -2 }
    };
    g.setColour(juce::Colours::black.withAlpha(shadowAlpha * 0.7f));
    for (auto offset : softOffsets)
        g.drawFittedText(text, textArea.translated(offset.x, offset.y), label.getJustificationType(), lineLimit, 0.9f);

    auto textColour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.6f);
    g.setColour(textColour);
    g.drawFittedText(text, textArea, label.getJustificationType(), lineLimit, 0.9f);
}

ModernRotaryLookAndFeel gModernRotaryLookAndFeel;
