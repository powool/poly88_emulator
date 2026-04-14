#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "IntelHex.hpp"

// --- Helpers (file-local) ---------------------------------------------------

static uint8_t hexNibble(char c) {
	if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
	if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
	if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
	throw IntelHexException("Invalid hex character");
}

static uint8_t parseByte(const std::string &line, size_t pos) {
	if (pos + 1 >= line.size())
		throw IntelHexException("Unexpected end of record");
	return static_cast<uint8_t>((hexNibble(line[pos]) << 4) | hexNibble(line[pos + 1]));
}

// --- Constructors from raw data (RAII – data is owned immediately) ----------

IntelHex::IntelHex(uint16_t address, const uint8_t *buf, uint16_t size)
	: address(address), data(buf, buf + size)
{
}

IntelHex::IntelHex(uint16_t address, const std::vector<uint8_t> &src)
	: address(address), data(src)
{
}

void IntelHex::ReadLine(const std::string &readLine, bool &gotData)
{
	std::string line = readLine;
	// Find the start code ':'
	auto colon = line.find(':');
	if (colon == std::string::npos) return;
	line = line.substr(colon + 1);

	// Strip trailing whitespace / CR
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'
			|| line.back() == ' '))
		line.pop_back();

	if (line.size() < 10)
		throw IntelHexException("Record too short");

	uint8_t byteCount = parseByte(line, 0);
	uint16_t addr     = static_cast<uint16_t>((parseByte(line, 2) << 8) | parseByte(line, 4));
	uint8_t recType   = parseByte(line, 6);

	// Verify checksum: sum of all decoded bytes (including checksum) == 0 (mod 256)
	size_t expectedLen = 8 + byteCount * 2 + 2;
	if (line.size() < expectedLen)
		throw IntelHexException(
			std::format("Record truncated: expected {} hex chars, got {}",
				expectedLen, line.size()).c_str());

	uint8_t sum = 0;
	for (size_t i = 0; i < expectedLen; i += 2)
		sum += parseByte(line, i);
	if (sum != 0)
		throw IntelHexException(
			std::format("Checksum error at address {:04X}", addr).c_str());

	switch (recType) {
	case 0x00: // Data
		if (!gotData) {
			address = addr;
			gotData = true;
		}
		for (uint8_t i = 0; i < byteCount; i++)
			data.push_back(parseByte(line, 8 + i * 2));
		break;
	case 0x01: // End Of File
		break;
	case 0x02:
		throw IntelHexException("Extended Segment Address (type 02) not supported");
	case 0x03:
		throw IntelHexException("Start Segment Address (type 03) not supported");
	case 0x04:
		throw IntelHexException("Extended Linear Address (type 04) not supported");
	case 0x05:
		throw IntelHexException("Start Linear Address (type 05) not supported");
	default:
		throw IntelHexException(
			std::format("Unknown record type {:02X}", recType).c_str());
	}
}

IntelHex::IntelHex(const std::string &fileName) : address(0)
{
	std::ifstream ifs(fileName);
	if (!ifs)
		throw IntelHexException(
			std::format("Cannot open file: {}", fileName).c_str());

	bool gotData = false;
	std::string line;

	while (std::getline(ifs, line)) {
		ReadLine(line, gotData);
	}

	if (!gotData)
		throw IntelHexException("No data records found");
}

IntelHex::IntelHex(const char *intelHex[]) : address(0)
{
	bool gotData = false;

	for (int i = 0; intelHex[i]; i++) {
		ReadLine(intelHex[i], gotData);
	}

	if (!gotData)
		throw IntelHexException("No data records found");
}

// --- Save as Intel HEX ------------------------------------------------------

void IntelHex::Save(const std::string &fileName, bool appendEndRecord) {
	std::ofstream ofs(fileName);
	if (!ofs)
		throw IntelHexException(
			std::format("Cannot create file: {}", fileName).c_str());

	const uint8_t bytesPerLine = 16;
	size_t offset = 0;

	while (offset < data.size()) {
		uint8_t count = static_cast<uint8_t>(
			std::min<size_t>(bytesPerLine, data.size() - offset));
		uint16_t addr = static_cast<uint16_t>(address + offset);

		// Build record: byteCount, addrH, addrL, type(00), data...
		uint8_t sum = count;
		sum += static_cast<uint8_t>(addr >> 8);
		sum += static_cast<uint8_t>(addr & 0xFF);
		sum += 0x00; // record type

		ofs << std::format(":{:02X}{:04X}00", count, addr);
		for (uint8_t i = 0; i < count; i++) {
			ofs << std::format("{:02X}", data[offset + i]);
			sum += data[offset + i];
		}
		uint8_t checksum = static_cast<uint8_t>(-sum);
		ofs << std::format("{:02X}\n", checksum);

		offset += count;
	}

	if (appendEndRecord) {
		// :00000001FF
		ofs << ":00000001FF\n";
	}

	if (!ofs)
		throw IntelHexException("Write error");
}

// --- Value accessor ---------------------------------------------------------

uint8_t IntelHex::Value(uint16_t offset) const {
	if (offset >= data.size())
		throw IntelHexException(
			std::format("Offset {:04X} out of range (size {:04X})",
				offset, data.size()).c_str());
	return data[offset];
}
