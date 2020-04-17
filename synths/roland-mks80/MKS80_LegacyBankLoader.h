#pragma once

#include "JuceHeader.h"

#include "Synth.h"

namespace midikraft {

	class MKS80_LegacyBankLoader {
	public:
		static TPatchVector loadM80File(std::vector<uint8> fileContent);
		static TPatchVector loadMKS80File(std::vector<uint8> fileContent);

	private:
		//! This reads a Pascal-like small string, where the first byte specifies the length of the string, and advances the read pointer
		static std::string readPascalString(std::vector<uint8>::iterator start, std::vector<uint8>::iterator &position, std::vector<uint8>::iterator const &end);
		static std::vector<uint8> readBinaryBlock(std::vector<uint8>::iterator &position, std::vector<uint8>::iterator const &end, int sizeToRead);
	};

}

