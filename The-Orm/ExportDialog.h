#pragma once

#include "JuceHeader.h"

#include "PropertyEditor.h"
#include "Librarian.h"

class ExportDialog : public Component {
public:
	ExportDialog();

	virtual void resized() override;

	midikraft::Librarian::ExportParameters getResult();

	static void showExportDialog(Component *centeredAround, std::function<void(midikraft::Librarian::ExportParameters)> callback);	

private:
	static std::unique_ptr<ExportDialog> sExportDialog_;
	static DialogWindow *sWindow_;

	PropertyEditor parameters_;
	TypedNamedValueSet props_;
	TextButton ok_;
	TextButton cancel_;

};


