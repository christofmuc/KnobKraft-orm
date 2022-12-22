#pragma once

#include "LambdaButtonStrip.h"

#include "EditBufferHandler.h"

class Rev2;
class Patch;
class MidiController;
class SimpleLogger;

class Rev2ButtonStrip : public LambdaButtonStrip {
public:
	Rev2ButtonStrip(Rev2 &rev2, MidiController *controller, EditBufferHandler *handler, SimpleLogger *logger);
	virtual ~Rev2ButtonStrip();

private:
	EditBufferHandler::HandlerHandle programChangeHandle_;
	EditBufferHandler *handler_;
	bool isLocked_;
	MidiMessage lockedSequence_;
	std::shared_ptr<Patch> currentPatch_;
};
