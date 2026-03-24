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

/*
 * loop until we die
 * 1 if we don't have bit sync, move ahead one quarter wave and re-sync
 * 2 if we don't have byte sync, move ahead one bit
 * 3 if we don't have an e6, go back to 1
 * 4 print
 *
 *
 *
 */

template<typename T>
std::ostream &operator << (std::ostream &stream, const std::vector<T> &vec)
{
	for(auto i = 0; i < vec.size(); i++)
		stream << std::dec << i << ": " << vec[i] << std::endl;
}

// Thrown when we don't get a clean 0/1 bit.
// Tweak timebase by some fraction of a wave.
class BitSyncError : public std::domain_error {
public:
	BitSyncError(const char *s) : std::domain_error(s) {;}
};

// Thrown when we don't get a 1 for a start bit.
// Resync timebase to the next full bit (3.33ms)
class ByteSyncError : public std::domain_error {
public:
	ByteSyncError(const char *s) : std::domain_error(s) {;}
};

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
	uint64_t AudacityIndex(double t) {
		return Time2Index(t) - 0x2c;
	}
	uint64_t AudacityIndex(uint64_t t) {
		return t - 0x2c;
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

	// Given a time index in seconds, determine if either a 0 or 1
	// bit appears to be encoded there
	//
	// 1200HZ => 0 bit
	// 2400HZ => 1 bit
	// 8 zero crossings => 0 bit
	// 16 zero crossings => 1 bit
	bool Bit(double t) {

//		std::cerr << AudacityIndex(t) << ": Start decode of bit.\n";

		// We could just count them, but for the moment,
		// track the time the zero crossings occur, as well
		// (not used at the present time).
		std::vector<double> zeroCrossings;
		std::vector<uint64_t> zeroCrossingIndex;

		// define the end of interval in which a single bit is encoded
		double intervalEnd = t + bitTime;

		// now count zero crossings inside the time interval
		// [t,intervalEnd]
		while(t < intervalEnd) {
			auto nextCrossing = TimeOfNextZeroCrossing(t);
			zeroCrossings.push_back(nextCrossing);
			zeroCrossingIndex.push_back(AudacityIndex(nextCrossing));	// debug
			t = nextCrossing + QuarterWaveTime(2400);
//			t += QuarterWaveTime(2400);	// the high freq is 2400HZ, just bump halfway into the shortest wave
		}

		// These checks are heuristic - we have a couple of challenges, a)
		// actually find the bit, b) do it in such a way that we can re-sync
		// on the waveforms.

		// in 3.333ms, if we see exactly 8 crossings, that is 1200HZ, so allow some slop
		if(zeroCrossings.size() >= 5 && zeroCrossings.size() <= 10)
			return false;

		// in 3.333ms, if we see exactly 16 crossings, that is 2400HZ, so allow some slop
		if(zeroCrossings.size() >= 11 && zeroCrossings.size() <= 20)
			return true;

		// This should be unusual. From what I've seen, we certainly would expect
		// quite a few at the beginning of the audio track (i.e. before the
		// casette interface even turns on). However, during the normal part of
		// audio file, I see very few cases where this should actually happen.
		#if 0
		// get noisy if we're well into the data part of the track
		if(t > 30) {
			std::cerr << zeroCrossingIndex;
			std::cerr << AudacityIndex(t) << ": unexpected loss of bit sync. Fix.\n";
		}
		#endif
		throw BitSyncError("lost bit sync - adjust timebase forward by half or quarter wave.");
	}

	// It should be near, but won't be exactly at the start of a bit
	// that changed from high to low, so previous zero crossings
	// should be fast, next ones slow, pick the zero crossing that
	// bisects the two.
	double ResyncAtBitChange(double t, bool bit = true) {

		// pick a spot a few fast cycles back (at 2400HZ, we have 8 to choose from, pick 3)
		t -= 3 * FullWaveTime(bit ? 2400 : 1200);

		// find a zero crossing after that
		auto firstCrossing = TimeOfNextZeroCrossing(t);
		while(true) {
			auto nextCrossing = TimeOfNextZeroCrossing(firstCrossing + QuarterWaveTime(2400));
			auto period = nextCrossing - firstCrossing;	// this is the period of a half wave

			// We shouldn't have to worry too much about getting the exact wave,
			// as we have a little bit of leeway.
			if(bit) {
				if(period >= HalfWaveTime(1800))
					return firstCrossing;
			} else {
				if(period < HalfWaveTime(1800))
					return firstCrossing;
			}

			firstCrossing = nextCrossing;
		}
		// The above loop throws when we run out of data.
	}

	//
	double SyncClockToBitChange(double t) {
		while(true) {
			try {
				auto bit1 = Bit(t);
				auto bit2 = Bit(t + bitTime);

				if(bit1 && !bit2) {
					t = ResyncAtBitChange(t + bitTime, true);
					return t;
				}
				if(!bit1 && bit2) {
					t = ResyncAtBitChange(t + bitTime, false);
					return t;
				}
			} catch(BitSyncError e) {
				// Ignore bit sync error here
			}
			t += HalfWaveTime(2400);
		}
	}
};

class ByteFormatTapeReader : public TapeBitReader {
	bool m_sync = false;
public:
	ByteFormatTapeReader(const char * fileName) : TapeBitReader(fileName) {;}
	std::pair<uint8_t, double> Byte(double t) {
		auto startBit = Bit(t);

		if(!startBit)
			throw ByteSyncError("failed to find start bit - adjust timebase forward 1 bit");

//		std::cerr << "yaaay! got a start bit!!\n";

		t += bitTime;

		uint8_t byte = 0;
		bool previousBit = true;	// previous bit was the start bit
		for(auto bit = 0; bit < 8; bit++) {
			auto thisBit = Bit(t);
			if(thisBit) {
				byte |= 1 << bit;
				previousBit = thisBit;
			}

			if(previousBit && !thisBit) {
				auto tNew = ResyncAtBitChange(t);
				std::cerr << AudacityIndex(t) << ": Resync to: " << AudacityIndex(tNew) << " - was that an improvement?\n";
				t = tNew;
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
		if(!m_sync) {
			auto tNew = SyncClockToBitChange(t);
			std::cerr << AudacityIndex(t) << ": Resync to: " << AudacityIndex(tNew) << " - was that an improvement?\n";
			t = tNew;
		}
		while(true) {
			try {
				auto result = Byte(t);
				if(result.first == 0xe6 || m_sync) {
					m_sync = true;
					return result;
				}

				// We got a full byte, with no exceptions, but we aren't
				// in sync, so we need to move forward a single bit until
				// we can decode our sync 0xe6 bytes.
			} catch (BitSyncError e) {
				if(t>30)
					std::cerr << std::dec << syncCount << ": lost bit sync at time " << t << " (" << e.what() << "), moving ahead one bitTime.\n";

				if(m_sync && t>10) abort();

				m_sync = false;

				syncCount++;
			} catch (ByteSyncError e) {
				if(t>30)
					std::cerr << std::dec << syncCount << ": lost byte sync at time " << t << " (" << e.what() << "), moving ahead one bitTime.\n";
				// We failed to see a 1 start bit, so move ahead one bit.
				m_sync = false;
				syncCount++;
			}

			t += bitTime - QuarterWaveTime(2400);
			t = TimeOfNextZeroCrossing(t);
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
