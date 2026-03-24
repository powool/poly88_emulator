#pragma once

#include "i8080.h"
#include "poly88_devices.h"

class Poly88 : public I8080
{
	Devices	m_devices;
	std::shared_ptr<KeyBoard> m_keyboard;
	std::shared_ptr<Usart> m_usart;
	std::shared_ptr<UsartControl> m_usartControl;
public:

	Poly88();
	~Poly88();

	bool Run(uint64_t &machineCycle);
	void Command();

	void ReadStartupFile();
	bool RunEmulatorCommand(const std::vector<std::string> &args);

	void Debug(bool debug) {
		m_devices.Debug(debug);
	}

	void LoadROM(const char *filename) { memory.LoadROM(filename); }
	void LoadRAM(const char *filename) { memory.LoadRAM(filename); }
};
