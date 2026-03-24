#pragma once

#include <memory>
#include "i8080_types.h"


class Memory
{

private:
    byte_t ram[65536];
    byte_t rom[65536];
    i8080_addr_t    rom_end;    /* start of rom is location 0 */
    bool debug;

	i8080_addr_t	guardLow = 0xe000;
	i8080_addr_t	guardHigh = 0xf000;

public:
    Memory();
    ~Memory();

    bool Debug() const {return debug;}
    bool Debug(bool b) {debug = b; return debug;}

    void LoadROM(const char *fileName);
    void LoadRAM(const char *fileName);
    byte_t& operator[](i8080_addr_t);
    byte_t& elem(i8080_addr_t i)
    {
        return ram[i];
    }
    void set_byte(i8080_addr_t, byte_t);
    void set_2byte(i8080_addr_t, i8080_addr_t);
    byte_t get_byte(i8080_addr_t) const;
    i8080_addr_t get_2byte(i8080_addr_t) const;
};
