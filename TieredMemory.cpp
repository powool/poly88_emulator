#include "IntelHex.hpp"
#include "TieredMemory.hpp"

// load an image in Intel Hex format
Storage::Storage(const std::string &filename, bool readWrite) :
	readWrite(readWrite)
{
	// this can throw IntelHexException
	IntelHex hex(filename);
	data = hex.Data();
	address = hex.Address();
}

// Create an image from internally stored Intel Hex
Storage::Storage(const char *intelHex[], bool readWrite) :
	readWrite(readWrite)
{
	IntelHex hex(intelHex);
	data = hex.Data();
	address = hex.Address();
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
