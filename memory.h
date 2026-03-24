#pragma once

#include "memory"
#include "i8080_types.h"
#include "sdl.h"
#include "vdi_font.h"


class Memory
{

private:
    byte_t m_ram[65536];
    byte_t m_rom[65536];
    i8080_addr_t    rom_end;    /* start of rom is location 0 */
    bool m_debug;

    std::unique_ptr<Cvdi_font> vdi_font;

	i8080_addr_t	m_guardLow = 0xe000;
	i8080_addr_t	m_guardHigh = 0xf000;

public:
    Memory();
    ~Memory();

    Csdl screen;

    bool Debug() const {return m_debug;}
    bool Debug(bool b) {m_debug = b; return m_debug;}

    void LoadROM(const char *fileName);
    void LoadRAM(const char *fileName);
    byte_t& operator[](i8080_addr_t);
    byte_t& elem(i8080_addr_t i)
    {
        return m_ram[i];
    }
    void set_byte(i8080_addr_t, byte_t);
    void set_2byte(i8080_addr_t, i8080_addr_t);
    byte_t get_byte(i8080_addr_t) const;
    i8080_addr_t get_2byte(i8080_addr_t) const;
    void redraw_screen();
};
