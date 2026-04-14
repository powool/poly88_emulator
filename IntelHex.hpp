#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class IntelHexException : public std::runtime_error {
    public:
	IntelHexException(const char *s) : std::runtime_error(s) {;}
};

class IntelHex {
	std::vector<uint8_t> data;
	uint16_t	address;
    public:

	IntelHex(uint16_t address, const uint8_t *data, uint16_t size);
	IntelHex(uint16_t address, const std::vector<uint8_t> &data);

	// throw with explanation of failure
	void ReadLine(const std::string &line, bool &gotData);
	IntelHex(const char *intelHex[]);
	IntelHex(const std::string &fileName);
	void Save(const std::string &fileName, bool appendEndRecord = false);
	const std::vector<uint8_t> &Data() const { return data; }
	uint16_t Address() { return address; }
	uint8_t Value(uint16_t offset = 0) const;
	uint8_t operator [](uint16_t index) const { return data[index]; }
};
