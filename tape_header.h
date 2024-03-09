#pragma once

#pragma pack(push, 1)

class TapeHeader {
public:
	static const uint8_t	SYNC = 0xE6;
	static const uint8_t	SOH = 1;
	char 		name[8];
	uint16_t    recordNumber;
	uint8_t 	dataLength; // bytes of data after type byte
	uint16_t    recordAddress;
	uint8_t 	type;   	// 0 == data  01 == comment  02 == end of file  03 == autoexecute
	uint8_t 	checksum;   // when data is all added including checksum, we should get 0
	uint8_t 	data[0];

	uint8_t ComputeChecksum() {
		uint8_t computedChecksum = 0;

		for (auto i = 0 ; i < sizeof(name); i++) computedChecksum += name[i];
		computedChecksum += recordNumber >> 8;
		computedChecksum += recordNumber & 0xff;
		computedChecksum += dataLength;
		computedChecksum += recordAddress >> 8;
		computedChecksum += recordAddress & 0xff;
		computedChecksum += type;
		computedChecksum += checksum;
		return computedChecksum;
	}
	void Dump() {
		std::cerr << "name: ";
		for(auto i = 0; i < sizeof(name) ; i++) {
			std::cout << name[i];
		}
		std::cerr << " record #" << recordNumber;
		std::cerr << " record length: " << static_cast<uint16_t>(dataLength);
		std::cerr << " record address: " << std::hex << recordAddress << std::dec;
		std::cerr << " record type: " << std::hex << static_cast<uint16_t>(type) << std::dec;
		std::cerr << " computed checksum: " << static_cast<uint16_t> (ComputeChecksum());
		std::cerr << std::endl;
	}
};

#pragma pack(pop)
