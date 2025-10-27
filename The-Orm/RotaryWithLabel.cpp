/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RotaryWithLabel.h"

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
