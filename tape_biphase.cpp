#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "audio.h"
#include "tape_header.h"

class ChecksumError : std::runtime_error {
public:
	ChecksumError(const char *s) : std::runtime_error(s) {;}
};

class PolyAudioTapeDecoder {
	// Hysterisis out of +/- 32767 - pdf says we want +/- 4mv hysterisis
	// According to https://en.wikipedia.org/wiki/Line_level, 0dB for
	// line level input is 1.095v.
//	const int hysterisis = .004 * (32767/1.095);
	const int hysterisis = 200;

	int samplesPerBit;

	bool debug;
	int bitCellStartIndex;
	int lastBit;

	// for debug dump of the captured byte:
	int debugByteStartIndex;
	int debugByteEndIndex;

	int bitRate;	// bit per second

	bool bitSync;
	bool byteSync;
	bool recordSync;
	Audio &audio;

public:
	PolyAudioTapeDecoder(Audio &_audio) : audio(_audio) {
		debug = false;
		bitCellStartIndex = 0;
		bitSync = false;
		SetBitRate(2400);
		lastBit = 0;
	}

	void SetDebug(int debug) { this->debug = debug; }
	void SetIndex(int bitCellStartIndex) { this->bitCellStartIndex = bitCellStartIndex; }
	void SetBitRate(int bitRate) { this->bitRate = bitRate; samplesPerBit = audio.SamplesPerBit(bitRate);}

	// See http://www.kazojc.com/elementy_czynne/IC/8T20.pdf
	//
	// On entry, we can be 100% in sync, but not pointing to a signal transition,
	// this is due to the fact that the signal can be out of phase with the synthetic clock.
	// The only time we know the next transition is exactly on a synthetic clock edge
	// is on the 1->0 transition.
	int ReadBit() {
		int oneShotTriggerIndex = bitCellStartIndex + .75 * samplesPerBit;

		int resultingBit = audio.Value(oneShotTriggerIndex) > 0;

		// see if we can re-sync exactly
		if (lastBit == 1 && resultingBit == 0) {
			// Closed loop:
			//
			// Here, due to the encoding, we guarantee that the following transition will
			// be the beginning of a bit cell. Find it and reset our cell index to that transition.
			bitCellStartIndex = audio.FindThisOrNextTransition(oneShotTriggerIndex, hysterisis);
		} else {
			// open loop clocking
			bitCellStartIndex += samplesPerBit;
		}

		lastBit = resultingBit;
		return resultingBit;
	}

	uint8_t ReadByte() {

		debugByteStartIndex = bitCellStartIndex;

		// There is exactly one start bit for the entire record,
		// not per character.
		if(!byteSync) {
			// search for a start bit
			while (ReadBit() != 1) {
				byteSync = false;
			}
		}
		byteSync = true;

		uint8_t resultByte = 0;
		for(auto i = 0 ; i < 8; i++) {
			auto dataBit = ReadBit();
			if (dataBit) {
				resultByte |= (1 << i);
			}
		}

		debugByteEndIndex = bitCellStartIndex;

		return resultByte;
	}

	void ReadRecord() {
		auto savedIndex = bitCellStartIndex;
		uint8_t byte;

		byteSync = false;

		while((byte = ReadByte()) != TapeHeader::SYNC) {
			// go back to the start bit
			bitCellStartIndex = savedIndex + samplesPerBit/4;

			// keep track of new possible start bit
			savedIndex = bitCellStartIndex;

			continue;
		}
		if(debug) std::cout << "yaay! got an E6!!" << std::endl;
		while(byte == TapeHeader::SYNC) {
			if (debug) {
				audio.Dump(std::cout, debugByteStartIndex, samplesPerBit * 9);
				std::cout << savedIndex << "/" << (bitCellStartIndex - savedIndex) << ", " << audio.TimeOffset(savedIndex) << "s: " << std::hex << static_cast<uint16_t>(byte) << std::dec << std::endl;
			}
			else std::cout << (byte);
			byte = ReadByte();
		}

		if(byte != TapeHeader::SOH) {
			std::cerr << bitCellStartIndex << ", " << audio.TimeOffset(bitCellStartIndex) << "s: " << std::hex << static_cast<uint16_t>(byte) << std::dec << " expected SOH = 0x01" << std::endl;
			return;
		}

		std::vector<uint8_t> headerBytes;

		headerBytes.resize(sizeof(TapeHeader), 0);

		for(auto i=0; i<sizeof(TapeHeader); i++) {
			headerBytes[i] = ReadByte();
		}

		TapeHeader *header = reinterpret_cast<TapeHeader *> (&headerBytes[0]);
		if (debug) header->Dump();

		if(!debug) {
			for(auto i=0; i<sizeof(TapeHeader); i++) {
				std::cout << headerBytes[i];
			}
		}

		uint8_t		runningChecksum = 0;
		uint16_t	dataLength = header->dataLength;
		// size == 0 actually means 256 bytes
		if(dataLength == 0) dataLength = 256;
		for(auto i=0; i<dataLength; i++) {
			auto byte = ReadByte();
			if (!debug) std::cout << byte;
			runningChecksum += byte;
		}
		// last byte after data is the trailing checksum
		runningChecksum += ReadByte();
		if(runningChecksum != 0) {
			std::cerr << "got bad checksum..." << std::endl;
		}
	}

	void ReadTape() {
		while(bitCellStartIndex < audio.SampleCount()) {
			ReadRecord();
		}
	}
};

void usage(int argc, char **argv) {
	std::cerr << "usage: " << argv[0] << " [options] 16 bit RIFF WAV file name" << std::endl;
	std::cerr << "where options are:" << std::endl;
	std::cerr << "	-d -> enable debug output" << std::endl;
	std::cerr << "	-p -> invert signal (usually for polyphase tapes)" << std::endl;
}

int main(int argc, char **argv) {
	int opt;
	int debug = false;
	bool invertPhase = false;
	int bitRate = 0;

	while ((opt = getopt(argc, argv, "b:dp")) != -1) {
		switch(opt) {
			case 'b':
				bitRate = atoi(optarg);
				break;
			case 'd':
				debug = true;
				break;
			case 'p':
				invertPhase = true;
				break;
			default:
				usage(argc, argv);
				exit(1);
		}
	}
	if(optind >= argc) {
		usage(argc, argv);
		exit(1);
	}

	Audio audio(argv[optind]);
	PolyAudioTapeDecoder decoder(audio);
	decoder.SetDebug(debug);
	decoder.SetIndex(1605616);
	if(bitRate) decoder.SetBitRate(bitRate);

	audio.SetInvertPhase(invertPhase);

	try {
		while(true) {
			decoder.ReadTape();
		}
	} catch (AudioEOF &e) {
		std::cerr << "Reached EOF!" << std::endl;
	} catch (ChecksumError &e) {
		std::cerr << "got bad checksum!" << std::endl;
	}
}
