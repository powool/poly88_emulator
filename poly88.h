#pragma once

#include "i8080.h"
#include "FileDialogBridge.hpp"
#include "memory.h"
#include "poly88_devices.h"

class Poly88 : public I8080
{
	std::shared_ptr<FileDialogBridge> fileDialogBridge;
	Devices	devices;
	std::shared_ptr<KeyBoard> keyboard;
	std::shared_ptr<Usart> usart;
	std::shared_ptr<UsartControl> usartControl;
public:

	Poly88( std::shared_ptr<FileDialogBridge> fileDialogBridge,
		MemoryInterfacePtr memory);
	~Poly88();

	bool Run(uint64_t &machineCycle, bool freeRunning = true);
	void Command();

	void ReadStartupFile();
	bool RunEmulatorCommand(const std::vector<std::string> &args);

	void Debug(bool debug) {
		devices.Debug(debug);
	}

	void KeyPress(uint8_t ch);
};
