#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//
// Read WAV file waveforms, converting samples
// to zero crossings, direction, and time base.
//
class TapeWaveReader {
public:
	TapeWaveReader(const char *fileName) {
		struct stat b;

		if(stat(fileName, &b))
		{
			perror((std::string() + "can't stat " + fileName).c_str());
			exit(1);
		}

		m_wav.resize(b.st_size);

		std::ifstream inputStream(fileName);
		if(!inputStream.good())
			throw(std::invalid_argument(std::string() + "can't open file: " + fileName));

		inputStream.read(reinterpret_cast<char *>(&m_wav[0]), m_wav.size());
	}
	double TimeOfNextZeroCrossing(double t) {
		auto index = Time2Index(t);
		auto startingSign = Sign(index);
		if(startingSign == 0)
			return t;
		while(Sign(++index) == startingSign) {;}
		return Index2Time(index);
	}

	double FullWaveTime(uint64_t hz) {
		return (1.0 / hz);
	}

	double HalfWaveTime(uint64_t hz) {
		return FullWaveTime(hz) / 2.0;
	}

	double QuarterWaveTime(uint64_t hz) {
		return FullWaveTime(hz) / 4.0;
	}

protected:
	std::string	m_fileName;
	std::vector<uint8_t>	m_wav;
	bool	m_phase = true;
	double Index2Time(uint64_t t) {
		return t / 44100.0;
	}
	uint64_t Time2Index(double t) {
		return 44100.0 * t;
	}
	int32_t Sign(uint64_t index) {
		auto value = Value(index);
		if(value < 0x80) return -1;
		if(value == 0x80) return 0;
		return 1;
	}
	uint8_t Value(uint64_t index) {
		if(index >= m_wav.size())
			throw std::out_of_range("oopsie, used all your dataz");
		if(m_phase)
			return m_wav[index];
		else
			return 0xff - m_wav[index];
	}
};

//
// Decode bits at some time t
//
class TapeBitReader : public TapeWaveReader {
protected:
	const double bitTime = 1.0 / 300.0;
public:
	TapeBitReader(const char * fileName) : TapeWaveReader(fileName) {;}

	bool Bit(double t) {
		std::vector<double> zeroCrossings;
		// this is the timer interval in which a single bit is encoded
		double intervalEnd = t + bitTime;

		while(t < intervalEnd) {
			auto nextCrossing = TimeOfNextZeroCrossing(t);
			zeroCrossings.push_back(nextCrossing);
			t = nextCrossing + QuarterWaveTime(2400);
//			t += QuarterWaveTime(2400);	// the high freq is 2400HZ, just bump halfway into the shortest wave
		}

		// in 3.333ms, if we see exactly 8 crossings, that is 1200HZ, so allow some slop
		if(zeroCrossings.size() >= 6 && zeroCrossings.size() <= 10)
			return false;

		// in 3.333ms, if we see exactly 16 crossings, that is 2400HZ, so allow some slop
		if(zeroCrossings.size() >= 14 && zeroCrossings.size() <= 18)
			return true;

		throw(std::domain_error("lost sync"));
	}

	// t should be near, but won't be exactly at the start of a bit
	// that changed from high to low, so previous zero crossings
	// should be fast, next ones slow, pick the zero crossing that
	// bisects the two.
	double ResyncAtBitChange(double t) {

		// pick a spot a few fast cycles back (at 2400HZ, we have 8 to choose from, pick 3)
		t -= 3 * FullWaveTime(2400);

		// find a zero crossing after that
		auto firstCrossing = TimeOfNextZeroCrossing(t);
		while(true) {
			auto nextCrossing = TimeOfNextZeroCrossing(firstCrossing + QuarterWaveTime(2400));

			// We shouldn't have to worry too much about getting the exact wave,
			// as we have a little bit of leeway.
			if(nextCrossing - firstCrossing > HalfWaveTime(2400) * 1.5)
				return firstCrossing;

			firstCrossing = nextCrossing;
		}
		// The above loop throws when we run out of data.
	}
};

class ByteFormatTapeReader : public TapeBitReader {
	bool m_sync = false;
public:
	ByteFormatTapeReader(const char * fileName) : TapeBitReader(fileName) {;}
	std::pair<uint8_t, double> Byte(double t) {
		auto startBit = Bit(t);

		if(!startBit)
			throw(std::domain_error("failed to find start bit"));

//		std::cerr << "yaaay! got a start bit!!\n";

		t += bitTime;

		uint8_t byte = 0;
		bool previousBit = true;	// previous bit was the start bit
		for(auto bit = 0; bit < 8; bit++) {
			auto thisBit = Bit(t);
			if(thisBit)
				byte |= 1 << bit;

			if(previousBit && !thisBit) {
				t = ResyncAtBitChange(t);
				previousBit = thisBit;
			}

			t += bitTime;
		}

		// skip two filler bits
		t += bitTime;
		t += bitTime;
		return std::make_pair(byte, t);
	}

	// Now we'll sync till we get an 0xe6, then call ourselves in
	// sync and return bytes until we're out of sync or throw on
	// end of sound data.
	std::pair<uint8_t, double> ReadByteAndSync(double t) {
		uint64_t syncCount = 0;
		while(true) {
			try {
				auto result = Byte(t);
				if(!m_sync && result.first != 0xe6)
					throw std::domain_error("not in sync yet");
				// Now we're synced, so return the 0xe6 sync byte,
				// and all data that is synced afterwards.
				m_sync = true;
				return result;
			} catch (std::domain_error e) {
				std::cerr << std::dec << syncCount << ": lost sync at time " << t << " (" << e.what() << "), moving ahead one bitTime.\n";
				m_sync = false;
				syncCount++;
				t += HalfWaveTime(2400); 
				t = TimeOfNextZeroCrossing(t);
			}
		}
	}
};

void usage(int argc, char **argv)
{
	std::cerr << "usage: " << argv[0] << " [WAV poly 88 file]" << std::endl;
}

/*
 * stuff to set:
 *    wave file samples/second
 *    expected bit rate (2400/4800/9600 bps)
 *    phase
 *    filename.wav
 */
int main(int argc, char **argv)
{

	int opt;
	bool phase = false;
	bool debug = false;
	uint8_t lower = 120;
	uint8_t upper = 134;

	while ((opt = getopt(argc, argv, "dl:pu:")) != -1) {
		switch(opt) {
			case 'd':
				debug = true;
				break;
			case 'l':
				lower = strtoul(optarg, NULL, 10);
				break;
			case 'p':
				phase = !phase;
				break;
			case 'u':
				upper = strtoul(optarg, NULL, 10);
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

	auto byteReader = std::make_shared<ByteFormatTapeReader>(argv[optind]);
	double t = 3.0;
	try {
		while(true) {
			auto result = byteReader->ReadByteAndSync(t);
			
			std::cout << "read byte: " << std::hex << (uint32_t) result.first << "\n";

			t = result.second;
		}
	} catch(std::out_of_range e)
	{
		std::cout << "\n";
	}
}
