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
		int chunkHeaderOffset = sizeof(RIFFHeader);

		const ChunkHeader *chunkHeader = reinterpret_cast<const ChunkHeader *>(header + chunkHeaderOffset);
		return chunkHeader->sampleSize;
	}
};

class Audio {
	std::unique_ptr<int16_t []>	wavData;
	int sampleCount;
public:
	Audio(const std::string &fileName) {
		struct stat b;

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

		wavData = std::make_unique<int16_t []>(sampleCount);

		inputStream.read(reinterpret_cast<char *>(&(wavData[0])), dataSize);
	}

	int Negative(int index) {
		return std::signbit(wavData[index]);
	}
	int16_t Value(int index) {
		return wavData[index];
	}

	// XXX get the header and use it:
	int SampleRate() { return 44100; }
	int SampleCount() { return sampleCount; }

	// samples per second / bits per second => samples per bit
	int SamplesPerBit(int bitRate) { return SampleRate() / bitRate; }

	double TimeOffset(int index) {
		return double(index) / SampleRate();
	}

	int FindThisOrNextZeroCrossing(int index, int bitRate) {
		auto lastIndex = SampleCount() - 2 * SamplesPerBit(bitRate);
		
		// skip to next negative to positive signal transition
		while (index < lastIndex) {
			if (Negative(index) && !Negative(index + 1)) {
				break;
			}
			index++;
		}
		return index;
	}
	int FindNearestZeroCrossing(int index, int bitRate) {
		auto lastIndex = SampleCount() - 2 * SamplesPerBit(bitRate);
		
		// find nearest negative to positive signal transition
		for (int distance = 0; distance < SamplesPerBit(bitRate); distance++) {
			if (Negative(index + distance) && !Negative(index + distance + 1)) {
				index += distance;
				break;
			}
			if (Negative(index - distance) && !Negative(index - distance + 1)) {
				index -= distance;
				break;
			}
		}

		return index;
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
		if (!audio.Negative(index) && audio.Value(index - 1) <= audio.Value(index) && audio.Value(index) >= audio.Value(index+1)) {
			if (abs(indexOfLastPeak - index) > 10) {
				fullWaveCount++;
				indexOfLastPeak = index;
			}
		}
		index++;
	}

	// recode full wave count found to 0 or 1 bit
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

	return std::make_pair(resultBit, audio.FindThisOrNextZeroCrossing(indexOfLastPeak, 300));
}

std::pair<int, int> BitDecodePolyPhaseEncodedBit(Audio &audio, int index, int bitRate) {
	// not implemented
	return std::make_pair(0, 0);
}

class Uart {
	bool debug;
	const int bitRate = 300;	// bit per second
	std::pair<int, int> (*bitDecoder)(Audio &, int, int);
	int syncedIndex;

public:
	Uart(std::pair<int, int> (*bitDecoder)(Audio &, int, int)) {
		debug = false;
		this->bitDecoder = bitDecoder;
		syncedIndex = 0;
	}

	void SetDebug(int debug) { this->debug = debug; }

	std::pair<int, int> BitRead(Audio &audio, int index) {
		return bitDecoder(audio, index, bitRate);
	}

	// return the index of the next valid bit, throw on out of data
	int SyncToValidBit(Audio &audio, int index) {
		auto lastIndex = audio.SampleCount() - 2 * audio.SamplesPerBit(bitRate);
		while(index < lastIndex) {
			index = audio.FindThisOrNextZeroCrossing(index, bitRate);

			auto bit = BitRead(audio, index);
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
	std::pair<int, int> ByteReadUnsynced(Audio &audio, int index) {
		int samplesPerBit = audio.SamplesPerBit(300);
		uint8_t resultByte = 0;

		std::pair<int, int> startBit;

		startBit = BitRead(audio, index);

		if (startBit.first != 0) {
			return std::make_pair(256, 0);
		}

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(startBit.first) << std::dec << " start bit" << std::endl;

		index = startBit.second;

		int bitIndex;
		// in theory, we have a stop bit, now get 8 data bits
		for(bitIndex = 0; bitIndex < 8; bitIndex++) {
			auto dataBit = BitRead(audio, index);

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

		auto firstStopBit = BitRead(audio, index);

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(firstStopBit.first) << std::dec << " first stop bit" << std::endl;

		// if we do not have a first stop bit (valid bit and value 0), then move ahead a bit and repeat
		// attempting to read a byte
		if (firstStopBit.first != 1) {
			return std::make_pair(256, index);
		}

		index = firstStopBit.second;

		auto secondStopBit = BitRead(audio, index);

		if (debug) std::cout << index << ", " << audio.TimeOffset(index) << "s: " << std::hex << static_cast<uint16_t>(secondStopBit.first) << std::dec << " second stop bit" << std::endl;

		if (secondStopBit.first != 1) {
			return std::make_pair(256, index);
		}

		index = secondStopBit.second;

		return std::make_pair(resultByte, index);
	}

	void SetSyncedReadIndex(Audio &audio, int index) {
		syncedIndex = index;
		syncedIndex = SyncToValidBit(audio, syncedIndex);
	}

	uint8_t ByteReadSynced(Audio &audio) {
		std::pair<int, int> byte;
		while(true) {
			if (debug) {
				audio.Dump(std::cout, syncedIndex);
			}
			byte = ByteReadUnsynced(audio, syncedIndex);
			if (byte.first == 256) {
				syncedIndex += 4;		// skip 4 samples
				syncedIndex = SyncToValidBit(audio, syncedIndex);
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
}

int main(int argc, char **argv) {
	int opt;
	int debug = false;
	bool phase = false;

	while ((opt = getopt(argc, argv, "dl:pu:")) != -1) {
		switch(opt) {
			case 'd':
				debug = true;
				break;
			case 'p':
				phase = !phase;
				break;
			default:
				usage(argc, argv);
				exit(1);
		}
	}

	Audio audio(argv[optind]);
	Uart uart(DecodeByteEncodedBit);
	uart.SetDebug(debug);
	uart.SetSyncedReadIndex(audio, 0);

	try {
		while(true) {
			auto ch = uart.ByteReadSynced(audio);
			std::cout << ch;
		}
	} catch (AudioEOF &e) {
		// ignore eof
	}
}
