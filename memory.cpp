
#include "memory.h"
#include "poly88_devices.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <ctype.h>

#define MEM_SIZE 65536
#define MAX_STRING 1024

extern int debug;

void Memory::LoadROM(const char *name)
{
    FILE *stream;
    int addr;
    char buf[1024];
    int new_addr,v2,v3, v1;
    int item_cnt;

    stream = fopen(name,"r");
    if(stream==NULL)
    {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, std::string("failed to open ROM file ") + name);
    }
    addr = 0;
    while(fgets(buf, MAX_STRING, stream))
    {
        if(isxdigit(buf[0]))
        {
            item_cnt = sscanf(buf,"%x %x %x %x",&new_addr,&v1, &v2, &v3);
            item_cnt--;
            if(new_addr!=addr)
            {
                fprintf(stderr,"warning: lost sync at address %04x (file says %04x)\n",
                        addr, new_addr);
            }
        }
        else item_cnt = sscanf(buf,"%x %x %x",&v1, &v2, &v3);

        switch(item_cnt)
        {
            case 1:
                rom[addr++] = v1;
                break;
            case 2:
                rom[addr++] = v1;
                rom[addr++] = v2;
                break;
            case 3:
                rom[addr++] = v1;
                rom[addr++] = v2;
                rom[addr++] = v3;
                break;
            default:
                fprintf(stderr,"eeeeewwww icky!!! you got the wrong # of bytes at addr %04x.\n",
                        addr);
        }

    }
    if(addr>=MEM_SIZE)
    {
		throw std::runtime_error("ROM image larger than addressable memory!");
    }
    rom_end = addr+1;
    fclose(stream);
    return;
}

void Memory::LoadRAM(const char *name)
{
	FILE *stream;

	stream = fopen(name,"r");
	i8080_addr_t address = 0x2000;
	int ch;
	while((ch = fgetc(stream)) != EOF)
	{
		ram[address++] = ch;
	}
}

Memory::Memory()
{
	auto dimensions = Cvdi_font::GetDimensions();
    screen.set_size(64 * dimensions.first, 16 * dimensions.second);

    if(screen.open())
    {
        screen.close();
		throw std::runtime_error("failed to open SDL screen.");
    }

    vdi_font = std::unique_ptr<Cvdi_font>(new Cvdi_font(screen.renderer()));

    debug = FALSE;
    if(::debug) std::cerr<<"starting up memory...\n";

}

Memory::~Memory()
{
    std::cerr << "shutting down memory." << std::endl;
}

// this method needs to check address range against ROM settings
byte_t& Memory::operator[](i8080_addr_t i)
{
    if(debug) std::cout << "reading byte at address " << i << "\n";
    return ram[i];
}

void Memory::set_byte(i8080_addr_t a, byte_t v)
{
    if(a<rom_end) return;
	if(a >= guardLow && a<guardHigh) return;

    ram[a] = v;
    if(a>=0xf800)
    {
        int row = (a >> 6) & 0xf;
        int col = a & 0x3f;

        SDL_Rect destination;
        destination.x = col * vdi_font->width();
        destination.y = row * vdi_font->height();
        destination.w = vdi_font->width();
        destination.h = vdi_font->height();
        screen.blit2screen((*vdi_font)[v], destination);
    }
}

void Memory::set_2byte(i8080_addr_t a, i8080_addr_t v)
{
    if(a<rom_end) return;
	if(a >= guardLow && a < guardHigh) return;
    ram[a] = (v&0xff);
    ram[a+1] = ((v>>8)&0xff);
    if(a>=0xf800)
    {
        set_byte(a, ram[a]);
        set_byte(a+1, ram[a+1]);
    }
}

byte_t Memory::get_byte(i8080_addr_t a) const
{
    if(a<rom_end) return rom[a];
	if(a >= guardLow && a < guardHigh) return 0xff;
    return ram[a];
}

i8080_addr_t Memory::get_2byte(i8080_addr_t a) const
{
    if(a<rom_end) return (rom[a+1]<<8) + rom[a];
	if(a >= guardLow && a < guardHigh) return 0xffff;
    return (ram[a+1]<<8) + ram[a];
}

void Memory::redraw_screen()
{
    return;
}
