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

class DropdownWithLabel : public juce::Component
{
public:
    DropdownWithLabel();

    void resized() override;

    void setUnused();
    void configureForLookup(const juce::String& labelText,
                            const std::map<int, std::string>& lookup,
                            std::function<void(int)> onChange);
    void setSelectedLookupValue(int value);
    void setTooltip(const juce::String& tooltip);

private:
    enum class Mode
    {
        None,
        Lookup,
    };

    void handleSelectionChanged();

    juce::Label label_;
    juce::ComboBox combo_;
    Mode mode_ { Mode::None };
    juce::Array<int> lookupKeys_;
    std::function<void(int)> lookupCallback_;
    bool ignoreCallbacks_ { false };
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
