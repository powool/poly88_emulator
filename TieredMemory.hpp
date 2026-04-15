#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "MemoryInterface.h"

class Storage {
    public:
	std::vector<uint8_t> data;
	bool addressIsValid = false;
	bool readWrite = false;
	uint16_t address = 0;

	Storage(uint16_t address, uint16_t size, bool readWrite) :
		address(address),
		readWrite(readWrite)
	{
		if (size == 0) {
			// 65536 is also 0 in uint16_t
			data.resize(0x10000);
		} else {
			data.resize(size);
		}
	}

	Storage(const std::string &filename, bool readWrite);
	Storage(const char *intexHex[], bool readWrite);
	// Some intel hex files will use 0 based offsets,
	// in which case we need to set the destination address.
	void SetAddress(uint16_t address) { this->address = address; }

	uint8_t &GetDataAddress(uint16_t address) {
		return data[address - this->address];
	}
};

class TieredMemory : public MemoryInterface {

	std::array<uint8_t *, 65536> readMemoryPtrs;
	std::array<uint8_t *, 65536> writeMemoryPtrs;
	std::vector<std::shared_ptr<Storage>> storagePtrs;
    public:
	void Insert(std::shared_ptr<Storage> storage) {
		uint16_t address;
		for (address = storage->address; address < storage->address + storage->data.size() - 1; address ++) {
			readMemoryPtrs[address] = &(storage->GetDataAddress(address));
			if (storage->readWrite) {
				writeMemoryPtrs[address] = &(storage->GetDataAddress(address));
			}
		}
		readMemoryPtrs[address] = &(storage->GetDataAddress(address));
		if (storage->readWrite) {
			writeMemoryPtrs[address] = &(storage->GetDataAddress(address));
		}
		storagePtrs.push_back(storage);
	}

	TieredMemory() {
		// Insert RAM storage, which ensures that every read and
		// write location has a valid memory pointer.
		Insert(std::make_shared<Storage>(0, 65536, true));
	}

	uint8_t ReadByte(uint16_t address) const {
		auto byteAddress = readMemoryPtrs[address];
		return *byteAddress;
	}

	void WriteByte(uint16_t address, uint8_t data) {
		// This will fall back to writing to the RAM
		// if you attempt to write to a ROM, but it
		// won't alter the ROM read data pointers.
		auto byteAddress = writeMemoryPtrs[address];
		*byteAddress = data;
	}
};
using TieredMemoryPtr = std::shared_ptr<TieredMemory>;
