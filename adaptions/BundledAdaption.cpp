/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "BundledAdaption.h"

#include "CompiledAdaptions.h"

namespace knobkraft {

	std::vector<knobkraft::BundledAdaption> gBundledAdaptions()
	{
		return {
			{ "DSI Pro 2", "DSI_Pro_2", std::string(DSI_Pro_2_py, DSI_Pro_2_py + DSI_Pro_2_py_size) },
			{ "DSI Prophet 08", "DSI_Prophet_08", std::string(DSI_Prophet_08_py, DSI_Prophet_08_py + DSI_Prophet_08_py_size) },
			{ "DSI Prophet 12", "DSI_Prophet_12", std::string(DSI_Prophet_12_py, DSI_Prophet_12_py + DSI_Prophet_12_py_size) },
			{ "Matrix 6", "Matrix_6", std::string(Matrix_6_py, Matrix_6_py + Matrix_6_py_size) },
			{ "Matrix 1000 Adaption", "Matrix_1000", std::string(Matrix1000_py, Matrix1000_py + Matrix1000_py_size) },
			{ "Pioneer Toraiz AS1", "Pioneer_Toraiz_AS1", std::string(PioneerToraiz_AS1_py, PioneerToraiz_AS1_py + PioneerToraiz_AS1_py_size) },
			{ "Roland JX-8P", "Roland_JX_8P", std::string(Roland_JX_8P_py, Roland_JX_8P_py + Roland_JX_8P_py_size) },
			{ "Sequential Pro 3", "Sequential_Pro_3", std::string(Sequential_Pro_3_py, Sequential_Pro_3_py + Sequential_Pro_3_py_size) },
			{ "Sequential Prophet 6", "Sequential_Prophet_6", std::string(Sequential_Prophet_6_py, Sequential_Prophet_6_py + Sequential_Prophet_6_py_size) },
		};
	}

}
