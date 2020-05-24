/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "RotaryWithLabel.h"

#include <boost/format.hpp>

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
	// Connect the slider to the value
	slider.getValueObject().referTo(param->value());
	slider.getValueObject().addListener(this);

	slider.setRange(param->minValue(), param->maxValue(), 1.0);
	switch (param->valueType()) {
	case ValueType::Lookup:
		valueToText_ = [param](double value) -> std::string {
			int v = (int)trunc(value);
			auto lookup = param->lookup();
			if (lookup.find(v) != lookup.end()) {
				return (boost::format("%s:\n%s") % param->name() % lookup[v]).str();
			}
			else {
				return "invalid";
			}
		};
		break;
	case ValueType::Bool:
	case ValueType::Integer:
		valueToText_ = [param](double value) {
			return (boost::format("%s:\n%d") % param->name() % (int)trunc(value)).str();
		};
		break;
	case ValueType::String:
		jassertfalse; // A rotary dial for a string property doesn't make sense?
		break;
	}
	label.setText(param->name(), dontSendNotification);
}

void RotaryWithLabel::setValue(int value) {
	slider.setValue(value, dontSendNotification);
	label.setText(valueToText_(value), dontSendNotification);
}

void RotaryWithLabel::valueChanged(Value& value)
{
	label.setText(valueToText_(value.getValue()), dontSendNotification);
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
