
#include "devices.h"    // class library
#include <iostream>
#include <stdlib.h>

//
// poly 88 has three devices that I care about in this file
// 1) keyboard(parallel, svc 5, vi2, in port f8 resets interrupt,gets data)
// 2) timer (svc 6, out port 8 to reset int to next 1/60 second)
// 3) tape (svc 4, vi3, status port (in 1) bit 2 means data ready, in port 0 for data)
//  write 96 to port one to start things up, resync usart
//  write 5 to port 4 for 2400 baud
//  write 6 to port 4 for 300 baud
//  write bunch of commands to port 1 for setup
//      includes sync bytes to search for, etc. see monitor code for doc
//
//  write to port 12 enables single step
//  e.g. ei, out 12, ret to user instruction
//
// add a commment

Devices::Devices()
{
}

void Devices::AddDevice(DevicePtr device)
{
	devices.push_back(device);

	inputPorts[device->inputPort] = device;
	outputPorts[device->outputPort] = device;

	interruptVector[device->IRQ] = device;
}

// returns true if we were able to trigger our interrupt
bool Device::CheckInterrupt(I8080 *cpu)
{
	if(interruptPending)
	{
		// only deliver an interrupt if we're not already doing one
		if(cpu->InterruptEnable() && cpu->Interrupt(IRQ))
		{
			if(debug)
				std::cerr << name << ": triggerring CPU interrupt " << (uint16_t) IRQ << std::endl;
			return true;
		}
	}
	return false;
}

void
Devices::CheckInterrupts(I8080 *cpu)
{
	if(!cpu->InterruptEnable())
		return;

	for(auto &device : interruptVector)
		if(device)
			if(device->CheckInterrupt(cpu))
				break;
}


Devices::~Devices()
{
}
