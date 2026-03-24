#include "TieredMemory.hpp"

TieredMemory::Storage::Storage(std::string filename)
{
    FILE *stream;
    int addr;
    char buf[1024];
    int new_addr,v2,v3, v1;
    int item_cnt;

    stream = fopen(filename.c_str(),"r");
    if(stream==NULL)
    {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, std::string("failed to open ROM file ") + filename);
    }
    addr = 0;
    while(fgets(buf, sizeof(buf), stream))
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
                data.push_back(v1);
		addr += 1;
                break;
            case 2:
                data.push_back(v1);
                data.push_back(v2);
		addr += 2;
                break;
            case 3:
                data.push_back(v1);
                data.push_back(v2);
                data.push_back(v3);
		addr += 3;
                break;
            default:
                fprintf(stderr,"eeeeewwww icky!!! you got the wrong # of bytes at addr %04x.\n",
                        addr);
        }

    }
    if(data.size()>=65536)
    {
		throw std::runtime_error("ROM image larger than addressable memory!");
    }
    fclose(stream);
    return;
}

#if 0
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
#endif
