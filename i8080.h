#pragma once

#include "i8080_types.h"
#include "i8080_trace.hpp"
#include "memory.h"
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <vector>

typedef uint16_t uint16_t;

class Devices;

class I8080
{

private:
	std::vector<I8080Trace> traces;

	// x86 only:
	union Register
	{
		struct
		{
			uint8_t l;
			uint8_t h;
		} byte;
		uint16_t word;
	};

	Register regBC;
	Register regDE;
	Register regHL;

	uint16_t regSP;
	uint16_t regPC;
	uint16_t regPC_breakpoint;
	uint16_t regPC_watchpoint;
	uint16_t watchpoint_location;

	uint8_t  regA;

	struct PSW
	{
		PSW() {
			carry = 0;
			skip1 = 1;
			parity = 0;
			skip3 = 0;
			aux_carry = 0;
			skip5 = 0;
			zero = 0;
			sign = 0;
		}
		uint8_t Get() const {
			return sign << 7 | zero << 6 | aux_carry << 4 | parity << 2 | 1 << 1 | carry;
		}
		void Set(uint8_t v) {
			sign = (v >> 7) & 1;
			zero = (v >> 6) & 1;
			aux_carry = (v >> 4) & 1;
			parity = (v >> 2) & 1;
			carry = (v >> 0) & 1;
		}
		unsigned int carry:1;
		unsigned int skip1:1;   // no bit 1
		unsigned int parity:1;  // 1->even, 0->odd parity
		unsigned int skip3:1;   // no bit 3
		unsigned int aux_carry:1;
		unsigned int skip5:1;   // no bit 5
		unsigned int zero:1;
		unsigned int sign:1;
	};

	PSW	_PSW;
	PSW PSWTable[256];

	bool halt;
	bool interruptEnable;

	int singleStepCounter;   // num instructions to interrupt (for single step)

public:

	Memory  memory;

	I8080();
	~I8080();
	void DumpState() const;

	void Reset() { regPC = 0; }

	int ExecuteCycle(Devices *);
	bool Interrupt(int interruptVector);

	bool Halt() const {return halt;}
	bool Halt(bool h) {halt = h ; return halt;}

	bool InterruptEnable() const { return interruptEnable; }
	bool InterruptEnable(bool b) { interruptEnable = b; return interruptEnable; }

	uint8_t PSW() const { return _PSW.Get(); }
	uint8_t PSW(uint8_t b) { _PSW.Set(b); return PSW();}

	uint8_t A() const { return regA; }
	uint8_t A(uint8_t b) { regA = b; return regA; }

	uint8_t B() const { return regBC.byte.h; }
	uint8_t B(uint8_t b) { regBC.byte.h = b; return B(); }

	uint8_t C() const { return regBC.byte.l; }
	uint8_t C(uint8_t b) { regBC.byte.l = b; return C(); }

	uint8_t D() const { return regDE.byte.h; }
	uint8_t D(uint8_t b) { regDE.byte.h = b; return D(); }

	uint8_t E() const { return regDE.byte.l; }
	uint8_t E(uint8_t b) { regDE.byte.l = b; return E(); }

	uint8_t H() const { return regHL.byte.h; }
	uint8_t H(uint8_t b) { regHL.byte.h = b; return H(); }

	uint8_t L() const { return regHL.byte.l; }
	uint8_t L(uint8_t b) { regHL.byte.l = b; return L(); }

	uint8_t M() const { return memory.get_byte(regHL.word); }
	uint8_t M(uint8_t b) { memory.set_byte(regHL.word,b); return M();}

	uint16_t SP() const { return regSP; }
	uint16_t SP(uint16_t a) { regSP = a; return SP(); }

	uint16_t PC() const { return regPC; }
	uint16_t PC(uint16_t a) { regPC = a; return PC();}

	uint16_t BC() const { return regBC.word; }
	uint16_t BC(uint16_t a) { regBC.word = a; return BC(); }

	uint16_t DE() const { return regDE.word; }
	uint16_t DE(uint16_t a) { regDE.word = a; return DE(); }

	uint16_t HL() const { return regHL.word; }
	uint16_t HL(uint16_t a) { regHL.word = a; return HL(); }

	void Dump()
	{
	std::cerr << std::hex << std::showbase;

	std::cerr << std::setw(6);
	std::cerr << " regA = " << regA << std::endl;
	std::cerr << "regBC = " << regBC.word << std::endl;
	std::cerr << "regDE = " << regDE.word << std::endl;
	std::cerr << "regHL = " << regHL.word << std::endl;
	std::cerr << "regPC = " << regPC << std::endl;
	std::cerr << "regSP = " << regSP << std::endl;
	std::cerr << std::setw(0) << std::dec << std::resetiosflags(std::ios::showbase);
	}

	bool RunEmulatorCommand(const std::vector<std::string> &args);
	void RunTraces();
	std::string Disassemble(uint16_t pc);
	std::string Flags();
	int InstructionLength(uint16_t pc);
};
