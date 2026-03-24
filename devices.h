#pragma once

#include "i8080.h"

#define MAX_DEVICES 5

class Devices;

class Device
{
protected:
	Devices &m_devices;
	I8080 &m_i8080;
	bool m_interruptPending = false;
	std::string m_name;
	bool m_debug = false;

public:
	uint8_t m_IRQ = 0;
	uint8_t m_inputPort = 0;
	uint8_t m_outputPort = 0;

	Device(I8080 &i8080, Devices &devices) : m_i8080(i8080), m_devices(devices) {;}
	void SetInterruptPending(bool p = true);
	virtual void StartUp() = 0;
	virtual void ShutDown() = 0;
	virtual uint8_t Read() = 0;
	virtual void Write(uint8_t data) = 0;
	bool CheckInterrupt(I8080 *cpu);
	void Debug(bool d) { m_debug = d; }
};

typedef std::shared_ptr<Device> DevicePtr;

class Devices
{
	friend I8080;
private:
	int m_debug;
	DevicePtr m_interruptVector[8]; // there are 8 interrupt lines

	std::vector<DevicePtr> m_devices;

// the following is an optimization so that in/out instructions
// don't have to scan the above all_devices array for the port number

	DevicePtr m_inputPorts[256];
	DevicePtr m_outputPorts[256];

public:
	Devices();
	~Devices();

	void Debug(bool d) { m_debug = d; }

	void StartDevices()
	{
		for(auto device : m_devices) device->StartUp();
	}

	void StopDevices()
	{
		for(auto device : m_devices) device->ShutDown();
	}

	void AddDevice(DevicePtr device);

	void CheckInterrupts(I8080 *);

	byte_t InputFrom(byte_t port)
	{
		if(m_inputPorts[port])
			return m_inputPorts[port]->Read();
		std::cerr << "attempt to read from bad port " << static_cast<uint16_t>(port) << std::endl;
		return 0;
	}
	void OutputTo(byte_t port, byte_t val)
	{
		if(m_outputPorts[port])
			m_outputPorts[port]->Write(val);
		else
			std::cerr << "attempt to write to bad port " << static_cast<uint16_t>(port) << std::endl;
	}
};

inline void Device::SetInterruptPending(bool p)
{
	if(m_debug) {
		if(p && !m_interruptPending)
			std::cerr << "turn interrupt on" << std::endl;

		if(!p && m_interruptPending)
			std::cerr << "turn interrupt off" << std::endl;
	}

	m_interruptPending = p;
	if(p)
		m_i8080.Halt(false);
}
