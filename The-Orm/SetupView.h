/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

//#include "HueLightControl.h"
#include "PropertyEditor.h"
#include "InfoText.h"
#include "LambdaButtonStrip.h"
#include "DebounceTimer.h"

#include "AutoDetection.h"

class SetupView : public Component,
	private ChangeListener, private Value::Listener
{
public:
	SetupView(midikraft::AutoDetection *autoDetection /*, HueLightControl *lights*/);
	virtual ~SetupView();

	virtual void resized() override;
	
	void refreshData();

private:
	virtual void valueChanged(Value& value) override;
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;
	void setValueWithoutListeners(Value &value, int newValue);

	void quickConfigure();

	std::vector<std::shared_ptr<TypedNamedValue>> properties_;
	midikraft::AutoDetection *autoDetection_;
	InfoText header_;
	//HueLightControl * lights_;
	LambdaButtonStrip functionButtons_;
	PropertyEditor propertyEditor_;

	DebounceTimer timedAction_;
};

