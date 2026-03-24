#pragma once
#include <cstdint>
#include "MemoryInterface.h"

class DeviceInterface {
    public:
	virtual void Seek(uint32_t offset) = 0;
	virtual bool DataAvailable(uint16_t size) = 0;
	virtual uint8_t ReadByte() = 0;
	virtual void WriteByte(uint8_t byte) = 0;
	virtual int ReadBlock(MemoryInterface &memory, uint16_t addr, uint16_t size) = 0;
	virtual void WriteBlock(MemoryInterface &memory, uint16_t addr, uint16_t size) = 0;
	virtual void RequestMedia() = 0;
};

