#pragma once

#include <istream>
#include <string>

/**
 * Allow async reads of input.
 *
 * istream::readsome() is very implementation depdendent,
 * however, in Linux, in main(), you can call:
 * 		std::ios::sync_with_stdio(false);
 * to allow readsome() to attempt to read data if any
 * is available.
 */
class PolledString {
	std::string inputLine;
	std::istream &inputStream;

	// Non-blocking read of characters up to a newline.
	void Poll() {
		char inputChar[2];

		// If we are polled again after having gotten a line,
		// clear the line and resume.
		if (inputLine.size() && inputLine.back() == '\n') {
			inputLine = "";
		}

		while (!inputStream.eof() && inputStream.readsome(inputChar, 1) > 0) {
			inputLine += inputChar[0];
			if (inputChar[0] == '\n') {
				break;
			}
		}
	}
	
public:
	PolledString(std::istream &input) : inputStream(input) {
	}

	std::shared_ptr<std::string> PollAndGetStringIfPresent() {
		Poll();

		std::shared_ptr<std::string> string = nullptr;

		if (inputLine.size() && inputLine.back() == '\n') {
			string = std::make_shared<std::string>(inputLine);
			inputLine = "";
		}

		return string;
	}
};
