
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
	m_devices.push_back(device);

	m_inputPorts[device->m_inputPort] = device;
	m_outputPorts[device->m_outputPort] = device;

	m_interruptVector[device->m_IRQ] = device;
}

// returns true if we were able to trigger our interrupt
bool Device::CheckInterrupt(I8080 *cpu)
{
	if(m_interruptPending)
	{
		// only deliver an interrupt if we're not already doing one
		if(cpu->InterruptEnable() && cpu->Interrupt(m_IRQ))
		{
			if(m_debug)
				std::cerr << m_name << ": triggerring CPU interrupt " << (uint16_t) m_IRQ << std::endl;
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

	for(auto &device : m_interruptVector)
		if(device)
			if(device->CheckInterrupt(cpu))
				break;
}


Devices::~Devices()
{
}
