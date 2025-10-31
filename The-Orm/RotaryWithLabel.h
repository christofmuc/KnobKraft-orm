/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "TypedNamedValue.h"

class RotaryWithLabel : public Component, private Value::Listener {
public:
	RotaryWithLabel();

	virtual void resized() override;

	void setUnused();
	void setSynthParameter(TypedNamedValue *param);
	void setValue(int value);

private:
	void valueChanged(Value& value) override;
	double valueToSliderPosition(const juce::var& value) const;

	Slider slider;
	Label label;
	std::function<std::string(double)> valueToText_;
	juce::Value unboundValue_;
	TypedNamedValue* boundParam_ = nullptr;
};

class RotaryWithLabelAndButtonFunction : public RotaryWithLabel {
public:
	RotaryWithLabelAndButtonFunction();

	void resized() override;
	void setButtonSynthParameter(std::string const &text);

private:
	Label buttonLabel_;
};

class ButtonWithLabel : public Component {
public:
	ButtonWithLabel();

	void resized() override;

	TextButton button_;
	Label label_;
};

class ModernRotaryLookAndFeel : public juce::LookAndFeel_V4
{
public:
	ModernRotaryLookAndFeel();
	void drawRotarySlider(juce::Graphics& g,
		int x,
		int y,
		int width,
		int height,
		float sliderPosProportional,
		float rotaryStartAngle,
		float rotaryEndAngle,
		juce::Slider& slider) override;
	void drawLabel(juce::Graphics& g, juce::Label& label) override;
};
extern ModernRotaryLookAndFeel gModernRotaryLookAndFeel;
