#pragma once

#include <stdint.h>

class I8080Trace {
	uint8_t oldMem8;
	uint8_t oldMem16;
	uint16_t oldPC;
	uint16_t lowRange;
	uint16_t highRange;
public:
	enum eWhat {
		PC,
		SP,
		BC,
		DE,
		HL,
		MEM8,
		MEM16
	} what;
	enum eWhen {
		WHEN_EQUAL,
		WHEN_NOT_EQUAL,
		WHEN_RANGE,
		WHEN_NOT_RANGE
	} when;
	enum eAction {
		SKIP_TRACING,
		HALT,
		BREAK_PC,
		DUMP,
		DISASSEMBLY
	} action;

	I8080Trace(eWhat _what, eWhen _when, eAction _action, uint16_t low = 0, uint16_t high = 0) :
		what(_what),
		when(_when),
		action(_action),
		lowRange(low),
		highRange(high)
	{
		;
	}
	bool inRange(uint16_t value) {
		return value >= lowRange && value <= highRange;
	}
};
