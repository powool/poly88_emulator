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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio.h"

const WaveHeader::ChunkHeader *WaveHeader::GetChunkHeader(const char *header) {
	int chunkHeaderOffset = sizeof(RIFFHeader);

	return reinterpret_cast<const ChunkHeader *>(header + chunkHeaderOffset);
}

int WaveHeader::Size() {
	// probably not safe/portable:
	return sizeof(RIFFHeader) + sizeof(ChunkHeader) + sizeof(DataHeader);
}

uint32_t WaveHeader::DataSize(const char *header) {
	int dataHeaderOffset = sizeof(RIFFHeader) + sizeof(ChunkHeader);

	const DataHeader *dataHeader = reinterpret_cast<const DataHeader *>(header + dataHeaderOffset);
	return dataHeader->size;
}

uint32_t WaveHeader::SampleSize(const char *header) {
	return GetChunkHeader(header)->sampleSize;
}

uint32_t WaveHeader::SamplesPerSecond(const char *header) {
	return GetChunkHeader(header)->samplesPerSecond;
}


Audio::Audio(const std::string &fileName) {
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

void Audio::SetInvertPhase(bool invertPhase) {this->invertPhase = invertPhase;}

int Audio::Negative(int index) {
	return std::signbit(wavData[index]);
}

int16_t Audio::Value(int index) {
	if(invertPhase) return -wavData[index];
	else return wavData[index];
}

int Audio::SampleRate() { return samplesPerSecond; }

int Audio::SampleCount() { return sampleCount; }

// samples per second / bits per second => samples per bit
int Audio::SamplesPerBit(int bitRate) { return SampleRate() / bitRate; }

double Audio::TimeOffset(int index) {
	return double(index) / SampleRate();
}

// This detects a negative to positive transition
int Audio::FindThisOrNextZeroCrossing(int index, int hysterisis) {

	// skip to next negative to positive signal transition
	while (index < SampleCount() - 1) {
		if (Value(index) - hysterisis < 0 && Value(index + 1) - hysterisis >= 0) {
			break;
		}
		index++;
	}
	return index;
}

// This detects a positive to negative transition
int Audio::FindThisOrNextNegativeZeroCrossing(int index, int hysterisis) {

	// skip to next negative to positive signal transition
	while (index < SampleCount() - 1) {
		if (Value(index) + hysterisis >= 0 && Value(index + 1) + hysterisis < 0) {
			break;
		}
		index++;
	}
	return index;
}

// This finds a nearby negative to positive transition.
// Apparently not terribly useful.
int Audio::FindNearestZeroCrossing(int index, int bitRate, int hysterisis) {
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
int Audio::FindThisOrNextTransition(int index, int hysterisis) {
	// Skip to next negative to positive signal transition.
	// Caller needs to verify if this is a local transition or not
	while (index < SampleCount()) {
		if ((Value(index) - hysterisis < 0 && Value(index + 1) - hysterisis >= 0) ||
				(Value(index) + hysterisis >=0 && Value(index + 1) + hysterisis < 0)) {
			break;
		}
		index++;
	}

	if(index >= SampleCount()) {
			throw AudioEOF("ran out of data");
	}

	return index;
}

// Detect if this is a regional high point.
// Due to noisy signals, the caller needs to see if this
// peak is unique.
// Example patterns seen near peak:  30 40 50 40 50 40 30
//                                   30 40 50 50 50 40 30
bool Audio::IsAPeak(int index) {
	if (index < 0 || index > sampleCount - 1) return false;
	return !Negative(index) && Value(index - 1) <= Value(index) && Value(index) >= Value(index+1);
}

void Audio::Dump(std::ostream &stream, int index, int count) {
	stream << index << ", " << TimeOffset(index) << "s: ";
	for(auto i = index - 3; i < index; i++) {
		stream << " " << Value(i);
		if (i < index - 1) stream << ", ";
	}
	stream << " (";
	stream << " " << Value(index) << ") ";
	for(auto i = index + 1; i < index + 4 + count; i++) {
		stream << " " << Value(i);
		if (i < index + 3) stream << ", ";
	}
	stream << std::endl;
}
