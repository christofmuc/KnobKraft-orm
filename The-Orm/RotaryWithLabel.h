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

	Slider slider;
	Label label;
	std::function<std::string(double)> valueToText_;
};

class RotaryWithLabelAndButtonFunction : public RotaryWithLabel {
public:
	RotaryWithLabelAndButtonFunction();

	void resized() override;
	void setButtonSynthParameter(std::string const &text);

private:
	Label buttonLabel_;
};
