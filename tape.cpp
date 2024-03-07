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

class AudioEOF : std::runtime_error {
public:
	AudioEOF(const char *s) : std::runtime_error(s) {;}
};

class WaveHeader {
	struct RIFFHeader {
		char	RIFF[4];	// 'RIFF'
		uint32_t	size;	//
		char	WAVE[4];	// 'WAVE'
	};

	struct ChunkHeader {
		char		format[4];	// 'fmt '
		uint32_t	chunkSize;	// sizeof(ChunkHeader)
		uint16_t	dataFormat;	// 0x0001
		uint16_t	channels;	// 0x0001
		uint32_t	samplesPerSecond;	// 44100
		uint32_t	bytesPerSecond;	// 0x00015888
		uint16_t	sampleSize;	// 0x0002 (16-bit mono)
		uint16_t	bitsPerSample;	// 0x0010
	};

	struct DataHeader {
		char		ID[4];	// 'data'
		uint32_t	size;	//
	};

	static const ChunkHeader *GetChunkHeader(const char *header) {
		int chunkHeaderOffset = sizeof(RIFFHeader);

		return reinterpret_cast<const ChunkHeader *>(header + chunkHeaderOffset);
	}

public:
	static int Size() {
		// probably not safe/portable:
		return sizeof(RIFFHeader) + sizeof(ChunkHeader) + sizeof(DataHeader);
	}
	static uint32_t DataSize(const char *header) {
		int dataHeaderOffset = sizeof(RIFFHeader) + sizeof(ChunkHeader);

		const DataHeader *dataHeader = reinterpret_cast<const DataHeader *>(header + dataHeaderOffset);
		return dataHeader->size;
	}

	static uint32_t SampleSize(const char *header) {
		return GetChunkHeader(header)->sampleSize;
	}

	static uint32_t SamplesPerSecond(const char *header) {
		return GetChunkHeader(header)->samplesPerSecond;
	}
};

class Audio {
	std::unique_ptr<int16_t []>	wavData;
	int sampleCount;
	int samplesPerSecond;
	bool invertPhase;
public:
	Audio(const std::string &fileName) {
		struct stat b;

		invertPhase = false;

		if(stat(fileName.c_str(), &b)) {
			perror((std::string() + "can't stat " + fileName).c_str());
			exit(1);
		}

		if (b.st_size < WaveHeader::Size()) {
			throw(std::invalid_argument(std::string() + "can't open file: " + fileName + ": truncated header"));
		}

		std::ifstream inputStream(fileName);

		if(!inputStream.good()) {
			throw(std::invalid_argument(std::string() + "can't open file: " + fileName));
		}

		auto headerData = std::make_unique<char []>(WaveHeader::Size());

		inputStream.read(&(headerData[0]), WaveHeader::Size());

		auto dataSize = WaveHeader::DataSize(&(headerData[0]));

		if (dataSize + WaveHeader::Size() > b.st_size) {
			throw(std::invalid_argument(std::string() + "can't open file: " + fileName + ": truncated data"));
		}

		sampleCount = dataSize / WaveHeader::SampleSize(&(headerData[0]));
		samplesPerSecond = WaveHeader::SamplesPerSecond(&(headerData[0]));

		wavData = std::make_unique<int16_t []>(sampleCount);

		inputStream.read(reinterpret_cast<char *>(&(wavData[0])), dataSize);
	}

	void SetInvertPhase(bool invertPhase) {this->invertPhase = invertPhase;}

	int Negative(int index) {
		return std::signbit(wavData[index]);
	}

	int16_t Value(int index) {
		if(invertPhase) return -wavData[index];
		else return wavData[index];
	}

	int SampleRate() { return samplesPerSecond; }

	int SampleCount() { return sampleCount; }

	// samples per second / bits per second => samples per bit
	int SamplesPerBit(int bitRate) { return SampleRate() / bitRate; }

	double TimeOffset(int index) {
		return double(index) / SampleRate();
	}

	// This detects a negative to positive transition
	int FindThisOrNextZeroCrossing(int index, int bitRate, int hysterisis = 0) {
		auto lastIndex = SampleCount() - 2 * SamplesPerBit(bitRate);

		// skip to next negative to positive signal transition
		while (index < lastIndex) {
			if (Value(index) - hysterisis < 0 && Value(index + 1) - hysterisis >= 0) {
				break;
			}
			index++;
		}
		return index;
	}

	// This detects a positive to negative transition
	int FindThisOrNextNegativeZeroCrossing(int index, int bitRate, int hysterisis = 0) {
		auto lastIndex = SampleCount() - 2 * SamplesPerBit(bitRate);

		// skip to next negative to positive signal transition
		while (index < lastIndex) {
			if (Value(index) + hysterisis >= 0 && Value(index + 1) + hysterisis < 0) {
				break;
			}
			index++;
		}
		return index;
	}

	// This detects a negative to positive transition
	int FindNearestZeroCrossing(int index, int bitRate, int hysterisis = 0) {
		auto lastIndex = SampleCount() - 2 * SamplesPerBit(bitRate);
		
		// find nearest negative to positive signal transition
		for (int distance = 0; distance < SamplesPerBit(bitRate); distance++) {
			if (Value(index + distance) - hysterisis < 0 && Value(index + distance + 1) - hysterisis >= 0) {
				index += distance;
				break;
			}
			if (Value(index - distance) - hysterisis < 0 && Value(index - distance + 1) - hysterisis >= 0) {
				index -= distance;
				break;
			}
		}

		return index;
	}

	// This detects any transition, with any polarity
	int FindThisOrNextTransition(int index, int hysterisis = 0) {
		// Skip to next negative to positive signal transition.
		// Caller needs to verify if this is a local transition or not
		while (index < SampleCount()) {
			if ((Value(index) - hysterisis < 0 && Value(index + 1) - hysterisis >= 0) ||
					(Value(index) + hysterisis >=0 && Value(index + 1) + hysterisis < 0)) {
				break;
			}
			index++;
		}
		return index;
	}

	// Detect if this is a regional high point.
	// Due to noisy signals, the caller needs to see if this
	// peak is unique.
	// Example patterns seen near peak:  30 40 50 40 50 40 30
	//                                   30 40 50 50 50 40 30
	bool IsAPeak(int index) {
		if (index < 0 || index > sampleCount - 1) return false;
		return !Negative(index) && Value(index - 1) <= Value(index) && Value(index) >= Value(index+1);
	}

	void Dump(std::ostream &stream, int index) {
		stream << index << ", " << TimeOffset(index) << "s: ";
		for(auto i = index - 3; i < index; i++) {
			stream << " " << Value(i);
			if (i < index - 1) stream << ", ";
		}
		stream << " (";
		stream << " " << Value(index) << ") ";
		for(auto i = index + 1; i < index + 4; i++) {
			stream << " " << Value(i);
			if (i < index + 3) stream << ", ";
		}
		stream << std::endl;
	}
};

// Decode 300 baud byte format data, which is a two tone encoding (AKA
// frequency shift key - FSK), where 1200HZ represents a 0, and 2400HZ
// represents a 1.
//
// To my knowledge, 300 bits per second is the only speed byte encoded tape I have.
std::pair<int, int> DecodeByteEncodedBit(Audio &audio, int index, int bitRate) {
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

	return std::make_pair(resultBit, audio.FindThisOrNextZeroCrossing(indexOfLastPeak, bitRate));
}

// handle polyphase (biphase/manchest encoded) data
std::pair<int, int> BitDecodePolyPhaseEncodedBit(Audio &audio, int index, int bitRate) {
	// not implemented
	return std::make_pair(0, 0);
}

class Uart {
	bool debug;
	const int bitRate = 300;	// bit per second
	std::pair<int, int> (*bitDecoder)(Audio &, int, int);
	int syncedIndex;
	Audio &audio;

public:
	Uart(std::pair<int, int> (*bitDecoder)(Audio &, int, int), Audio &_audio) : audio(_audio) {
		debug = false;
		this->bitDecoder = bitDecoder;
		syncedIndex = 0;
	}

	void SetDebug(int debug) { this->debug = debug; }

	std::pair<int, int> BitRead(int index) {
		return bitDecoder(audio, index, bitRate);
	}

	// return the index of the next valid bit, throw on out of data
	int SyncToValidBit(int index) {
		auto lastIndex = audio.SampleCount() - 2 * audio.SamplesPerBit(bitRate);
		while(index < lastIndex) {
			index = audio.FindThisOrNextZeroCrossing(index, bitRate);

			auto bit = BitRead(index);
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

		startBit = BitRead(index);

		// if not a 0 (start) bit, let caller know
		if (startBit.first != 0) {
			return std::make_pair(256, 0);
		}

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(startBit.first) << std::dec << " start bit" << std::endl;

		index = startBit.second;

		int bitIndex;
		// in theory, we have a stop bit, now get 8 data bits
		for(bitIndex = 0; bitIndex < 8; bitIndex++) {
			auto dataBit = BitRead(index);

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

		auto firstStopBit = BitRead(index);

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(firstStopBit.first) << std::dec << " first stop bit" << std::endl;

		// if we do not have a first stop bit (valid bit and value 0), then move ahead a bit and repeat
		// attempting to read a byte
		if (firstStopBit.first != 1) {
			return std::make_pair(256, index);
		}

		index = firstStopBit.second;

		auto secondStopBit = BitRead(index);

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(secondStopBit.first) << std::dec << " second stop bit" << std::endl;

		if (secondStopBit.first != 1) {
			return std::make_pair(256, index);
		}

		index = secondStopBit.second;

		return std::make_pair(resultByte, index);
	}

	void SetSyncedReadIndex(int index) {
		syncedIndex = index;
		syncedIndex = SyncToValidBit(syncedIndex);
	}

	// See http://www.kazojc.com/elementy_czynne/IC/8T20.pdf
	//
	std::pair<int, int> BitReadUnsyncedPolyPhase(int bitCellStartIndex) {
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

	std::pair<int, int> ByteReadUnsyncedPolyPhase(int bitCellStartIndex) {
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
	Uart uart(DecodeByteEncodedBit, audio);
	uart.SetDebug(debug);
	uart.SetSyncedReadIndex(0);

	audio.SetInvertPhase(invertPhase);

	try {
		while(true) {
			auto ch = uart.ByteReadSynced();
			std::cout << ch;
		}
	} catch (AudioEOF &e) {
		// ignore eof
	}
}
