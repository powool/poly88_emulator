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

class PolyAudioTapeDecoder {
	bool debug;
	const int bitRate = 300;	// bit per second
	int syncedIndex;
	Audio &audio;

	// Decode 300 baud byte format data, which is a two tone encoding (AKA
	// frequency shift key - FSK), where 1200HZ represents a 0, and 2400HZ
	// represents a 1.
	//
	// To my knowledge, 300 bits per second is the only speed byte encoded tape I have.
	std::pair<int, int> DecodeByteEncodedBit(int index) {
		int samplesPerBit = audio.SamplesPerBit(bitRate);
		int initialIndex = index;

		// samples per second / cycles per second / 2 => samples per half wave cycle:
		int halfWave2400HZSampleCount = audio.SampleRate() / 2400 / 2;

		// On transitions from 0 (1200HZ) to 1 (2400HZ), there seems to
		// be significant skew in the 2400HZ waveform, causing the first cycle
		// of the following 1 to be included in the current 0.
		// NB: does this heuristic introduce problems elsewhere?
		auto lastIndex = index + samplesPerBit - halfWave2400HZSampleCount;

		int fullWaveCount = 0;
		int indexOfLastPeak = std::numeric_limits<int>::max();

#if 0
		std::cout << index << ", " << audio.TimeOffset(index) << "s: start reading bit" << std::endl;
#endif

		while (index < lastIndex) {
			// count peaks - but don't use adjacent samples, as they can be noisy:
			if (audio.IsAPeak(index)) {
				if (abs(indexOfLastPeak - index) > 10) {
					fullWaveCount++;
					indexOfLastPeak = index;
				}
			}
			index++;
		}

		// Recode full wave count found to 0 or 1 bit.
		// Getting edge cases exactly right is hard in the face
		// of signal noise, so fudge on peak counting.
		int resultBit;
		switch(fullWaveCount) {
			case 3:
			case 4:
			case 5:
				resultBit = 0;
				break;
			case 7:
			case 8:
			case 9:
				resultBit = 1;
				break;
			default:
				resultBit = 2;
#if 0
				std::cout << initialIndex << ", " << audio.TimeOffset(initialIndex) << "s: fullWaveCount = " << fullWaveCount << " lost sync" << std::endl;
#endif
				break;
		}

		return std::make_pair(resultBit, audio.FindThisOrNextZeroCrossing(indexOfLastPeak));
	}

public:
	PolyAudioTapeDecoder(Audio &_audio) : audio(_audio) {
		debug = false;
		syncedIndex = 0;
	}

	void SetDebug(int debug) { this->debug = debug; }

	// return the index of the next valid bit, throw on out of data
	int SyncToValidBit(int index) {
		while(index < audio.SampleCount()) {
			index = audio.FindThisOrNextZeroCrossing(index);

			auto bit = DecodeByteEncodedBit(index);
			if (bit.first == 0 || bit.first == 1) {
				return index;
			}

			// skip to next sample
			index++;
		}
		throw AudioEOF("ran out of data");
	}

	// A byte on tape is encoded as a single start bit (value=0),
	// followed by 8 data bits, then ending with a pair of stop
	// bits (also value 0).
	// Any time we fall out of sync (e.g. BitRead returns a value
	// of 2), we need to re-sync appropriately.
	// The return pair looks like this:
	//   resulting data byte (guaranteed to be valid)
	//   synchronized index of waveform immediately following last stop bit
	//
	// Synchronization is a bit sticky. On the poly-88, the processor
	// loops, reading a byte - if it sees an 0xe6, it is done, otherwise,
	// it resets the uart to start at a new bit offset in the stream,
	// and repeats until it gets an 0xe6.
	//
	// This byte reading code is working a little differently, choosing
	// to reset when we don't get the right stop bits. I'm not sure this
	// is a very good approach yet.
	//
	// If we run out of data to return, we throw an exception
	std::pair<int, int> ByteReadUnsynced(int index) {
		int samplesPerBit = audio.SamplesPerBit(bitRate);
		uint8_t resultByte = 0;

		std::pair<int, int> startBit;

		startBit = DecodeByteEncodedBit(index);

		// if not a 0 (start) bit, let caller know
		if (startBit.first != 0) {
			return std::make_pair(256, 0);
		}

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(startBit.first) << std::dec << " start bit" << std::endl;

		index = startBit.second;

		int bitIndex;
		// in theory, we have a stop bit, now get 8 data bits
		for(bitIndex = 0; bitIndex < 8; bitIndex++) {
			auto dataBit = DecodeByteEncodedBit(index);

			if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(dataBit.first) << std::dec << " data bit #" << bitIndex << std::endl;

			if(dataBit.first == 1) {
				resultByte |= 1 << bitIndex;
			}

			if(dataBit.first == 2) {
				return std::make_pair(256, index);
			}

			index = dataBit.second;
		}

		// now check for two stop bits

		for(auto i = 0; i < 2; i++) {
			auto firstStopBit = DecodeByteEncodedBit(index);

			if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(firstStopBit.first) << std::dec << " first stop bit" << std::endl;

			// if we do not have a first stop bit (valid bit and value 0), then move ahead a bit and repeat
			// attempting to read a byte
			if (firstStopBit.first != 1) {
				return std::make_pair(256, index);
			}

			index = firstStopBit.second;
		}

		return std::make_pair(resultByte, index);
	}

	void SetSyncedReadIndex(int index) {
		syncedIndex = index;
		syncedIndex = SyncToValidBit(syncedIndex);
	}

	// See http://www.kazojc.com/elementy_czynne/IC/8T20.pdf
	//
	std::pair<int, int> BitReadUnsyncedBiPhase(int bitCellStartIndex) {
		const int bitRate = 4800;
		int samplesPerBit = audio.SamplesPerBit(bitRate);
		// Hysterisis out of +/- 32767 - pdf says we want +/- 4mv hysterisis
		// According to https://en.wikipedia.org/wiki/Line_level, 0dB for
		// line level input is 1.095v.
		const int hysterisis = .004 * (32767/1.095);
		int oneShotTriggerIndex = bitCellStartIndex + .75 * samplesPerBit;

		// ensure we're at the start of a transition

		auto fallingEdgeIndex = audio.FindThisOrNextTransition(bitCellStartIndex + 1, hysterisis);

		// Here, due to the encoding, we guarantee that the following transition will
		// be the beginning of a bit cell.
		bitCellStartIndex = audio.FindThisOrNextTransition(oneShotTriggerIndex, hysterisis);

		int resultingBit = audio.Value(oneShotTriggerIndex) > 0;

		return std::make_pair(resultingBit, bitCellStartIndex);
	}

	std::pair<int, int> ByteReadUnsyncedBiPhase(int bitCellStartIndex) {
		return std::make_pair(0, 0);
	}

	uint8_t ByteReadSynced() {
		std::pair<int, int> byte;
		while(true) {
			if (debug) {
				audio.Dump(std::cout, syncedIndex);
			}
			byte = ByteReadUnsynced(syncedIndex);
			if (byte.first == 256) {
				syncedIndex += 4;		// skip 4 samples
				syncedIndex = SyncToValidBit(syncedIndex);
				continue;
			}
			syncedIndex = byte.second;
			break;
		}
		if (debug) std::cout << syncedIndex << ", " << audio.TimeOffset(syncedIndex) << "s: " << std::hex << static_cast<uint16_t>(byte.first) << std::dec << std::endl;
		return byte.first;
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

	while ((opt = getopt(argc, argv, "dl:pu:")) != -1) {
		switch(opt) {
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
	decoder.SetSyncedReadIndex(0);

	audio.SetInvertPhase(invertPhase);

	try {
		while(true) {
			auto ch = decoder.ByteReadSynced();
			std::cout << ch;
		}
	} catch (AudioEOF &e) {
		// ignore eof
	}
}
