#include "devices.h"
#include "poly88_devices.h"
#include "i8080_types.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/time.h>
#include <unistd.h>
#include <queue>
#include <stdlib.h>
#include <string.h>

// #define DEBUG

// sleep t microseconds
void
MyIntSleep(int t)
{
	struct timeval to;

	to.tv_sec = t / 1000000;
	to.tv_usec = t % 1000000;
	select(0, NULL, NULL, NULL, &to);
}

KeyBoard::KeyBoard(I8080 &i8080, Devices &devices) :
	Device(i8080, devices)
{
	IRQ = 5;
	inputPort = 0xf8;
	debug = false;
	name = "Keyboard";
}

void KeyBoard::StartUp()
{
}

void KeyBoard::ShutDown()
{
}


uint8_t KeyBoard::Read()
{
	if(keys.size())
	{
		lastKey = keys.front();
		keys.pop();
	}

	// clear interrupt pending when circular buffer is cleared
	if(!keys.size())
		SetInterruptPending(false);

	return lastKey;
}

void KeyBoard::Write(uint8_t data)
{
}

bool KeyBoard::Poll()
{
	SDL_Event event;

	// spin until there are no more events for us to process
	while(SDL_PollEvent(&event))
	{
		switch(event.type) {
			case SDL_QUIT:
				return true;

			case SDL_TEXTINPUT:
//		    fprintf(stderr, "got text '%s'\n", event.text.text);
				{
					char *s = event.text.text;
					while(*s) {
						keys.push(*s);
						s++;
					}
				}
				break;

			case SDL_KEYDOWN:
				switch(event.key.keysym.scancode) {
					case SDL_SCANCODE_LCTRL:
					case SDL_SCANCODE_RCTRL:
					case SDL_SCANCODE_LSHIFT:
					case SDL_SCANCODE_RSHIFT:
					case SDL_SCANCODE_LALT:
					case SDL_SCANCODE_RALT:
					case SDL_SCANCODE_LGUI:
					case SDL_SCANCODE_RGUI:
						continue;
				}
				auto key = event.key.keysym.sym;
				if(key < 0x100 && isalpha(key) && event.key.keysym.mod & KMOD_CTRL) {
					// handle control chars
					keys.push(key & ~0x60);
				} else if(key < 0x20 || key == 0x7f) {
					// handle everything else that is not control, nor is text
					keys.push(key);
				} else if(debug) {
						fprintf(stderr, "keysym.sym = 0x%02x keysym.mod = 0x%02x\n", event.key.keysym.sym, event.key.keysym.mod);
				}
				break;
		}
	}
	if(keys.size())
		SetInterruptPending(true);
	return false;
}

bool KeyBoard::RunEmulatorCommand(const std::vector<std::string> &args)
{
	if (args[0] == "keyboard" || args[0] == "k") {
		for (int i = 1; i < args.size(); i++) {
			bool backslash = false;
			for (auto ch: args[i]) {
				if (ch == '\\') {
					backslash = true;
					continue;
				}
				if (backslash) {
					switch(ch) {
						case 'n':
							ch = '\n';
							break;
						case 'r':
							ch = '\r';
							break;
						case 't':
							ch = '\t';
							break;
						default:
							backslash = false;
							continue;
					}
					backslash = false;
					continue;
				}
				std::cout << "wrote to keyboard: " << ch << std::endl;
				keys.push(ch);
			}
		}
		if(keys.size())
			SetInterruptPending(true);
		return false;
	}

	return false;
}

Timer::Timer(I8080 &i8080, Devices &devices) :
	Device(i8080, devices)
{
	IRQ = 6;
	outputPort = 8;
	name = "Timer";
}

Timer::~Timer()
{
	timerCallback = nullptr;
}

std::function<void()>    Timer::timerCallback;

// Signal handler has to be a static function,
// so it will call a function object bound to
// the class method to set interrupt pending.
void Timer::Interrupt(int signum)
{
	if(timerCallback)
		timerCallback();
}

void Timer::StartUp()
{
	struct itimerval it;

	timerCallback = std::bind(&Timer::SetInterruptPending, this, true);

	signal(SIGALRM, &Timer::Interrupt);
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 16666; /* 1/60 second */
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 16666;    /* 1/60 second */
	if(setitimer(ITIMER_REAL, &it, NULL))
		perror("setitimer call:");
}

void Timer::ShutDown()
{
	struct itimerval it;

	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 0;
	if(setitimer(ITIMER_REAL, &it, NULL))
		perror("setitimer call:");
	signal(SIGALRM, SIG_DFL);

	timerCallback = nullptr;
}

uint8_t Timer::Read()
{
	return 0;
}

void Timer::Write(uint8_t data)
{
	// The ROM interrupt service routing for the timer interrupt
	// writes a byte to port 8, which resets the timer interrupt.
	SetInterruptPending(false);
}

Usart::Usart(I8080 &i8080, Devices &devices) :
	Device(i8080, devices)
{
	inputPort = 0x00;
	outputPort = 0x00;
	IRQ = 4;
	name = "USART";
	debug = false;
}

void Usart::StartUp() { }

void Usart::ShutDown() { }

uint8_t Usart::Read()
{
	uint8_t ch = 0;
	if(usartFile)
		ch = usartFile->Read();

	// Always set to false - poll will turn it back on if need be -
	// this allows rate limiting to work
	SetInterruptPending(false);

	if(debug)
		std::cout << "Usart::Read() returns: " << util::hex(2) << (uint16_t) ch << std::endl;
	return ch;
#if 0
	unsigned char buf;
#ifdef DEBUG_USART_INTERRUPT
		std::cerr << "in usart_input:" << std::endl;
#endif
	if(port==0 && usart_input_file!=NULL)
	{
#ifdef DEBUG_USART_INTERRUPT
		std::cerr << "in usart_input(" << util::hex(2) << port << ") = ";
#endif
		buf = fgetc(usart_input_file);
		if(buf == EOF)
		{
			fclose(usart_input_file);
			usart_input_file = NULL;
#ifdef DEBUG_USART_INTERRUPT
			std::cerr << "finished reading file '" << usart_input_file_name << "'" << std::endl;
#endif
			*usart_flag_2 = false;
			return 0;
		}
#ifdef DEBUG_USART_INTERRUPT
		std::cout << util::hex(2) << buf << std::endl;
#endif
		return buf;
	}
	if(port==1)
	{
		// to ensure we return not ready sometimes:
		static bool flipflop = false;
		// reading status port.  need to indicate that everything is ready
		byte_t status = 0;

		flipflop = !flipflop;
		if(flipflop) return status;
		if(usart_input_file!=NULL && !feof(usart_input_file)) status |= 2;
		if(usart_output_file!=NULL) status |= 4;    // XXX check these bits
#ifdef DEBUG_USART_INTERRUPT
		if(status!=0)
			std::cerr << "in usart_input(" << util::hex(2) << port << ") = " << status << std::endl;
#endif
		return status;
	}
#ifdef DEBUG_USART_INTERRUPT
	std::cerr << "in usart_input(" << util::hex(2) << port << ") = " << 0 << std::endl;
#endif
	return 0;
	#endif
}

void Usart::Write(uint8_t data)
{
	std::cerr << "usart output byte: " << util::hex(2) << (uint16_t) data << std::endl;
	if(usartFile)
		usartFile->Write(data);
	SetInterruptPending(false);
#if 0
#ifdef DEBUG_USART_INTERRUPT
//	    std::cerr << "usart_output(port:" << util::hex(2) << static_cast<uint16_t>(port) << ", data:" << static_cast<uint16_t>(val) << ")" << std::endl;
	std::cerr << "usart_output(port:";
	std::cerr << util::hex(2) << static_cast<uint16_t>(port);
	std::cerr << ", data:";
	std::cerr << util::hex(2) << static_cast<uint16_t>(val);
	std::cerr << ")" << std::endl;
#endif
	if(port==1)
	{
		if(val == 0x96)	// input
		{
			if(usart_input_file)
				fclose(usart_input_file);
			usart_input_file = fopen(usart_input_file_name,"r");
		}
		else if(val == 0x21) // output
		{
			usart_output_file = fopen(usart_output_file_name,"w");
		}
		else if(val == 0)	// close input and/or output
		{
			if(usart_output_file)
			{
#ifdef DEBUG_USART_INTERRUPT
				std::cerr << "closing output file '" << usart_output_file_name << "'" << std::endl;
#endif
				fclose(usart_output_file);
				usart_output_file = NULL;
			}
			if(usart_input_file)
			{
#ifdef DEBUG_USART_INTERRUPT
				std::cerr << "closing input file '" << usart_input_file_name << "'" << std::endl;
#endif
				fclose(usart_input_file);
				usart_input_file = NULL;
			}
		}
	}
	else if(port == 0 && usart_output_file!=NULL)	// output to tape
	{
		fputc(val, usart_output_file);
	}
	return;
#endif
}


UsartControl::UsartControl(I8080 &i8080, Devices &devices, std::shared_ptr<Usart> usart) :
	usart(usart),
	Device(i8080, devices)
{
	inputPort = 0x01;
	outputPort = 0x01;
	tapeRunning = false;
}

void UsartControl::StartUp() { }

void UsartControl::ShutDown() { }

uint8_t UsartControl::Read()
{
	// This prevents us from returning ready character immediately, as this
	// crashes the ROM monitor
	static auto lastStatus = false;
	lastStatus = !lastStatus;
	if(lastStatus)
	{
		return 0;
	}

	byte_t status = 0;
	if(usart->usartFile && usart->usartFile->Ready())
		switch(usart->usartFile->GetState()) {
			case IUsartFile::INPUT:
				status |= 0x02;
				break;
			case IUsartFile::OUTPUT:
				status |= 0x01;		//doc said 0x04?
				break;
		}

	return status;
}

//
// Split the string input into words delimited by the character
// delimiter.  For a given number of input delimiters, result.size()
// will not change, regardless of the data in between the delimiters.
//
// Refactor this to pre-allocate the word that we place data into,
// then we have minimal data copy.
//
int Tokenize(std::vector<std::string> &result, const char *input, char delimiter)
{
    if(*input=='\0') {
        result.clear();
        return 0;
    }

    size_t wordCount = 1;

    // since input is non-empty, we know we will have at least
    // one word, so we allocate it here, and begin to fill it in
    if(result.size()<wordCount) result.resize(1);
    else result[0].clear();

    std::string *word = &result[0];

    while(*input) {
        if(*input==delimiter) {
            // we got a delimeter, and since an empty word following
            // a delimeter still counts as a word, we allocate it here
            wordCount++;
            if(result.size()<wordCount) result.resize(wordCount);
            else {
                result[wordCount-1].clear();
            }
            word = &result[wordCount-1];
        } else {
            // save the char in this word
            word->push_back(*input);
        }
        input++;
    }

    if(wordCount < result.size()) result.resize(wordCount);   // potentially truncate to wordCount elements

    return result.size();
}

std::shared_ptr<IUsartFile> OpenTapeFile(std::string filename)
{
	auto isHexDigits = [](const std::string &number, int n)
	{
		if(number.size() != n) return false;
		for(auto ch : number)
			if(!isxdigit(ch)) return false;
		return true;
	};

	std::vector<std::string> components;

	Tokenize(components, filename.c_str(), '/');

	auto leaf = components.back();

	Tokenize(components, leaf.c_str(), '.');

	auto baseName = components[0];

	std::string loadAddress;
	std::string startAddress;
	std::string suffix;

	if(components.size() > 2 && isHexDigits(components[1], 4))
		loadAddress = components[1];

	if(components.size() > 3 && isHexDigits(components[2], 4))
		startAddress = components[2];

	if(components.size() > 1)
		suffix = components.back();

	// find out of the file is: image, tape, hex records

	// XXX finish the tokenizing stuff above
	return std::make_shared<UsartInputFile>(filename);
}

bool UsartControl::RunEmulatorCommand(const std::vector<std::string> &args)
{
	if (args.size() == 1 && args[0] == "tape") {
		// dump queued up read and write files
		std::cout << "tape read queue has " << readFiles.size() << " files." << std::endl;
		std::cout << "tape write queue has " << writeFiles.size() << " files." << std::endl;
		return false;
	}
	if (args.size() == 2 && args[0] == "tape" && args[1] == "clear") {
		readFiles = {};
		writeFiles = {};
		return false;
	}
	if (args.size() == 3 && args[0] == "tape" && args[1] == "read") {
		readFiles.push(args[2]);
		return false;
	}
	if (args.size() == 3 && args[0] == "tape" && args[1] == "write") {
		writeFiles.push(args[2]);
		return false;
	}

	if (args[0] == "tape") {
		std::cerr << "unknown command." << std::endl;
		return false;
	}

	return false;
}

void UsartControl::Write(uint8_t data)
{
	if(debug)
		std::cerr << "Usart control write: " << util::hex(2) << (uint16_t) data << std::endl;
	try {
		if(data == 0x96) {
			if(!usart->usartFile) {
				std::string filename;

				if (readFiles.empty()) {
					std::cout << "starting the mag tape for read!" << std::endl;
					std::cout << "enter a filename here!!!!" << std::endl;
					std::cin >> filename;
				} else {
					filename = readFiles.front();
					readFiles.pop();
					std::cout << "opening pre-queued tape file for read: " << filename << std::endl;
				}
				SetUsartFile(std::make_shared<UsartInputFile>(filename));
				// polling rate limits.
				// here we want interrupts, because input is interrupt driven
				usart->SetInterruptPending(true);
				tapeRunning = true;
			}
		} else if(data == 0x26 || data == 0x21) {
			if(!usart->usartFile) {
				std::string filename;
				if (readFiles.empty()) {
					std::cout << "starting the mag tape for write!" << std::endl;
					std::cout << "enter a filename here!!!!" << std::endl;
					std::cin >> filename;
				} else {
					filename = writeFiles.front();
					writeFiles.pop();
					std::cout << "opening pre-queued tape file for write: " << filename << std::endl;
				}
				SetUsartFile(std::make_shared<UsartOutputFile>(filename));
				usart->SetInterruptPending(true);
				tapeRunning = true;
			}
		} else if(data == 0x00) {
			if(usart->usartFile) {
				if(usart->usartFile->GetState() == IUsartFile::INPUT) {
					std::cout << "stop the mag tape!" << std::endl;
					SetUsartFile(nullptr);
					usart->SetInterruptPending(false);
				}

				// This lets Usart::Poll close the output tape file
				// because basic is shutting off the tape device every record.
				tapeRunning = false;
				tapeTimeout = time(nullptr);
			}
		}
	}
	catch (std::exception &e) {
		std::cerr << "Tape operation failed: " << e.what() << std::endl;
	}
}

void UsartControl::Poll()
{
	static struct timespec t1 = {0,0};

	if(usart->usartFile && usart->usartFile->Ready())
	{
		if(usart->usartFile->GetState() == IUsartFile::OUTPUT &&
			!tapeRunning &&
			tapeTimeout + 3 < time(nullptr)) {
			std::cerr << "closing output file." << std::endl;
			SetUsartFile(nullptr);
			usart->SetInterruptPending(false);
			return;
		}

		struct timespec t2;
		clock_gettime(CLOCK_REALTIME, &t2);

		double t1F = t1.tv_sec + (t1.tv_nsec / 1000000000.0);
		double t2F = t2.tv_sec + (t2.tv_nsec / 1000000000.0);

		// ~ 1000 characters per second
		if(t2F - t1F > .0005)
		{
			t1 = t2;
			usart->SetInterruptPending(true);
		}
	}
}

BaudRateGenerator::BaudRateGenerator(I8080 &i8080, Devices &devices) :
	Device(i8080, devices)
{
	inputPort = 0x04;
	outputPort = 0x04;
}

void BaudRateGenerator::StartUp() { }

void BaudRateGenerator::ShutDown() { }

uint8_t BaudRateGenerator::Read() { return 0; }

void BaudRateGenerator::Write(uint8_t data) { }

void Command(const char *msg)
{
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
