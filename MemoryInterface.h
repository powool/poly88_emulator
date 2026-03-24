#pragma once
#include <cstdint>

class MemoryInterface {
    public:
	virtual uint8_t ReadByte(uint16_t offset) const = 0;
	virtual void WriteByte(uint16_t offset, uint8_t data) = 0;
};
