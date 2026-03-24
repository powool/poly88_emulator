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
#include "PolyAudioTapeDecoder.hpp"
#include "tape_header.h"

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
	int startingIndex = 0;

	while ((opt = getopt(argc, argv, "b:di:p")) != -1) {
		switch(opt) {
			case 'b':
				bitRate = atoi(optarg);
				break;
			case 'd':
				debug = true;
				break;
			case 'i':
				startingIndex = atoi(optarg);
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
	if (startingIndex) decoder.SetIndex(startingIndex);
	if(bitRate) decoder.SetBitRate(bitRate);

	audio.SetInvertPhase(invertPhase);

	try {
		while(true) {
			decoder.ReadTape();
		}
	} catch (AudioEOF &e) {
		std::cerr << "Reached EOF!" << std::endl;
	} catch (ChecksumError &e) {
		std::cerr <<  e.what() << std::endl;
	}
}
