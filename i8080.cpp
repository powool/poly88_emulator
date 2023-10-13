
#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>
#include "i8080.h"
#include "i8080_trace.hpp"
#include "poly88_devices.h"
#include "util.h"
#include "z80d.h"

I8080::I8080()
{
	int i,j;
	int ringsum, val;

	// for performance, build a table of flags - one for each
	// possible byte value.
	for(i=0; i<256; i++)
	{
		m_PSWTable[i].zero = (i == 0);

		m_PSWTable[i].sign = (i >= 0x80);

		ringsum = 0;
		val = i;
		for(j=0; j<8; j++)
		{
			ringsum ^= (val & 0x01);
			val = val>>1;
		}
		m_PSWTable[i].parity = (~ringsum) & 0x01;
	}

	m_regPC_breakpoint = 0x0;
	m_regPC_watchpoint = 0;
	m_watchpoint_location = 0x0c0e;
}

I8080::~I8080()
{
	std::cerr << "Shutting down the 8080 emulator." << std::endl;
}

void I8080::DumpState() const
{
	std::cout << "reg  a=" << util::hex(2) << A() << std::endl;
	std::cout << "reg bc=" << util::hex(4) << BC() << std::endl;
	std::cout << "reg de=" << util::hex(4) << DE() << std::endl;
	std::cout << "reg hl=" << util::hex(4) << HL() << std::endl;
}

int I8080::ExecuteCycle(Devices *dev)
{
	uint16_t    tmp16;
	uint8_t          tmp8;
	uint8_t          tbyte1, tbyte2;
	uint16_t    tword1;

// instructions that are invalid call this macro
#define INVALID() { ; }

#define PUSH(val) \
	{SP(SP()-2); memory.set_2byte(SP(),val);}

#define POP() \
	(SP(SP()+2), memory.get_2byte(SP()-2))

#define IMMEDIATE_BYTE() \
	(memory.get_byte(PC()+1))

#define IMMEDIATE_2BYTE() \
	(memory.get_2byte(PC()+1))


#define MOVE_RR(opcode, r1, r2) \
		case opcode: \
			r1(r2()); \
			PC(PC() + 1); \
			return 0;

#define ADD_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tword1 = tbyte1+tbyte2; \
			A((uint8_t) tword1); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = ( (tbyte1 & 0xf) + (tbyte2 & 0xf) > 0xf); \
			m_PSW.carry = (tword1 > 0xff); \
			PC(PC() + 1); \
			return 0;

#define ADC_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tword1 = tbyte1+tbyte2 + m_PSW.carry; \
			A((uint8_t) tword1); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = ( m_PSW.carry + (tbyte1 & 0xf) + (tbyte2 & 0xf) > 0xf); \
			m_PSW.carry = (tword1 > 0xff); \
			PC(PC() + 1); \
			return 0;


#define SUB_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tmp8 = tbyte1 - tbyte2; \
			A(tmp8); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = ( (tbyte1 & 0xf) <  (tbyte2 & 0xf) ); \
			m_PSW.carry = (tbyte1 < tbyte2); \
			PC(PC() + 1); \
			return 0;

#define SBB_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tword1 = tbyte1 - tbyte2 - m_PSW.carry; \
			A((uint8_t) tword1); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = ( (tbyte1 & 0xf) <  (tbyte2 & 0xf) + m_PSW.carry); \
			m_PSW.carry = (tbyte1 < (tbyte2+m_PSW.carry)); \
			PC(PC() + 1); \
			return 0;

#define ANA_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tmp8 = tbyte1 & tbyte2; \
			A(tmp8); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = ((tbyte1 | tbyte2) & 0x08) != 0; \
			m_PSW.carry = 0; \
			PC(PC() + 1); \
			return 0;

#define XRA_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tmp8 = tbyte1 ^ tbyte2; \
			A(tmp8); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = 0; \
			m_PSW.carry = 0; \
			PC(PC() + 1); \
			return 0;

#define ORA_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tmp8 = tbyte1 | tbyte2; \
			A(tmp8); \
			m_PSW.zero = m_PSWTable[A()].zero; \
			m_PSW.sign = m_PSWTable[A()].sign; \
			m_PSW.parity = m_PSWTable[A()].parity; \
			m_PSW.aux_carry = 0; \
			m_PSW.carry = 0; \
			PC(PC() + 1); \
			return 0;

#define CMP_R(opcode, r) \
		case opcode: \
			tbyte1 = A(); \
			tbyte2 = r(); \
			tmp8 = tbyte1 - tbyte2; \
			m_PSW.zero = m_PSWTable[tmp8].zero; \
			m_PSW.sign = m_PSWTable[tmp8].sign; \
			m_PSW.parity = m_PSWTable[tmp8].parity; \
			m_PSW.aux_carry = ( (tbyte1 & 0xf) <  (tbyte2 & 0xf) ); \
			m_PSW.carry = (tbyte1 < tbyte2); \
			PC(PC() + 1); \
			return 0;

#define LXI_R(opcode, rp) \
		case opcode:    /* LXI D,xxxx */ \
			rp(IMMEDIATE_2BYTE()); \
			PC(PC() + 3); \
			return 0;

#define STAX_R(opcode, rp) \
		case opcode:    /* STAX D */ \
			memory.set_byte(rp(), A()); \
			PC(PC() + 1); \
			return 0;

#define INX_R(opcode, rp) \
		case opcode: \
			rp(rp() + 1); \
			PC(PC() + 1); \
			return 0;

#define INR_R(opcode, r) \
		case opcode:    /* INR r */ \
			tbyte1 = r(); \
			r(r() + 1); \
			m_PSW.zero = m_PSWTable[r()].zero; \
			m_PSW.sign = m_PSWTable[r()].sign; \
			m_PSW.parity = m_PSWTable[r()].parity; \
			m_PSW.aux_carry = (tbyte1 & 0xf) == 0xf; \
			PC(PC() + 1); \
			return 0;

#define DCR_R(opcode, r) \
		case opcode:    /* DCR r */ \
			tbyte1 = r(); \
			r(r() - 1); \
			m_PSW.zero = m_PSWTable[r()].zero; \
			m_PSW.sign = m_PSWTable[r()].sign; \
			m_PSW.parity = m_PSWTable[r()].parity; \
			m_PSW.aux_carry = ((tbyte1 & 0xf) == 0); \
			PC(PC() + 1); \
			return 0;

#define MVI_R(opcode, r) \
		case opcode:    /* MVI r,xx */ \
			r(IMMEDIATE_BYTE()); \
			PC(PC() + 2); \
			return 0;

#define DAD_R(opcode, rp) \
		case opcode:    /* DAD rp */ \
			tmp16 = HL(); \
			HL(HL() + rp()); \
			m_PSW.carry = (HL() < tmp16); \
			PC(PC() + 1); \
			return 0;

#define LDAX_R(opcode, r) \
		case opcode:    /* LDAX r */ \
			A(memory.get_byte(r())); \
			PC(PC() + 1); \
			return 0;

#define DCX_R(opcode, rp) \
		case opcode: \
			rp(rp() - 1); \
			PC(PC() + 1); \
			return 0;

#define RST(opcode, n) \
		case opcode:    /* RST n */ \
			PUSH(PC()+1); \
			PC(n*8); \
			return 0;

	if(m_singleStepCounter > 0 && --m_singleStepCounter == 0) Interrupt(7);
	RunTraces();
	if(m_regPC_breakpoint && m_regPC_breakpoint==PC())
	{
		std::cerr << "got to breakpoint at pc=0x" << util::hex(4) << PC() << std::endl;
	}
	if(m_regPC_watchpoint && m_regPC_watchpoint==PC())
	{
		tmp16 = memory.get_2byte(m_watchpoint_location);
		std::cout << "at watchpoint 0x" << util::hex(4) << m_regPC_watchpoint << ", location 0x" << m_watchpoint_location << "=0x" << tmp16 << "\n";
	}

	// decode instruction and execute it
	switch(memory.get_byte(PC()))
	{

		case 0x00:  /* NOP */
		case 0x08:  /* undocumented */
		case 0x10:
		case 0x18:
		case 0x20:
		case 0x28:
		case 0x30:
		case 0x38:
			PC(PC() + 1);
			return 0;

			LXI_R(0x01, BC);
			STAX_R(0x02, BC);
			INX_R(0x03, BC);
			INR_R(0x04, B);
			DCR_R(0x05, B);
			MVI_R(0x06, B);

		case 0x07:  /* RLC */
			m_PSW.carry = (A() >> 7) & 1;
			A(A() << 1);
			A(A() | m_PSW.carry);
			PC(PC() + 1);
			return 0;

			DAD_R(0x09, BC);
			LDAX_R(0x0a, BC);
			DCX_R(0x0b, BC);
			INR_R(0x0c, C);
			DCR_R(0x0d, C);
			MVI_R(0x0e, C);

		case 0x0f:  /* RRC */
			m_PSW.carry = A() & 1;
			A(A() >> 1);
			A(A() | (m_PSW.carry << 7));
			PC(PC() + 1);
			return 0;


			LXI_R(0x11, DE);
			STAX_R(0x12, DE);
			INX_R(0x13, DE);
			INR_R(0x14, D);
			DCR_R(0x15, D);
			MVI_R(0x16, D);

		case 0x17:  /* RAL */
			tmp8 = (A() >> 7) & 1;  // hold high bit
			A(A() << 1);
			A(A() | (m_PSW.carry));
			m_PSW.carry = tmp8;
			PC(PC() + 1);
			return 0;

			DAD_R(0x19, DE);
			LDAX_R(0x1a, DE);
			DCX_R(0x1b, DE);
			INR_R(0x1c, E);
			DCR_R(0x1d, E);
			MVI_R(0x1e, E);

		case 0x1f:  /* RAR */
			tmp8 = A() & 1;   /* save low order bit */
			A(A() >> 1);
			A(A() | (m_PSW.carry << 7));
			m_PSW.carry = tmp8;
			PC(PC() + 1);
			return 0;

			LXI_R(0x21, HL);

		case 0x22:  /* SHLD */
			memory.set_2byte(IMMEDIATE_2BYTE(),HL());
			PC(PC() + 3);
			return 0;

			INX_R(0x23, HL);
			INR_R(0x24, H);
			DCR_R(0x25, H);
			MVI_R(0x26, H);

		case 0x27:  /* DAA */
			if(m_PSW.aux_carry || (A() & 0xf)>9)
			{
				A(A() + 6);
				m_PSW.aux_carry = 1;
			}
			if(m_PSW.carry || (A() >> 4) > 9 || ((A() >> 4) >= 9 && (A() & 0x0f) > 9))
			{
				A(A() + 0x60);
				m_PSW.carry = 1;
			}
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			PC(PC() + 1);
			return 0;

			DAD_R(0x29, HL);

		case 0x2a:  /* LHLD */
			HL(memory.get_2byte(IMMEDIATE_2BYTE()));
			PC(PC() + 3);
			return 0;

			DCX_R(0x2b, HL);
			INR_R(0x2c, L);
			DCR_R(0x2d, L);
			MVI_R(0x2e, L);

		case 0x2f:  /* CMA */
			A(~A());
			PC(PC() + 1);
			return 0;

			LXI_R(0x31, SP);

		case 0x32:  /* STA xxxx */
			memory.set_byte(IMMEDIATE_2BYTE(), A());
			PC(PC() + 3);
			return 0;

			INX_R(0x33, SP);
			INR_R(0x34, M);
			DCR_R(0x35, M);
			MVI_R(0x36, M);

		case 0x37:  /* STC */
			m_PSW.carry = 1;
			PC(PC() + 1);
			return 0;

			DAD_R(0x39, SP);

		case 0x3a:  /* LDA xxxx */
			A(memory.get_2byte(IMMEDIATE_2BYTE()));
			PC(PC() + 3);
			return 0;

			DCX_R(0x3b, SP);
			INR_R(0x3c, A);
			DCR_R(0x3d, A);
			MVI_R(0x3e, A);

		case 0x3f:  /* CMC */
			m_PSW.carry = !m_PSW.carry;
			PC(PC() + 1);
			return 0;

// register moves:
			MOVE_RR(0x40, B, B);
			MOVE_RR(0x41, B, C);
			MOVE_RR(0x42, B, D);
			MOVE_RR(0x43, B, E);
			MOVE_RR(0x44, B, H);
			MOVE_RR(0x45, B, L);
			MOVE_RR(0x46, B, M);
			MOVE_RR(0x47, B, A);

			MOVE_RR(0x48, C, B);
			MOVE_RR(0x49, C, C);
			MOVE_RR(0x4a, C, D);
			MOVE_RR(0x4b, C, E);
			MOVE_RR(0x4c, C, H);
			MOVE_RR(0x4d, C, L);
			MOVE_RR(0x4e, C, M);
			MOVE_RR(0x4f, C, A);

			MOVE_RR(0x50, D, B);
			MOVE_RR(0x51, D, C);
			MOVE_RR(0x52, D, D);
			MOVE_RR(0x53, D, E);
			MOVE_RR(0x54, D, H);
			MOVE_RR(0x55, D, L);
			MOVE_RR(0x56, D, M);
			MOVE_RR(0x57, D, A);

			MOVE_RR(0x58, E, B);
			MOVE_RR(0x59, E, C);
			MOVE_RR(0x5a, E, D);
			MOVE_RR(0x5b, E, E);
			MOVE_RR(0x5c, E, H);
			MOVE_RR(0x5d, E, L);
			MOVE_RR(0x5e, E, M);
			MOVE_RR(0x5f, E, A);

			MOVE_RR(0x60, H, B);
			MOVE_RR(0x61, H, C);
			MOVE_RR(0x62, H, D);
			MOVE_RR(0x63, H, E);
			MOVE_RR(0x64, H, H);
			MOVE_RR(0x65, H, L);
			MOVE_RR(0x66, H, M);
			MOVE_RR(0x67, H, A);

			MOVE_RR(0x68, L, B);
			MOVE_RR(0x69, L, C);
			MOVE_RR(0x6a, L, D);
			MOVE_RR(0x6b, L, E);
			MOVE_RR(0x6c, L, H);
			MOVE_RR(0x6d, L, L);
			MOVE_RR(0x6e, L, M);
			MOVE_RR(0x6f, L, A);

			MOVE_RR(0x70, M, B);
			MOVE_RR(0x71, M, C);
			MOVE_RR(0x72, M, D);
			MOVE_RR(0x73, M, E);
			MOVE_RR(0x74, M, H);
			MOVE_RR(0x75, M, L);

		case 0x76:  /* HLT */
			Halt(true);
			PC(PC() + 1);
			return 0;

			MOVE_RR(0x77, M, A);
			MOVE_RR(0x78, A, B);
			MOVE_RR(0x79, A, C);
			MOVE_RR(0x7a, A, D);
			MOVE_RR(0x7b, A, E);
			MOVE_RR(0x7c, A, H);
			MOVE_RR(0x7d, A, L);
			MOVE_RR(0x7e, A, M);
			MOVE_RR(0x7f, A, A);

			ADD_R(0x80, B);
			ADD_R(0x81, C);
			ADD_R(0x82, D);
			ADD_R(0x83, E);
			ADD_R(0x84, H);
			ADD_R(0x85, L);
			ADD_R(0x86, M);
			ADD_R(0x87, A);

			ADC_R(0x88, B);
			ADC_R(0x89, C);
			ADC_R(0x8a, D);
			ADC_R(0x8b, E);
			ADC_R(0x8c, H);
			ADC_R(0x8d, L);
			ADC_R(0x8e, M);
			ADC_R(0x8f, A);

			SUB_R(0x90, B);
			SUB_R(0x91, C);
			SUB_R(0x92, D);
			SUB_R(0x93, E);
			SUB_R(0x94, H);
			SUB_R(0x95, L);
			SUB_R(0x96, M);
			SUB_R(0x97, A);

			SBB_R(0x98, B);
			SBB_R(0x99, C);
			SBB_R(0x9a, D);
			SBB_R(0x9b, E);
			SBB_R(0x9c, H);
			SBB_R(0x9d, L);
			SBB_R(0x9e, M);
			SBB_R(0x9f, A);

			ANA_R(0xa0, B);
			ANA_R(0xa1, C);
			ANA_R(0xa2, D);
			ANA_R(0xa3, E);
			ANA_R(0xa4, H);
			ANA_R(0xa5, L);
			ANA_R(0xa6, M);
			ANA_R(0xa7, A);

			XRA_R(0xa8, B);
			XRA_R(0xa9, C);
			XRA_R(0xaa, D);
			XRA_R(0xab, E);
			XRA_R(0xac, H);
			XRA_R(0xad, L);
			XRA_R(0xae, M);
			XRA_R(0xaf, A);

			ORA_R(0xb0, B);
			ORA_R(0xb1, C);
			ORA_R(0xb2, D);
			ORA_R(0xb3, E);
			ORA_R(0xb4, H);
			ORA_R(0xb5, L);
			ORA_R(0xb6, M);
			ORA_R(0xb7, A);

			CMP_R(0xb8, B);
			CMP_R(0xb9, C);
			CMP_R(0xba, D);
			CMP_R(0xbb, E);
			CMP_R(0xbc, H);
			CMP_R(0xbd, L);
			CMP_R(0xbe, M);
			CMP_R(0xbf, A);

		case 0xc0:  /* RNZ */
			if(!m_PSW.zero) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xc1:  /* POP B */
			BC(POP());
			PC(PC() + 1);
			return 0;

		case 0xc2:  /* JNZ xxxx */
			if(!m_PSW.zero) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xc3:  /* JMP xxxx */
//		case 0xcb:  /* undocumented */
			PC(IMMEDIATE_2BYTE());
			return 0;

		case 0xc4:  /* CNZ xxxx */
			if(!m_PSW.zero)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xc5:  /* PUSH B */
			PUSH(BC());
			PC(PC() + 1);
			return 0;

		case 0xc6:  /* ADI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tword1 = tbyte1+tbyte2;
			A((uint8_t) tword1);
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.aux_carry = ((tbyte1 & 0xf) + (tbyte2 & 0xf) > 0xf);
			m_PSW.carry = (tword1 > 0xff);
			PC(PC() + 2);
			return 0;

			RST(0xc7,0);
		case 0xc8:  /* RZ */
			if(m_PSW.zero) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xc9:  /* RET */
//		case 0xd9:  /* undocumented */
			PC(POP());
			return 0;

		case 0xca:  /* JZ xxxx */
			if(m_PSW.zero) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xcb:  /* INVALID */
			INVALID();
			PC(PC() + 1);
			return 0;

		case 0xcc:  /* CZ xxxx */
			if(m_PSW.zero)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xcd:  /* CALL xxxx */
//		case 0xdd:	/* undocumented */
//		case 0xed:
//		case 0xfd:
			PUSH(PC()+3);
			PC(IMMEDIATE_2BYTE());
			return 0;

		case 0xce:  /* ACI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tword1 = tbyte1 + tbyte2 + m_PSW.carry;
			A((uint8_t) tword1);
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.aux_carry = ((tword1 & 0xf) > 0xf);
			m_PSW.carry = (tword1 > 0xff);
			PC(PC() + 2);
			return 0;

			RST(0xcf,1);

		case 0xd0:  /* RNC */
			if(!m_PSW.carry) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xd1:  /* POP D */
			DE(POP());
			PC(PC() + 1);
			return 0;

		case 0xd2:  /* JNC xxxx */
			if(!m_PSW.carry) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xd3:  /* OUT xx */
			tmp8 = IMMEDIATE_BYTE();
#if 0
			if(tmp8 != 8)
			{
				fprintf(stderr,"doing OUT %d\n",tmp8);
				fprintf(stderr,"dev->input_ports[%d]=%x, dev->output_ports[%d]=%x\n",
					tmp8, dev->input_ports[tmp8], tmp8, dev->output_ports[tmp8]);
			}
#endif
			if(tmp8==12)
				m_singleStepCounter = 3;   // port 12 initializes SS interrupt
			else
				dev->OutputTo(tmp8, A());
			PC(PC() + 2);
			return 0;

		case 0xd4:  /* CNC xxxx */
			if(!m_PSW.carry)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xd5:  /* PUSH D */
			PUSH(DE());
			PC(PC() + 1);
			return 0;

		case 0xd6:  /* SUI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tmp8 = tbyte1 - tbyte2;
			A(tmp8);
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.aux_carry = ((tbyte1 & 0xf) < (tbyte2 & 0xf));
			m_PSW.carry = (tbyte1 < tbyte2);
			PC(PC() + 2);
			return 0;

			RST(0xd7,2);

		case 0xd8:  /* RC */
			if(m_PSW.carry) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xd9:      /* INVALID */
			INVALID();
			PC(PC() + 1);
			return 0;

		case 0xda:  /* JC xxxx */
			if(m_PSW.carry) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xdb:  /* IN xx */
			tmp8 = IMMEDIATE_BYTE();
			A(dev->InputFrom(tmp8));
			PC(PC() + 2);
			return 0;

		case 0xdc:  /* CC xxxx */
			if(m_PSW.carry)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xdd:      /* INVALID */
			INVALID();
			PC(PC() + 1);
			return 0;

		case 0xde:  /* SBI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tword1 = tbyte1 - tbyte2 - m_PSW.carry;
			A((uint8_t) tword1);
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.aux_carry = ((tbyte1 & 0xf) < (tbyte2 & 0xf + m_PSW.carry));
			m_PSW.carry = (tbyte1 < (tbyte2+m_PSW.carry));
			PC(PC()+2);
			return 0;

			RST(0xdf,3);

		case 0xe0:  /* RPO */
			if(!m_PSW.parity) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xe1:  /* POP H */
			HL(POP());
			PC(PC() + 1);
			return 0;

		case 0xe2:  /* JPO xxxx */
			if(!m_PSW.parity) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xe3:  /* XTHL */
			tmp16 = POP();
			PUSH(HL());
			HL(tmp16);
			PC(PC() + 1);
			return 0;

		case 0xe4:  /* CPO xxxx */
			if(!m_PSW.parity)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xe5:  /* PUSH H */
			PUSH(HL());
			PC(PC() + 1);
			return 0;

		case 0xe6:  /* ANI xx */
			A(A() & IMMEDIATE_BYTE());
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.carry = 0;
			m_PSW.aux_carry = 0;
			PC(PC() + 2);
			return 0;

			RST(0xe7,4);

		case 0xe8:  /* RPE */
			if(m_PSW.parity) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xe9:  /* PCHL   (PC<->hl) */
			tmp16 = HL();
			HL(PC());
			PC(tmp16);
			return 0;

		case 0xea:  /* JPE xxxx */
			if(m_PSW.parity) PC(IMMEDIATE_2BYTE());
			else PC(PC() + 3);
			return 0;

		case 0xeb:  /* XCHG (de<->hl) */
			tmp16 = HL();
			HL(DE());
			DE(tmp16);
			PC(PC() + 1);
			return 0;

		case 0xec:  /* CPE xxxx */
			if(m_PSW.parity)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xed:
			INVALID();
			PC(PC() + 1);
			return 0;

		case 0xee:  /* XRI xx */
			A(A() ^ IMMEDIATE_BYTE());
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.carry = 0;
			m_PSW.aux_carry = 0;
			PC(PC() + 2);
			return 0;

			RST(0xef,5);

		case 0xf0:  /* RP */
			if(!m_PSW.sign) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xf1:  /* POP PSW */
			tmp16 = POP();
			m_PSW.Set(tmp16 & 0xff);
			A((uint8_t)(tmp16 >> 8));
			PC(PC() + 1);
			return 0;

		case 0xf2:  /* JP xxxx */
			if(!m_PSW.sign)
			{
				PC(IMMEDIATE_2BYTE());
				return 0;
			}
			PC(PC() + 3);
			return 0;

		case 0xf3:  /* DI */
			InterruptEnable(false);
			PC(PC() + 1);
			return 0;

		case 0xf4:  /* CP xxxx */
			if(!m_PSW.sign)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
				return 0;
			}
			PC(PC() + 3);
			return 0;

		case 0xf5:  /* PUSH PSW */
			tmp16 = (A() << 8) | m_PSW.Get();
			PUSH(tmp16);
			PC(PC() + 1);
			return 0;

		case 0xf6:  /* ORI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tmp8 = tbyte1 | tbyte2;
			A(tmp8);
			m_PSW.zero = m_PSWTable[A()].zero;
			m_PSW.sign = m_PSWTable[A()].sign;
			m_PSW.parity = m_PSWTable[A()].parity;
			m_PSW.carry = 0;
			PC(PC() + 2);
			return 0;

			RST(0xf7,6);

		case 0xf8:  /* RM */
			if(m_PSW.sign) PC(POP());
			else PC(PC() + 1);
			return 0;

		case 0xf9:  /* SPHL */
			SP(HL());
			PC(PC() + 1);
			return 0;

		case 0xfa:  /* JM xxxx */
			if(m_PSW.sign)
				PC(IMMEDIATE_2BYTE());
			else
				PC(PC() + 3);
			return 0;

		case 0xfb:  /* EI */
			InterruptEnable(true);
			PC(PC() + 1);
			return 0;

		case 0xfc:  /* CM xxxx */
			if(m_PSW.sign)
			{
				PUSH(PC()+3);
				PC(IMMEDIATE_2BYTE());
			}
			else PC(PC() + 3);
			return 0;

		case 0xfd:  /* INVALID */
			INVALID();
			PC(PC() + 1);
			return 0;

		case 0xfe:  /* CPI xx */
			tbyte1 = A();
			tbyte2 = IMMEDIATE_BYTE();
			tmp8 = tbyte1 - tbyte2;
			m_PSW.zero = m_PSWTable[tmp8].zero;
			m_PSW.sign = m_PSWTable[tmp8].sign;
			m_PSW.parity = m_PSWTable[tmp8].parity;
			m_PSW.aux_carry = ((tbyte1 & 0xf) < (tbyte2 & 0xf));
			m_PSW.carry = (tbyte1 < tbyte2);
			PC(PC() + 2);
			return 0;

			RST(0xff,7);

		default:
			return 1;
	}
	return 1;
}

bool I8080::Interrupt(int i)
{
	if(!InterruptEnable())
		return false;

	PUSH(PC());
	PC((uint16_t) i*8);
	InterruptEnable(false);
	return true;
}

void I8080::RunTraces()
{
	for(auto &trace : traces) {
		switch(trace.what) {
			case I8080Trace::PC:
				if (trace.when == I8080Trace::WHEN_RANGE &&
						trace.action == I8080Trace::SKIP_TRACING &&
						trace.inRange(PC())) {
					return;
				}
				if (trace.when == I8080Trace::WHEN_RANGE &&
						trace.action == I8080Trace::DISASSEMBLY &&
						trace.inRange(PC())) {
					std::cerr << Disassemble(PC()) << std::endl;
					continue;
				}
				break;
			default:
				break;
		}
	}
}

bool I8080::RunEmulatorCommand(const std::vector<std::string> &args)
{
	if (args.size() == 4 && args[0] == "trace" && args[1] == "skip") {
		uint16_t low = std::stoi(args[2], nullptr, 16);
		uint16_t high = std::stoi(args[3], nullptr, 16);
		I8080Trace newTrace(I8080Trace::PC, I8080Trace::WHEN_RANGE,I8080Trace::SKIP_TRACING, low, high);
		traces.push_back(newTrace);
		std::cout << "set trace skip for range (" << std::hex << low << ", " << high << ")." << std::endl;
	}
	if (args.size() == 4 && args[0] == "trace" && args[1] == "pc") {
		uint16_t low = std::stoi(args[2], nullptr, 16);
		uint16_t high = std::stoi(args[3], nullptr, 16);
		I8080Trace newTrace(I8080Trace::PC, I8080Trace::WHEN_RANGE,I8080Trace::DISASSEMBLY, low, high);
		traces.push_back(newTrace);
		std::cout << "set pc tracing for range (" << std::hex << low << ", " << high << ")." << std::endl;
	}

	return false;
}

std::string I8080::Disassemble(uint16_t pc)
{
	std::stringstream outputLine;

	outputLine << util::hex(4) << PC() << "   ";

	unsigned char instruction[8];
	instruction[0] = memory.get_byte(PC());
	instruction[1] = memory.get_byte(PC()+1);
	instruction[2] = memory.get_byte(PC()+2);
	instruction[3] = memory.get_byte(PC()+3);
	instruction[4] = memory.get_byte(PC()+4);
	instruction[5] = memory.get_byte(PC()+5);
	instruction[6] = memory.get_byte(PC()+6);
	instruction[7] = memory.get_byte(PC()+7);

	const unsigned char *in = instruction;
	char args[64];

	auto z80_instruction_size = z80_disassemble_size(in);

	outputLine << util::hex(2) << static_cast<uint16_t>(memory.get_byte(PC())) << " ";
	if(z80_instruction_size>1)
		outputLine << util::hex(2) << static_cast<uint16_t>(memory.get_byte(PC()+1)) << " ";
	else
		outputLine << "   ";
	if(z80_instruction_size>2)
		outputLine << util::hex(2) << static_cast<uint16_t>(memory.get_byte(PC()+2)) << " " ;
	else
		outputLine << "   ";

	auto z80_instruction = z80_disassemble( in, args, 0, true );

	outputLine.fill(' ');
	outputLine.width(5);
	outputLine << std::left << z80_instruction;
	outputLine.width(14);
	outputLine << std::left << args;
	outputLine.width(0);
	outputLine << std::right;

	auto tos = memory.get_2byte(SP());
	outputLine << "a:" << util::hex(2) << static_cast<uint16_t>(A()) << " bc=" << util::hex(4) << BC() << " de=" << util::hex(4) << DE() << " hl=" << util::hex(4) << HL() << " m=" << util::hex(2) << static_cast<uint16_t>(M()) << " sp=" << util::hex(4) << SP() << " *sp=" << tos << "\tpsw=";
	if(m_PSW.zero) outputLine << "Z,";
	else outputLine << "NZ,";
	if(m_PSW.parity) outputLine << "PE,";
	else outputLine << "PO,";
	if(m_PSW.carry) outputLine << "C,";
	else outputLine << "NC,";
	if(m_PSW.aux_carry) outputLine << "AC";
	else outputLine << "NAC";

	return std::move(outputLine.str());
}
