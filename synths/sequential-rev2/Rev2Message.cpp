#include "Rev2Message.h"

#include "Rev2.h"

#include <boost/format.hpp>

namespace midikraft {

	Rev2Message::Rev2Message() : nrpn_number_msb_(0), nrpn_number_lsb_(0), nrpn_value_msb_(0), nrpn_value_lsb_(0)
	{
	}

	Rev2Message::~Rev2Message()
	{
	}

	bool Rev2Message::addMessage(MidiMessage const &message)
	{
		if (message.isController()) {
			switch (message.getRawData()[1]) {
			case 0x63:
				nrpn_number_msb_ = message.getRawData()[2];
				break;
			case 0x62:
				nrpn_number_lsb_ = message.getRawData()[2];
				break;
			case 0x06:
				nrpn_value_msb_ = message.getRawData()[2];
				break;
			case 0x26:
				nrpn_value_lsb_ = message.getRawData()[2];
				return true;
			default:
				// Ignored controller, not part of NRPN message
				break;
			}
		}
		return false;
	}

	int Rev2Message::nrpnController() const {
		return ((nrpn_number_msb_ << 7) | (nrpn_number_lsb_));
	}

	int Rev2Message::nrpnValue() const {
		return ((nrpn_value_msb_ << 7) | (nrpn_value_lsb_));
	}

	std::string Rev2Message::getName() const
	{
		return (boost::format("Set %s (#%d) to %d") % Rev2::nameOfNrpn(*this) % nrpnController() % nrpnValue()).str();
	}

}
