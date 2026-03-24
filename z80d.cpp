// source: https://gist.github.com/raxoft/b90b7844341fe79744fe
//
// Z80 disassembler.
//
// Written by Patrik Rak for ZXDS in 2010, released under MIT license in 2011.
//
// There are two main entry points, one for doing the full blown disassembly,
// the other one for just measuring how many bytes given opcode sequence takes.
//
// The full details are below:
//
// const char *
// z80_disassemble(
//   const byte * & in,
//   char * args,
//   uint address = 0,
//   bool hexadecimal = false,
//   uint * target_address = NULL
// )
//
// Disassemble the sequence pointed to by +in+.
// The Z80 instruction set implies the sequence is no more than 4 bytes long at maximum.
// Returns pointer to constant string containing the name of the instruction.
// The arguments are stored as null terminated string into the +args+ buffer.
// The buffer size is implied by the Z80 instruction set, so 32 bytes shall entirely suffice.
// The +in+ pointer is updated, so it can be used for both measuring the original opcode size
// as well as immediately disassembling the following one.
// The +address+ value shall match the address where the opcode resides in the Z80 memory.
// It is needed only to compute the absolute address of relative jumps, though.
// The +hexadecimal+ flag simply switches between decimal and hexadecimal output.
// The +target_address+ pointer, if set, will be used for storing whichever 16 bit value
// the instruction uses. If the instruction references no such value, that location is not modified.
// Handy for finding out what memory address the instruction accesses or where it jumps to, etc.
//
// uint
// z80_disassemble_size(
//   const byte * in
// )
//
// Measure how many bytes belong to the opcode sequence pointed to by +in+.
// Note that +in+ is not updated in this case, as the result can be easily used
// for incrementing it instead if necessary.
//
// There is a simple but comprehensive disassembler test at end of the file, uncomment it
// if you want to test how this compiles or to check how the disassembler output looks like.
//
// Enjoy!
//
// Patrik

#include <stdlib.h>
#include <stdio.h>

// The types I use below. You may want to adjust this to match your own types instead.

typedef unsigned char byte ;    // 8 bit unsigned value.
typedef signed char sbyte ;     // 8 bit signed value.
typedef unsigned short word ;   // 16 bit unsigned value.
typedef unsigned int uint ;     // 32+ bit unsigned value.
typedef signed int sint ;       // 32+ bit signed value.

// The entry point prototypes. You will likely want to put them to a header file instead.

const char * z80_disassemble( const byte * & in, char * args, uint address = 0, bool hexadecimal = false, uint * target_address = NULL ) ;
uint z80_disassemble_size( const byte * in ) ;

// Argument types.

enum {
    A_END = 0,      // End of args.
    A_REG,          // Register B C D E H L (HL) A.
    A__REG_,        // Dtto in ().
    A_PAIR,         // Register pair BC DE HL SP.
    A__PAIR_,       // Dtto in ().
    A_PAIR_AF,      // Register pair AF or AF'.
    A_IR,           // Register I R.
    A_BIT,          // Bit N or other small number.
    A_BYTE,         // Byte N.
    A__BYTE_,       // Dtto in ().
    A_WORD,         // Word NN.
    A__WORD_,       // Dtto in ().
    A_COND,         // Condition NZ Z NC C PO PE P M.
    A_REL,          // Relative jump offset.
    A_IX,           // Use IX instead of HL.
    A_IY,           // Use IY instead of HL.
    A_DISP,         // IX/IY displacement.
    A_SWAP,         // Swap the first two arguments.
} ;

// Instruction names.
//
// Don't change the order, the low values are important.

#define NAME_TABLE \
INST(NOP) \
INST(LD) \
INST(INC) \
INST(DEC) \
INST(DJNZ) \
INST(JR) \
INST(RLCA) \
INST(RRCA) \
INST(RLA) \
INST(RRA) \
INST(DAA) \
INST(CPL) \
INST(SCF) \
INST(CCF) \
INST(EX) \
\
INST(ADD) \
INST(ADC) \
INST(SUB) \
INST(SBC) \
INST(AND) \
INST(XOR) \
INST(OR) \
INST(CP) \
\
INST(JP) \
INST(RET) \
INST(CALL) \
INST(PUSH) \
INST(POP) \
INST(IN) \
INST(OUT) \
INST(EXX) \
INST(RST) \
INST(DI) \
INST(EI) \
\
INST(IM) \
INST(RETI) \
INST(RETN) \
INST(RRD) \
INST(RLD) \
INST(NEG) \
\
INST(RLC) \
INST(RRC) \
INST(RL) \
INST(RR) \
INST(SLA) \
INST(SRA) \
INST(SLIA) \
INST(SRL) \
INST(BIT) \
INST(RES) \
INST(SET) \
\
INST(LDI) \
INST(CPI) \
INST(INI) \
INST(OUTI) \
INST(LDD) \
INST(CPD) \
INST(IND) \
INST(OUTD) \
INST(LDIR) \
INST(CPIR) \
INST(INIR) \
INST(OTIR) \
INST(LDDR) \
INST(CPDR) \
INST(INDR) \
INST(OTDR) \
\
INST(HALT) \
INST(DB)

enum {
#define INST(n) I_##n,
    NAME_TABLE
#undef INST
    I_COUNT
} ;

static const char * instruction_names[ I_COUNT ] = {
#define INST(n) #n,
    NAME_TABLE
#undef INST
} ;

// Disassemble the 0x00-0x3F range.

static uint basic_003f_disassemble( const byte op, const byte * & in, uint * args, bool xy )
{
    enum {
        R = 1 << 5,
        C = R,
        N = 2 << 5,
        NN = 3 << 5,
        NN_HL = 4 << 5,
        NN_A = 5 << 5,
    } ;
    static const byte table[0x40] = {
        I_NOP,  I_LD|NN, I_LD|R, I_INC, I_INC|R, I_DEC|R, I_LD|N, I_RLCA,
        I_EX, I_ADD, I_LD|R, I_DEC, I_INC|R, I_DEC|R, I_LD|N, I_RRCA,
        I_DJNZ,  I_LD|NN, I_LD|R, I_INC, I_INC|R, I_DEC|R, I_LD|N, I_RLA,
        I_JR, I_ADD, I_LD|R, I_DEC, I_INC|R, I_DEC|R, I_LD|N, I_RRA,
        I_JR|C,  I_LD|NN, I_LD|NN_HL, I_INC, I_INC|R, I_DEC|R, I_LD|N, I_DAA,
        I_JR|C, I_ADD, I_LD|NN_HL, I_DEC, I_INC|R, I_DEC|R, I_LD|N, I_CPL,
        I_JR|C,  I_LD|NN, I_LD|NN_A, I_INC, I_INC|R, I_DEC|R, I_LD|N, I_SCF,
        I_JR|C, I_ADD, I_LD|NN_A, I_DEC, I_INC|R, I_DEC|R, I_LD|N, I_CCF
    } ;
    uint out = table[ op ] ;
    uint flags = out & 0xE0 ;
    out ^= flags ;
    switch ( out ) {
        case I_LD: {
            switch ( flags ) {
                case R: {
                    *args++ = A__PAIR_ ;
                    *args++ = ( op >> 4 ) ;
                    *args++ = A_REG ;
                    *args++ = 7 ;
                    if ( op & 0x08 ) {
                        *args++ = A_SWAP ;
                    }
                    break ;
                }
                case N: {
                    *args++ = A_REG ;
                    *args++ = ( op >> 3 ) & 7 ;
                    if ( xy && args[-1] == 6 ) {
                        *args++ = A_DISP ;
                        *args++ = *in++ ;
                    }
                    *args++ = A_BYTE ;
                    *args++ = *in++ ;
                    break ;
                }
                case NN: {
                    *args++ = A_PAIR ;
                    *args++ = ( op >> 4 ) ;
                    *args++ = A_WORD ;
                    *args++ = *in++ ;
                    *args++ = *in++ ;
                    break ;
                }
                case NN_HL: {
                    *args++ = A_PAIR ;  // First because of XY recognition done later.
                    *args++ = 2 ;
                    *args++ = A__WORD_ ;
                    *args++ = *in++ ;
                    *args++ = *in++ ;
                    if ( ( op & 0x08 ) == 0 ) {
                        *args++ = A_SWAP ;
                    }
                    break ;
                }
                case NN_A: {
                    *args++ = A_REG ;
                    *args++ = 7 ;
                    *args++ = A__WORD_ ;
                    *args++ = *in++ ;
                    *args++ = *in++ ;
                    if ( ( op & 0x08 ) == 0 ) {
                        *args++ = A_SWAP ;
                    }
                    break ;
                }
            }
            break ;
        }
        case I_INC:
        case I_DEC:
        {
            if ( flags ) {
                *args++ = A_REG ;
                *args++ = ( op >> 3 ) & 7 ;
                if ( xy && args[-1] == 6 ) {
                    *args++ = A_DISP ;
                    *args++ = *in++ ;
                }
            }
            else {
                *args++ = A_PAIR ;
                *args++ = ( op >> 4 ) ;
            }
            break ;
        }
        case I_ADD: {
            *args++ = A_PAIR ;
            *args++ = 2 ;
            *args++ = A_PAIR ;
            *args++ = ( op >> 4 ) ;
            break ;
        }
        case I_DJNZ:
        case I_JR:
        {
            if ( flags ) {
                *args++ = A_COND ;
                *args++ = ( op >> 3 ) & 3 ;
            }
            *args++ = A_REL ;
            *args++ = *in++ ;
            break ;
        }
        case I_EX: {
            *args++ = A_PAIR_AF ;
            *args++ = 1 ;
            *args++ = A_PAIR_AF ;
            *args++ = 0 ;
            break ;
        }
    }
    return out ;
}

// Disassemble the 0xC0-0xFF range.

static uint basic_c0ff_disassemble( const byte op, const byte * & in, uint * args, bool xy )
{
    enum {
        R = 1 << 6,
        C = R,
        AF = R,
        N = 2 << 6,
    } ;
    static const byte table[0x40] = {
        I_RET|C, I_POP, I_JP|C, I_JP|N, I_CALL|C, I_PUSH, I_ADD|N, I_RST,
        I_RET|C, I_RET, I_JP|C, I_NOP, I_CALL|C, I_CALL|N, I_ADC|N, I_RST,
        I_RET|C, I_POP, I_JP|C, I_OUT, I_CALL|C, I_PUSH, I_SUB|N, I_RST,
        I_RET|C, I_EXX, I_JP|C, I_IN, I_CALL|C, I_NOP, I_SBC|N, I_RST,
        I_RET|C, I_POP, I_JP|C, I_EX|R, I_CALL|C, I_PUSH, I_AND|N, I_RST,
        I_RET|C, I_JP, I_JP|C, I_EX, I_CALL|C, I_NOP, I_XOR|N, I_RST,
        I_RET|C, I_POP|AF, I_JP|C, I_DI, I_CALL|C, I_PUSH|AF, I_OR|N, I_RST,
        I_RET|C, I_LD, I_JP|C, I_EI, I_CALL|C, I_NOP, I_CP|N, I_RST,
    } ;
    uint out = table[ op & 0x3F ] ;
    uint flags = out & 0xC0 ;
    out ^= flags ;
    switch ( out ) {
        case I_RET: {
            if ( flags ) {
                *args++ = A_COND ;
                *args++ = ( op >> 3 ) ;
            }
            break ;
        }
        case I_JP:
        case I_CALL:
        {
            switch ( flags ) {
                case C: {
                    *args++ = A_COND ;
                    *args++ = ( op >> 3 ) ;
                    // fall ;
                }
                case N: {
                    *args++ = A_WORD ;
                    *args++ = *in++ ;
                    *args++ = *in++ ;
                    break ;
                }
                case 0: {
                    *args++ = A__PAIR_ ;
                    *args++ = 2 ;
                }
            }
            break ;
        }
        case I_POP:
        case I_PUSH:
        {
            *args++ = ( flags ? A_PAIR_AF : A_PAIR ) ;
            *args++ = ( op >> 4 ) ;
            break ;
        }
        case I_RST: {
            *args++ = A_BYTE ;
            *args++ = ( op - 199 ) ;
            break ;
        }
        case I_EX: {
            if ( flags ) {
                *args++ = A__PAIR_ ;
                *args++ = 3 ;
            }
            else {
                if ( xy ) {
                    break ;
                }
                *args++ = A_PAIR ;
                *args++ = 1 ;
            }
            *args++ = A_PAIR ;
            *args++ = 2 ;
            break ;
        }
        case I_IN:
        case I_OUT:
        {
            *args++ = A_REG ;
            *args++ = 7 ;
            *args++ = A__BYTE_ ;
            *args++ = *in++ ;
            if ( out == I_OUT ) {
                *args++ = A_SWAP ;
            }
            break ;
        }
        case I_LD: {
            *args++ = A_PAIR ;
            *args++ = 3 ;
            *args++ = A_PAIR ;
            *args++ = 2 ;
            break ;
        }
        default: {
            if ( flags ) {
                if ( out < I_AND ) {
                    *args++ = A_REG ;
                    *args++ = 7 ;
                }
                *args++ = A_BYTE ;
                *args++ = *in++ ;
            }
            break ;
        }
    }
    return out ;
}

// Disassemble the simple (non-prefixed) opcodes.

static uint basic_disassemble( const byte op, const byte * & in, uint * args, bool xy = false )
{
    uint out = I_LD ;

    switch ( op >> 6 ) {
        case 0 : {
            return basic_003f_disassemble( op, in, args, xy ) ;
        }
        case 1 : {
            if ( op == 0x76 ) {
                out = I_HALT ;
                break ;
            }
            *args++ = A_REG ;
            *args++ = ( op >> 3 ) & 7 ;
            *args++ = A_REG ;
            *args++ = op & 7 ;
            if ( xy && ( args[-1] == 6 || args[-3] == 6 ) ) {
                *args++ = A_DISP ;
                *args++ = *in++ ;
            }
            break ;
        }
        case 2 : {
            out = I_ADD + ( ( op >> 3 ) & 7 ) ;
            if ( out < I_AND ) {
                *args++ = A_REG ;
                *args++ = 7 ;
            }
            *args++ = A_REG ;
            *args++ = op & 7 ;
            if ( xy && args[-1] == 6 ) {
                *args++ = A_DISP ;
                *args++ = *in++ ;
            }
            break ;
        }
        case 3 : {
            return basic_c0ff_disassemble( op, in, args, xy ) ;
        }
    }

    return out ;
}

// Disassemble the opcodes with 0xCB prefix.

static uint cb_disassemble( const byte * & in, uint * args, bool xy = false )
{
    uint out ;
    const byte op = *in++ ;
    if ( op >= 0x40 ) {
        out = I_BIT - 1 + ( op >> 6 ) ;
        *args++ = A_BIT ;
        *args++ = ( op >> 3 ) ;
    }
    else {
        out = I_RLC + ( op >> 3 ) ;
    }
    if ( xy ) {
        *args++ = A__PAIR_ ;
        *args++ = 2 ;
        if ( out == I_BIT || ( op & 7 ) == 6 ) {
            return out ;
        }
    }
    *args++ = A_REG ;
    *args++ = op ;
    return out ;
}

// Disassemble the 0xED 0x40-0x7F range.

static uint ed_407f_disassemble( const byte op, const byte * & in, uint * args )
{
    enum {
        N = 1 << 7,
        IR = N,
    } ;
    static const byte table[0x40] = {
        I_IN, I_OUT, I_SBC, I_LD, I_NEG, I_RETN, I_IM, I_LD|IR,
        I_IN, I_OUT, I_ADC, I_LD, I_NEG, I_RETI, I_IM, I_LD|IR,
        I_IN, I_OUT, I_SBC, I_LD, I_NEG, I_RETN, I_IM, I_LD|IR,
        I_IN, I_OUT, I_ADC, I_LD, I_NEG, I_RETN, I_IM, I_LD|IR,
        I_IN, I_OUT, I_SBC, I_LD, I_NEG, I_RETN, I_IM, I_RRD,
        I_IN, I_OUT, I_ADC, I_LD, I_NEG, I_RETN, I_IM, I_RLD,
        I_IN|N, I_OUT|N, I_SBC, I_LD, I_NEG, I_RETN, I_IM, I_NOP,
        I_IN, I_OUT, I_ADC, I_LD, I_NEG, I_RETN, I_IM, I_NOP,
    } ;
    uint out = table[ op & 0x3f ] ;
    uint flags = out & 0x80 ;
    out ^= flags ;
    switch ( out ) {
        case I_IN:
        case I_OUT:
        {
            if ( flags ) {
                if ( out == I_OUT ) {
                    *args++ = A_BIT ;
                    *args++ = 0 ;
                }
            }
            else {
                *args++ = A_REG ;
                *args++ = ( op >> 3 ) ;
            }
            *args++ = A__REG_ ;
            *args++ = 1 ;
            if ( out == I_OUT ) {
                *args++ = A_SWAP ;
            }
            break ;
        }
        case I_LD: {
            if ( flags ) {
                *args++ = A_IR ;
                *args++ = ( op >> 3 ) ;
                *args++ = A_REG ;
                *args++ = 7 ;
                if ( op & 0x10 ) {
                    *args++ = A_SWAP ;
                }
                break ;
            }
            *args++ = A__WORD_ ;
            *args++ = *in++ ;
            *args++ = *in++ ;
            *args++ = A_PAIR ;
            *args++ = ( op >> 4 ) ;
            if ( op & 0x08 ) {
                *args++ = A_SWAP ;
            }
            break ;
        }
        case I_ADC:
        case I_SBC:
        {
            *args++ = A_PAIR ;
            *args++ = 2 ;
            *args++ = A_PAIR ;
            *args++ = ( op >> 4 ) ;
            break ;
        }
        case I_IM: {
            *args++ = A_BIT ;
            uint mode = ( op >> 3 ) & 3 ;
            *args++ = ( mode ? mode - 1 : mode ) ;
            break ;
        }
    }
    return out ;
}

// Disassemble the opcodes with 0xED prefix.

static uint ed_disassemble( const byte * & in, uint * args )
{
    uint out = I_NOP ;
    const byte op = *in++ ;
    if ( op >= 0x40 && op < 0x80 ) {
        out = ed_407f_disassemble( op, in, args ) ;
    }
    else if ( op >= 0xA0 && op < 0xC0 ) {
        if ( ( op & 7 ) < 4 ) {
            out = I_LDI + ( ( op & 0x18 ) >> 1 ) + ( op & 3 ) ;
        }
    }
    return out ;
}

// Disassemble the opcodes with 0xDD (IX) or 0xFD (IY) prefix.

static uint xy_disassemble( const byte prefix, const byte * & in, uint * args )
{
    const byte * const old_in = in ;
    uint * const old_args = args ;

    const byte op = *in++ ;

    // In case of 0xCB prefix, it is simple, everything is valid XY code with displacement.

    if ( op == 0xCB ) {
        *args++ = A_DISP ;
        *args++ = *in++ ;
        return cb_disassemble( in, args, true ) ;
    }

    // Otherwise, try normal disassembly (with optional displacement extraction),
    // then verify if it is really valid XY instruction.

    uint out = basic_disassemble( op, in, args, true ) ;

    for ( ; ; ) {
        switch ( *args++ ) {
            case A_PAIR:
            case A__PAIR_:
            {
                uint pair = *args++ & 3 ;
                if ( pair == 2 ) {
                    return out ;
                }
                continue ;
            }
            case A_REG: {
                uint reg = *args++ & 7 ;
                if ( reg >= 4 && reg <= 6 ) {
                    return out ;
                }
                continue ;
            }
        }
        break ;
    }

    // Nope, it's not valid, so revert everything back and output it as DB code.

    in = old_in ;
    args = old_args ;
    *args++ = A_BYTE ;
    *args++ = prefix ;
    *args++ = A_END ;
    return I_DB ;
}

// Disassemble the z80 opcodes.

static uint internal_disassemble( const byte * & in, uint * args )
{
    const byte op = *in++ ;
    switch ( op ) {
        case 0xDD: {
            *args++ = A_IX ;
            return xy_disassemble( op, in, args ) ;
        }
        case 0xFD: {
            *args++ = A_IY ;
            return xy_disassemble( op, in, args ) ;
        }
        case 0xCB: {
            return cb_disassemble( in, args ) ;
        }
        case 0xED: {
            return ed_disassemble( in, args ) ;
        }
        default: {
            return basic_disassemble( op, in, args ) ;
        }
    }
}

// Create argument string for given argument type.

static void output_argument( char * & buffer, uint type, uint value, const char * xy_set, sint disp, bool use_disp, bool hexadecimal )
{
    switch( type ) {
        case A_BIT: {
            *buffer++ = '0' + ( value & 7 ) ;
            break ;
        }
        case A_BYTE: {
            sprintf( buffer, hexadecimal ? "#%02X" : "%03u", value ) ;
            buffer += 3 ;
            break ;
        }
        case A_WORD: {
            sprintf( buffer, hexadecimal ? "#%04X" : "%05u", value ) ;
            buffer += 5 ;
            break ;
        }
        case A_REG: {
            value &= 7 ;
            *buffer++ = "BCDEHL.A" [ value ] ;
            if ( xy_set && ! use_disp && value >= 4 && value <= 5 ) {
                *buffer++ = *xy_set ;
            }
            break ;
        }
        case A_PAIR: {
            value &= 3 ;
            if ( xy_set && value == 2 ) {
                *buffer++ = 'I' ;
                *buffer++ = *xy_set ;
                break ;
            }
            *buffer++ = "BDHS" [ value ] ;
            *buffer++ = "CELP" [ value ] ;
            break ;
        }
        case A_PAIR_AF: {
            *buffer++ = 'A' ;
            *buffer++ = 'F' ;
            if ( value == 0 ) {
                *buffer++ = '\'' ;
            }
            break ;
        }
        case A_COND: {
            value &= 7 ;
            const char * out = "NZZ\0NCC\0POPEP\0M" + ( value << 1 ) ;
            *buffer++ = *out++ ;
            if ( *out ) {
                *buffer++ = *out ;
            }
            break ;
        }
        case A_IR: {
            *buffer++ = "IR" [ value & 1 ] ;
            break ;
        }
        case A__REG_:
        case A__PAIR_:
        case A__BYTE_:
        case A__WORD_:
        {
            *buffer++ = '(' ;
            output_argument( buffer, type - 1, value, xy_set, disp, use_disp, hexadecimal ) ;
            if ( use_disp ) {
                *buffer++ = ( disp < 0 ? '-' : '+' ) ;
                output_argument( buffer, A_BYTE, abs( disp ), xy_set, disp, use_disp, hexadecimal ) ;
            }
            *buffer++ = ')' ;
            break ;
        }
    }
}

// Create argument strings from argument info.

static void output_arguments( char * buffer, const uint * args, uint address, bool hexadecimal, uint * target_address )
{
    // Prepare the arguments.

    uint arg_types[ 3 ] ;
    uint arg_values[ 3 ] ;
    uint arg_count = 0 ;
    const char * xy_set = NULL ;
    sint disp = 0 ;
    bool use_disp = false ;

    while ( *args != A_END ) {
        uint arg_type = *args++ ;
        switch( arg_type ) {
            case A_IX: {
                xy_set = "X" ;
                break ;
            }
            case A_IY: {
                xy_set = "Y" ;
                break ;
            }
            case A_DISP: {
                disp = sbyte( *args++ ) ;
                use_disp = true ;
                break ;
            }
            case A_SWAP: {
                uint t = arg_types[ 0 ] ;
                arg_types[ 0 ] = arg_types[ 1 ] ;
                arg_types[ 1 ] = t ;
                t = arg_values[ 0 ] ;
                arg_values[ 0 ] = arg_values[ 1 ] ;
                arg_values[ 1 ] = t ;
                break ;
            }
            default: {
                uint arg_value = *args++ ;
                if ( arg_type == A_WORD || arg_type == A__WORD_ ) {
                    arg_value += *args++ << 8 ;
                }
                else if ( arg_type == A_REL ) {
                    arg_type = A_WORD ;
                    arg_value = word( address + 2 + sbyte( arg_value ) ) ;
                }
                else if ( arg_type == A_REG && ( arg_value & 7 ) == 6 ) {
                    arg_type = A__PAIR_ ;
                    arg_value = 2 ;
                }
                if ( target_address && ( arg_type == A_WORD || arg_type == A__WORD_ ) ) {
                    *target_address = arg_value ;
                }
                arg_types[ arg_count ] = arg_type ;
                arg_values[ arg_count ] = arg_value ;
                arg_count++ ;
                break ;
            }
        }
    }

    // Now print them out to the output buffer.

    for ( uint i = 0 ; i < arg_count ; i++ ) {
        if ( i > 0 ) {
            *buffer++ = ',' ;
        }
        output_argument( buffer, arg_types[ i ], arg_values[ i ], xy_set, disp, use_disp, hexadecimal ) ;
    }

    *buffer = 0 ;
}

// Disassemble the Z80 op code to instruction name and argument strings.

const char * z80_disassemble( const byte * & in, char * args, uint address, bool hexadecimal, uint * target_address )
{
    uint internal_args[ 16 ] = { 0 } ;
    uint out = internal_disassemble( in, internal_args ) ;
    output_arguments( args, internal_args, address, hexadecimal, target_address ) ;
    return instruction_names[ out ] ;
}

// Measure the size of the Z80 op code.

uint z80_disassemble_size( const byte * in )
{
    uint internal_args[ 16 ] = { 0 } ;
    const byte * out = in ;
    internal_disassemble( out, internal_args ) ;
    return uint( out - in ) ;
}

#if 0

// Test the disassembler, dumping complete op code set disassembly to given file.

static void z80_disassemble_test( FILE * file )
{

    byte buffer[ 16 ] = { 0x00, 0x11, 0x22, 0x33 } ;
    char args[ 64 ] ;
    
#define TEST \
    const byte * in = buffer ; \
    fprintf( file, "%02x%02x%02x%02x %u %s %s\n", buffer[0], buffer[1], buffer[2], buffer[3], z80_disassemble_size( buffer ), z80_disassemble( in, args, 0, true ), args ) ; \
    if ( ( i & 7 ) == 7 ) fputc( '\n', file )

    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 0 ] = i ;
        TEST ;
    }

    buffer[ 0 ] = 0xCB ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 1 ] = i ;
        TEST ;
    }
    buffer[ 0 ] = 0xED ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 1 ] = i ;
        TEST ;
    }

    buffer[ 0 ] = 0xDD ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 1 ] = i ;
        TEST ;
    }
    buffer[ 1 ] = 0xCB ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 3 ] = i ;
        TEST ;
    }

    buffer[ 0 ] = 0xFD ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 1 ] = i ;
        TEST ;
    }
    buffer[ 1 ] = 0xCB ;
    for ( uint i = 0 ; i < 256 ; i++ ) {
        buffer[ 3 ] = i ;
        TEST ;
    }
    
    fflush( file ) ;
}

// Run the test.

int main( int, char * [] )
{
    z80_disassemble_test( stdout ) ;
    return EXIT_SUCCESS ;
}

#endif

// EOF // 
