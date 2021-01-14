/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BundledAdaptation.h"

#include "CompiledAdaptations.h"
#include "GenericAdaptation.h"

namespace knobkraft {

	std::vector<knobkraft::BundledAdaptation> BundledAdaptations::getAll()
	{
		return {
			{ "Alesis Andromeda A6", "Alesis_Andromeda_A6", std::string(AlesisAndromedaA6_py, AlesisAndromedaA6_py + AlesisAndromedaA6_py_size) },
			{ "BC Kijimi", "BC_Kijimi", std::string(BC_Kijimi_py, BC_Kijimi_py + BC_Kijimi_py_size) },
			{ "Deepmind 12", "Deepmind_12", std::string(Behringer_Deepmind_12_py, Behringer_Deepmind_12_py + Behringer_Deepmind_12_py_size) },
			{ "DSI Pro 2", "DSI_Pro_2", std::string(DSI_Pro_2_py, DSI_Pro_2_py + DSI_Pro_2_py_size) },
			{ "DSI Prophet 08", "DSI_Prophet_08", std::string(DSI_Prophet_08_py, DSI_Prophet_08_py + DSI_Prophet_08_py_size) },
			{ "DSI Prophet 12", "DSI_Prophet_12", std::string(DSI_Prophet_12_py, DSI_Prophet_12_py + DSI_Prophet_12_py_size) },
			{ "Electra One", "Electra_one", std::string(ElectraOne_py, ElectraOne_py + ElectraOne_py_size) },
			{ "Kawai K1", "Kawai_K1", std::string(KawaiK1_py, KawaiK1_py + KawaiK1_py_size) },
			{ "Korg DW-6000", "Korg_DW_6000", std::string(KorgDW6000_py, KorgDW6000_py + KorgDW6000_py_size) },
			{ "Korg MS2000", "Korg_MS2000", std::string(KorgMS2000_py, KorgMS2000_py + KorgMS2000_py_size) },
			{ "Matrix 6", "Matrix_6", std::string(Matrix_6_py, Matrix_6_py + Matrix_6_py_size) },
			{ "Oberheim OB-8", "Oberheim_OB8", std::string(OberheimOB8_py, OberheimOB8_py + OberheimOB8_py_size) },
			{ "Oberheim OB-X", "Oberheim_OB_X", std::string(OberheimOBX_py, OberheimOBX_py + OberheimOBX_py_size) },
			{ "Oberheim OB-Xa", "Oberheim_OB_Xa", std::string(OberheimOBXa_py, OberheimOBXa_py + OberheimOBXa_py_size) },
#ifdef _DEBUG
			{ "Matrix 1000 Test", "Matrix_1000", std::string(Matrix1000_py, Matrix1000_py + Matrix1000_py_size) },
			{ "Korg DW-8000 Test", "Korg_DW_8000_Adaption", std::string(KorgDW8000_py, KorgDW8000_py + KorgDW8000_py_size) },
			{ "Kawai K3 Test", "Kawai_K3", std::string(KawaiK3_py, KawaiK3_py + KawaiK3_py_size) },
#endif
			{ "Pioneer Toraiz AS1", "Pioneer_Toraiz_AS1", std::string(PioneerToraiz_AS1_py, PioneerToraiz_AS1_py + PioneerToraiz_AS1_py_size) },
			{ "Quasimidi Cyber-6", "Quasimidi_Cyber_6", std::string(QuasimidiCyber6_py, QuasimidiCyber6_py + QuasimidiCyber6_py_size) },
			{ "Roland JX-8P", "Roland_JX_8P", std::string(Roland_JX_8P_py, Roland_JX_8P_py + Roland_JX_8P_py_size) },
			{ "Sequential Pro 3", "Sequential_Pro_3", std::string(Sequential_Pro_3_py, Sequential_Pro_3_py + Sequential_Pro_3_py_size) },
			{ "Sequential Prophet 5 Rev4", "Sequential_Prophet_5_Rev4", std::string(Sequential_Prophet_5_Rev4_py, Sequential_Prophet_5_Rev4_py + Sequential_Prophet_5_Rev4_py_size) },
			{ "Sequential Prophet 6", "Sequential_Prophet_6", std::string(Sequential_Prophet_6_py, Sequential_Prophet_6_py + Sequential_Prophet_6_py_size) },
			{ "Sequential Prophet X", "Sequential_Prophet_X", std::string(Sequential_Prophet_X_py, Sequential_Prophet_X_py + Sequential_Prophet_X_py_size) },
			{ "Waldorf Blofeld", "Waldorf_Blofeld", std::string(Waldorf_Blofeld_py, Waldorf_Blofeld_py + Waldorf_Blofeld_py_size) },
		};
	}

	bool BundledAdaptations::breakOut(std::string synthName)
	{
		// Find it
		BundledAdaptation adaptation;
		for (auto a : getAll()) {
			if (a.synthName == synthName) {
				adaptation = a;
				break;
			}
		}
		if (adaptation.pythonModuleName.empty()) {
			SimpleLogger::instance()->postMessage("Program error - could not find adaptation for synth " + synthName);
			return false;
		}

		auto dir = GenericAdaptation::getAdaptationDirectory();

		// Copy out source code
		File target = dir.getChildFile(adaptation.pythonModuleName + ".py");
		if (target.exists()) {
			juce::AlertWindow::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "File exists", "There is already a file for this adaptation, which we will not overwrite.");
			return false;
		}

		FileOutputStream out(target);
#if WIN32
		out.writeText(adaptation.adaptationSourceCode, false, false, "\r\n");
#else
		out.writeText(adaptation.adaptationSourceCode, false, false, "\n");
#endif
		return true;
	}

}
