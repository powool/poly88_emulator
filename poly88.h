#pragma once

#include "i8080.h"
#include "MediaQueue.hpp"
#include "memory.h"
#include "poly88_devices.h"

class Poly88 : public I8080
{
	std::shared_ptr<MediaQueue> mediaQueue;
	Devices	devices;
	std::shared_ptr<KeyBoard> keyboard;
	std::shared_ptr<Usart> usart;
	std::shared_ptr<UsartControl> usartControl;
public:

	Poly88(std::shared_ptr<MediaQueue> mediaQueue);
	~Poly88();

	bool Run(uint64_t &machineCycle, bool freeRunning = true);
	void Command();

	void ReadStartupFile();
	bool RunEmulatorCommand(const std::vector<std::string> &args);

	void Debug(bool debug) {
		devices.Debug(debug);
	}

	void LoadROM(const char *filename) { memory.LoadROM(filename); }
	void LoadRAM(const char *filename) { memory.LoadRAM(filename); }
	void KeyPress(uint8_t ch);
};
