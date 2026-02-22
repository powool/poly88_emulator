#pragma once

#include <queue>
#include "MemoryInterface.h"
#include "DeviceInterface.h"

class KeyBoard : public DeviceInterface {
	std::queue<uint8_t>	keys;
	uint8_t lastKey;
    public:
	KeyBoard();
	void Seek(uint32_t offset) override { ; }
	bool DataAvailable(uint16_t size) override;
	uint8_t ReadByte() override;
	void WriteByte(uint8_t byte) override;
	int ReadBlock(MemoryInterface &memory, uint16_t addr, uint16_t size) override { return 0; }
	void WriteBlock(MemoryInterface &memory, uint16_t addr, uint16_t size) override { ; }
	void RequestMedia() override { ; }
};
