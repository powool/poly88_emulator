
#include "poly88.h"
#include "poly88_devices.h" // contains poly specific device names
#include "polled_string.hpp"

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>

// stolen from https://stackoverflow.com/questions/2275135/splitting-a-string-by-whitespace-in-c
static std::vector<std::string> GetArgv(const std::string& inputLine, char delim)
{
	std::vector<std::string> argv;
	std::stringstream sstream(inputLine);
	std::string arg;

	if (inputLine.size() && inputLine[0] =='#')
		return argv;

	while(getline(sstream, arg, delim)) {
		argv.push_back(arg);
	}

	return argv;
}

Poly88::Poly88()
{
	m_keyboard = std::make_shared<KeyBoard>(*this, m_devices);
	m_devices.AddDevice(m_keyboard);
	m_devices.AddDevice(std::make_shared<Timer>(*this, m_devices));
	m_devices.AddDevice(std::make_shared<BaudRateGenerator>(*this, m_devices));

	m_usart = std::make_shared<Usart>(*this, m_devices);
	m_usartControl = std::make_shared<UsartControl>(*this, m_devices, m_usart);
	m_devices.AddDevice(m_usart);
	m_devices.AddDevice(m_usartControl);

	m_devices.StartDevices();
}

Poly88::~Poly88()
{
	m_devices.StopDevices();
}

void Poly88::ReadStartupFile()
{
	std::ifstream inputStream(".poly88rc");

	while (!inputStream.fail()) {
		std::string inputLine;
		std::getline(inputStream, inputLine);

		if (inputLine.size() && inputLine.back() == '\n')
			inputLine.pop_back();

		if (inputLine.size()) {
			std::cout << "read line from .poly88rc: " << inputLine << std::endl;
			auto args = GetArgv(inputLine, ' ');
			if (RunEmulatorCommand(args)) break;
		}
	}
}

bool Poly88::RunEmulatorCommand(const std::vector<std::string> &args)
{
	if (args.size() == 0) return false;

	if (args.size() == 1 && args[0] == "quit") {
		return true;
	}

	m_usartControl->RunEmulatorCommand(args);
	I8080::RunEmulatorCommand(args);
	return false;
}

bool Poly88::Run(uint64_t &machineCycle)
{
	machineCycle++;
	// every 10K cycles, flush the screen
	if((machineCycle%100000)==0)
	{
//			std::cerr << "Updating screen: " << machineCycle << std::endl;
		memory.screen.update();
	}

	// every 1K cycles, check interrupts
	if(((machineCycle%1000)==0)) {
		m_usartControl->Poll();
		if(m_keyboard->Poll()) {
			std::cout << "User closed application." << std::endl;
			return true;
		}
		if(InterruptEnable())
			m_devices.CheckInterrupts(this);  // may reset PC
	}

	if(Halt())
	{
//		fprintf(stderr,"halted\n");
		MyIntSleep(1000000/60);
		// allow screen to refresh and keyboard to be immediately polled
		machineCycle = -1;
		return false;
	}

	if(ExecuteCycle(&m_devices)) {
		std::cout << "bad instruction!" << std::endl;
		return false;
	}

	return false;
}

void Poly88::Command()
{
	PolledString pollString(std::cin);
	uint64_t machineCycle = -1;

	std::ios::sync_with_stdio(false);

	ReadStartupFile();

	while(1)
	{
		if (machineCycle % 1000 == 0) {
			auto inputLine = pollString.PollAndGetStringIfPresent();
			if(inputLine) {
				if (inputLine->size() && inputLine->back() == '\n')
					inputLine->pop_back();

				std::cout << "got a command, here's the line: " << *inputLine << std::endl;

				auto args = GetArgv(*inputLine, ' ');
				if (RunEmulatorCommand(args)) break;
			}
		}

		if (Run(machineCycle)) {
			break;
		}
	}
}
