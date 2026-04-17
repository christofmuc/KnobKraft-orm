/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PropertyEditor.h"
#include "Librarian.h"

class ExportDialog : public Component {
public:
	ExportDialog(std::shared_ptr<midikraft::Synth> synth);

	virtual void resized() override;

	midikraft::Librarian::ExportParameters getResult();

	static void showExportDialog(Component *centeredAround, std::string const& title, std::shared_ptr<midikraft::Synth> synth, std::function<void(midikraft::Librarian::ExportParameters)> callback);

	static void shutdown();

private:
	static std::unique_ptr<ExportDialog> sExportDialog_;
	static DialogWindow *sWindow_;

	PropertyEditor parameters_;
	TypedNamedValueSet props_;
	TextButton ok_;
	TextButton cancel_;

};

