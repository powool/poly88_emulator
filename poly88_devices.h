#pragma once

#include "i8080_types.h"
#include "devices.h"

#include <fstream>
#include <functional>
#include <memory>
#include <queue>

extern void MyIntSleep(int);

class KeyBoard : public Device
{
	std::queue<uint8_t>	keys;
	uint8_t lastKey;
public:
	KeyBoard(I8080 &i8080, Devices &devices);
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	void Write(uint8_t data) override;
	bool Poll();
	bool RunEmulatorCommand(const std::vector<std::string> &args);
	void Insert(uint8_t data);
};

class Timer : public Device
{
	static void Interrupt(int signum);
	static std::function<void()>	timerCallback;
public:
	Timer(I8080 &i8080, Devices &devices);
	~Timer();
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	// Write might need to clear the pending interrupt, I am not sure
	void Write(uint8_t data);
};

class IUsartFile
{
public:
	enum State {
		INPUT = 0,
		OUTPUT
	};
	virtual uint8_t Ready() = 0;
	virtual uint8_t Read() = 0;
	virtual void Write(uint8_t data) = 0;
	State GetState() { return usartState; }
protected:
	State usartState;
	std::string	filename;
};

class UsartInputFile : public IUsartFile
{
public:
	UsartInputFile(std::string filename) : input(filename, std::ios_base::binary)
	{
		if (input.fail()) {
			throw std::invalid_argument(std::string("can't open file: " + filename));
		}
		usartState = INPUT;
		filename = filename;
		std::cerr << "Open input file: " << filename << std::endl;
	}
	~UsartInputFile() {
		std::cerr << "Close input file: " << filename << std::endl;
	}
	uint8_t Ready() { return !input.eof(); }
	uint8_t Read() { return input.get(); }
	void Write(uint8_t data) { ; }
private:
	std::ifstream	input;
};

class UsartOutputFile : public IUsartFile
{
public:
	UsartOutputFile(std::string filename) : output(filename, std::ios_base::binary)
	{
		usartState = OUTPUT;
		filename = filename;
		std::cerr << "Open output file: " << filename << std::endl;
	}
	~UsartOutputFile() {
		std::cerr << "Close output file: " << filename << std::endl;
	}
	uint8_t Ready() { return true; }
	uint8_t Read() { return 0; }
	void Write(uint8_t data) {
		output.put(data);
	}
private:
	std::ofstream	output;
};

class Usart : public Device
{
	friend class UsartControl;
public:
	Usart(I8080 &i8080, Devices &devices);
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	void Write(uint8_t data) override;
protected:
	std::shared_ptr<IUsartFile> usartFile;
};

// Usart control
class UsartControl : public Device
{
	std::queue<std::string> readFiles;
	std::queue<std::string> writeFiles;
public:
	UsartControl(I8080 &i8080, Devices &devices, std::shared_ptr<Usart> usart);
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	void Write(uint8_t data) override;
	void SetUsartFile(std::shared_ptr<IUsartFile> usartFile) { usart->usartFile = usartFile; }
	void Poll();
	bool RunEmulatorCommand(const std::vector<std::string> &args);
private:
	std::shared_ptr<Usart> usart;
	bool tapeRunning;
	int	tapeTimeout;
};

// Baud Rate Generator - NOP on port 4
class BaudRateGenerator : public Device
{
public:
	BaudRateGenerator(I8080 &i8080, Devices &devices);
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	void Write(uint8_t data) override;
};

std::shared_ptr<IUsartFile> OpenTapeFile(std::string fileName);
