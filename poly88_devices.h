#pragma once

#include "i8080_types.h"
#include "devices.h"

#include <fstream>
#include <functional>
#include <memory>
#include <queue>

extern void MyIntSleep(int);

extern void Command(const char *msg = NULL);

class KeyBoard : public Device
{
	std::queue<uint8_t>	m_keys;
	uint8_t m_lastKey;
public:
	KeyBoard(I8080 &i8080, Devices &devices);
	void StartUp() override;
	void ShutDown() override;
	uint8_t Read() override;
	void Write(uint8_t data) override;
	bool Poll();
};

class Timer : public Device
{
	static void Interrupt(int signum);
	static std::function<void()>	m_timerCallback;
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
	State GetState() { return m_usartState; }
protected:
	State m_usartState;
	std::string	m_filename;
};

class UsartInputFile : public IUsartFile
{
public:
	UsartInputFile(std::string filename) : m_input(filename, std::ios_base::binary)
	{
		if (m_input.fail()) {
			throw std::invalid_argument(std::string("can't open file: " + filename));
		}
		m_usartState = INPUT;
		m_filename = filename;
		std::cerr << "Open input file: " << m_filename << std::endl;
	}
	~UsartInputFile() {
		std::cerr << "Close input file: " << m_filename << std::endl;
	}
	uint8_t Ready() { return !m_input.eof(); }
	uint8_t Read() { return m_input.get(); }
	void Write(uint8_t data) { ; }
private:
	std::ifstream	m_input;
};

class UsartOutputFile : public IUsartFile
{
public:
	UsartOutputFile(std::string filename) : m_output(filename, std::ios_base::binary)
	{
		m_usartState = OUTPUT;
		m_filename = filename;
		std::cerr << "Open output file: " << filename << std::endl;
	}
	~UsartOutputFile() {
		std::cerr << "Close output file: " << m_filename << std::endl;
	}
	uint8_t Ready() { return true; }
	uint8_t Read() { return 0; }
	void Write(uint8_t data) {
		m_output.put(data);
	}
private:
	std::ofstream	m_output;
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
	std::shared_ptr<IUsartFile> m_usartFile;
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
	void SetUsartFile(std::shared_ptr<IUsartFile> usartFile) { m_usart->m_usartFile = usartFile; }
	void Poll();
	bool RunEmulatorCommand(const std::vector<std::string> &args);
private:
	std::shared_ptr<Usart> m_usart;
	bool m_tapeRunning;
	int	m_tapeTimeout;
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
