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

	Register m_regBC;
	Register m_regDE;
	Register m_regHL;

	uint16_t m_regSP;
	uint16_t m_regPC;
	uint16_t m_regPC_breakpoint;
	uint16_t m_regPC_watchpoint;
	uint16_t m_watchpoint_location;

	uint8_t  m_regA;

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

	PSW	m_PSW;
	PSW m_PSWTable[256];

	bool m_halt;
	bool m_interruptEnable;

	int m_singleStepCounter;   // num instructions to interrupt (for single step)

public:

	Memory  memory;

	I8080();
	~I8080();
	void DumpState() const;

	void Reset() { m_regPC = 0; }

	int ExecuteCycle(Devices *);
	bool Interrupt(int interruptVector);

	bool Halt() const {return m_halt;}
	bool Halt(bool h) {m_halt = h ; return m_halt;}

	bool InterruptEnable() const { return m_interruptEnable; }
	bool InterruptEnable(bool b) { m_interruptEnable = b; return m_interruptEnable; }

	uint8_t PSW() const { return m_PSW.Get(); }
	uint8_t PSW(uint8_t b) { m_PSW.Set(b); return PSW();}

	uint8_t A() const { return m_regA; }
	uint8_t A(uint8_t b) { m_regA = b; return m_regA; }

	uint8_t B() const { return m_regBC.byte.h; }
	uint8_t B(uint8_t b) { m_regBC.byte.h = b; return B(); }

	uint8_t C() const { return m_regBC.byte.l; }
	uint8_t C(uint8_t b) { m_regBC.byte.l = b; return C(); }

	uint8_t D() const { return m_regDE.byte.h; }
	uint8_t D(uint8_t b) { m_regDE.byte.h = b; return D(); }

	uint8_t E() const { return m_regDE.byte.l; }
	uint8_t E(uint8_t b) { m_regDE.byte.l = b; return E(); }

	uint8_t H() const { return m_regHL.byte.h; }
	uint8_t H(uint8_t b) { m_regHL.byte.h = b; return H(); }

	uint8_t L() const { return m_regHL.byte.l; }
	uint8_t L(uint8_t b) { m_regHL.byte.l = b; return L(); }

	uint8_t M() const { return memory.get_byte(m_regHL.word); }
	uint8_t M(uint8_t b) { memory.set_byte(m_regHL.word,b); return M();}

	uint16_t SP() const { return m_regSP; }
	uint16_t SP(uint16_t a) { m_regSP = a; return SP(); }

	uint16_t PC() const { return m_regPC; }
	uint16_t PC(uint16_t a) { m_regPC = a; return PC();}

	uint16_t BC() const { return m_regBC.word; }
	uint16_t BC(uint16_t a) { m_regBC.word = a; return BC(); }

	uint16_t DE() const { return m_regDE.word; }
	uint16_t DE(uint16_t a) { m_regDE.word = a; return DE(); }

	uint16_t HL() const { return m_regHL.word; }
	uint16_t HL(uint16_t a) { m_regHL.word = a; return HL(); }

	void Dump()
	{
	std::cerr << std::hex << std::showbase;

	std::cerr << std::setw(6);
	std::cerr << " m_regA = " << m_regA << std::endl;
	std::cerr << "m_regBC = " << m_regBC.word << std::endl;
	std::cerr << "m_regDE = " << m_regDE.word << std::endl;
	std::cerr << "m_regHL = " << m_regHL.word << std::endl;
	std::cerr << "m_regPC = " << m_regPC << std::endl;
	std::cerr << "m_regSP = " << m_regSP << std::endl;
	std::cerr << std::setw(0) << std::dec << std::resetiosflags(std::ios::showbase);
	}

	bool RunEmulatorCommand(const std::vector<std::string> &args);
	void RunTraces();
	std::string Disassemble(uint16_t pc);
};
