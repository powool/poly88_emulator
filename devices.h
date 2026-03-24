#pragma once

#include "i8080.h"

#define MAX_DEVICES 5

class Devices;

class Device
{
protected:
	Devices &devices;
	I8080 &i8080;
	bool interruptPending = false;
	std::string name;
	bool debug = false;

public:
	uint8_t IRQ = 0;
	uint8_t inputPort = 0;
	uint8_t outputPort = 0;

	Device(I8080 &i8080, Devices &devices) : i8080(i8080), devices(devices) {;}
	void SetInterruptPending(bool p = true);
	virtual void StartUp() = 0;
	virtual void ShutDown() = 0;
	virtual uint8_t Read() = 0;
	virtual void Write(uint8_t data) = 0;
	bool CheckInterrupt(I8080 *cpu);
	void Debug(bool d) { debug = d; }
};

typedef std::shared_ptr<Device> DevicePtr;

class Devices
{
	friend I8080;
private:
	int debug;
	DevicePtr interruptVector[8]; // there are 8 interrupt lines

	std::vector<DevicePtr> devices;

// the following is an optimization so that in/out instructions
// don't have to scan the above all_devices array for the port number

	DevicePtr inputPorts[256];
	DevicePtr outputPorts[256];

public:
	Devices();
	~Devices();

	void Debug(bool d) { debug = d; }

	void StartDevices()
	{
		for(auto device : devices) device->StartUp();
	}

	void StopDevices()
	{
		for(auto device : devices) device->ShutDown();
	}

	void AddDevice(DevicePtr device);

	void CheckInterrupts(I8080 *);

	byte_t InputFrom(byte_t port)
	{
		if(inputPorts[port])
			return inputPorts[port]->Read();
		std::cerr << "attempt to read from bad port " << static_cast<uint16_t>(port) << std::endl;
		return 0;
	}
	void OutputTo(byte_t port, byte_t val)
	{
		if(outputPorts[port])
			outputPorts[port]->Write(val);
		else
			std::cerr << "attempt to write to bad port " << static_cast<uint16_t>(port) << std::endl;
	}
};

inline void Device::SetInterruptPending(bool p)
{
	if(debug) {
		if(p && !interruptPending)
			std::cerr << "turn interrupt on" << std::endl;

		if(!p && interruptPending)
			std::cerr << "turn interrupt off" << std::endl;
	}

	interruptPending = p;
	if(p)
		i8080.Halt(false);
}
