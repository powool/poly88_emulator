
#include "poly88.h"
#include "poly88_devices.h" // contains poly specific device names

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

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

const char *Poly88::Run()
{
	auto poll = -1;
	while(1) {
		poll++;
		// every 10K cycles, flush the screen
		if((poll%100000)==0)
		{
//			std::cerr << "Updating screen: " << poll << std::endl;
			memory.screen.update();
		}

		// every 1K cycles, check interrupts
		if(((poll%1000)==0)) {
			m_usartControl->Poll();
			if(m_keyboard->Poll())
				return "quit";
			if(InterruptEnable())
				m_devices.CheckInterrupts(this);  // may reset PC
		}

		if(Halt())
		{
//		fprintf(stderr,"halted\n");
			MyIntSleep(1000000/60);
			// allow screen to refresh and keyboard to be polled
			poll = -1;
			continue;
		}

		if(ExecuteCycle(&m_devices))
			return "bad instruction!";

		if(memory[0xc80] == 0x12 && memory[0xc81] == 0x34)
		{
			memory[0xc80] = 0;
			memory[0xc81] = 0;
			return "stopped because *0x0c80 == 0x1234!";
		}
	}
}

void Poly88::Command()
{
	while(1)
	{
		std::string result = Run();
		std::cerr << result << std::endl;
		if(result == "quit")
			return;
	}

#if 0
	std::string buffer;

	if(msg) std::cout << msg;
	while(1)
	{
		int length;
		DumpState();
		std::cout << std::endl;
		std::cout << "i <filename>    tape input from filename" << std::endl;
		std::cout << "o <filename>    tape output to filename" << std::endl;
		std::cout << "d               close tape output file" << std::endl;
		std::cout << "r               reset the 8080" << std::endl;
		std::cout << "c               continue with emulator" << std::endl;
		std::cout << "q               quit" << std::endl;
		std::cout << "command [c]: ";
		fflush(stdout);
		std::getline(std::cin, buffer);
		switch(buffer[0])
		{
			case 'i':
				#if 0
				if(usart_input_file!=NULL)
				{
					std::cout << "warning, closing previously opened file." << std::endl;
					fclose(usart_input_file);
				}
				std::cout << "setting input file to " << buffer.c_str()+2 << std::endl;
//				usart_device.SetInputFile(foobar);
				strcpy(usart_input_file_name, buffer.c_str() + 2);
				usart_input_file = fopen(usart_input_file_name, "r");
				if(usart_input_file==NULL)
					perror("fopen");
				*usart_flag_1 = true;
				*usart_flag_2 = true;
				#endif
				break;
			case 'o':
				#if 0
				if(usart_output_file != NULL)
					fclose(usart_output_file);
				strcpy(usart_output_file_name, buffer.c_str() + 2);
				std::cout << "trying to open file " << usart_output_file_name << std::endl;
//				usart_device.SetInputFile(foobar);
				usart_output_file = fopen(usart_output_file_name,"r");
				if(usart_output_file == NULL)
					perror("open");
				#endif
				break;
			case 'd':
				#if 0
				if(usart_output_file != NULL)
					fclose(usart_output_file);
				usart_output_file = NULL;
				#endif
				break;
			case 'r':
				poly->Reset();
				break;
			case 'c':
				poly->memory.redraw_screen();
				return;
			case 'q':
				#if 0
				if(usart_output_file!=NULL) fclose(usart_output_file);
				#endif
//                endwin();
				exit(0);
				break;
			default:
				poly->memory.redraw_screen();
				return;
		}

	}

	return;
#endif
}
