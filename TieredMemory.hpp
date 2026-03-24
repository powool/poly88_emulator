#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "MemoryInterface.h"

class TieredMemory : public MemoryInterface {

	struct Storage {
		std::vector<uint8_t> data;
		uint16_t address = 0;

		Storage(uint16_t address, uint16_t size) : address(address) {
			data.resize(size);
		}

		Storage(std::string filename);

		uint8_t &GetDataAddress(uint16_t address) {
			return data[address - this->address];
		}
	};

	std::array<uint8_t *, 65536> readMemoryPtrs;
	std::array<uint8_t *, 65536> writeMemoryPtrs;
	std::vector<std::shared_ptr<Storage>> storagePtrs;

	void Insert(std::shared_ptr<Storage> storage, bool isRAM = false) {
		for (uint16_t address = storage->address; address < storage->address + storage->data.size(); address ++) {
			readMemoryPtrs[address] = &(storage->GetDataAddress(address));
			if (isRAM) {
				writeMemoryPtrs[address] = &(storage->GetDataAddress(address));
			}
		}
		storagePtrs.push_back(storage);
	}

	TieredMemory() {
		// Insert RAM storage, which ensures that ever read and
		// write location has a valid memory pointer.
		Insert(std::make_shared<Storage>(0, 65536), true);
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
