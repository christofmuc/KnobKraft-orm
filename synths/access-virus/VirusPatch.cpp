/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "VirusPatch.h"

#include <boost/format.hpp>
#include <locale>
#include <codecvt>

namespace midikraft {

	// C++ 11 has u32string, which allows me to access any character with the character index from the Virus' sysex data. The arrows are not rendered by JUCE, however
	std::u32string virus_codepage = U" !\"  #&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[¥]^_`abcdefghijklmnopqrstuvwxyz{|}→←";
	//                                       &' )   -./0         :    ?@A                          ¥                                }
	// Second line lists verified characters

	// Now, this gets more ugly the longer I look at it. The whole uconvert function down there is also deprecated from C++ 17, so I am sure I will come back
#ifdef _MSC_VER
	typedef int32_t CHAR32_T_FIX;
#else
	typedef char32_t CHAR32_T_FIX;
#endif

	// Constructing the converter is really slow, so don't do this in a loop!
	std::wstring_convert<std::codecvt_utf8<CHAR32_T_FIX>, CHAR32_T_FIX> convert;

	// Sigh
	// https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
	std::string utf32_to_utf8(char32_t utf32_char)
	{
		std::u32string utf32_string;
		utf32_string.push_back(utf32_char);
		auto p = reinterpret_cast<const CHAR32_T_FIX *>(utf32_string.data());
		return convert.to_bytes(p, p + utf32_string.size());
	}


	VirusPatch::VirusPatch(Synth::PatchData const &data) : Patch(PATCH_VIRUS_B, data)
	{
		jassert(data.size() == 256);
	}

	std::string VirusPatch::name() const
	{
		std::string result;
		for (int i = index(PageB, 112); i < index(PageB, 122); i++) {
			// Character lookup
			uint8 charIndex = data()[i];
			if (charIndex >= 32 && charIndex <= 32 + virus_codepage.length()) {
				result = result + utf32_to_utf8(virus_codepage[charIndex - 32]); // Could be more than one character
			}
			else {
				jassert(false);
				result.push_back('?');
			}
		}
		return result;
	}

	void VirusPatch::setName(std::string const &name)
	{
		ignoreUnused(name);
		//TODO
	}

	bool VirusPatch::isDefaultName() const
	{
		return name() == "- Init -";
	}

	std::shared_ptr<PatchNumber> VirusPatch::patchNumber() const
	{
		return std::make_shared<VirusPatchNumber>(place_);
	}

	void VirusPatch::setPatchNumber(MidiProgramNumber patchNumber)
	{
		place_ = patchNumber;
	}

	int VirusPatch::index(Page page, int index)
	{
		// We have stored the A and the B pages in one vector
		return page * 128 + index;
	}

	std::string VirusPatchNumber::friendlyName() const
	{
		char bankChar(char(programNumber_.toZeroBased() / 128 + 'a'));
		uint8 progNo = programNumber_.toZeroBased() % 128;
		return (boost::format("%c%d") % bankChar % progNo).str();
	}

}
