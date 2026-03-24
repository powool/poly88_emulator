#pragma once
#include <atomic>
#include <thread>
#include "EmulatorInterface.h"
#include "MediaQueue.hpp"
#include "poly88.h"

class PolyMorphics88 : public EmulatorInterface {
	Poly88 poly88;
	std::shared_ptr<MediaQueue> mediaQueue;
	uint64_t machineCycle = 0;
	std::thread executionThread;
	std::atomic<bool> requestThreadExit = false;
	std::atomic<bool> running = false;
	void ExecutionThread() {
		while(!requestThreadExit) {
			if (running && !poly88.Halt()) {
				// let poly88 do any rate limiting on speed
				poly88.Run(machineCycle, true);
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
	}
    public:
	PolyMorphics88(std::shared_ptr<MediaQueue> mediaQueue) :
		mediaQueue(mediaQueue),
		poly88(mediaQueue)
	{
		poly88.memory.LoadROM("POLY-88-EPROM");
		poly88.Reset();
		poly88.InterruptEnable(false);
		executionThread = std::thread(&PolyMorphics88::ExecutionThread, this);
	}

	~PolyMorphics88() {
		requestThreadExit = true;
		executionThread.join();
	}

	uint8_t GetMemoryByte(uint16_t address) const override {
		return poly88.memory.get_byte(address);
	}

	uint8_t GetMemoryInt(uint16_t address) const override {
		return poly88.memory.get_byte(address) | (poly88.memory.get_byte(address+1) << 8);
	}

	void RunOneInstruction() override {
		poly88.Run(machineCycle, false);
	};

	// Reset PC to 0
	void Reset() override {
		if (!running) poly88.Reset();
	}

	void RunStop(bool runStop) override {
		running = runStop;
	}

	bool Halted() const override {
		return poly88.Halt();
	}

	bool InterruptEnable() const override {
		return poly88.InterruptEnable();
	}

	uint8_t A() const override {
		return poly88.A();
	}

	uint8_t M() const override {
		return poly88.A();
		return 0x11;
	}

	std::string PSW() const override {
		std::string result;
		if (poly88.PSW() & 0x80) result += "N"; else result += " ";
		if (poly88.PSW() & 0x40) result += "Z"; else result += " ";
		if (poly88.PSW() & 0x10) result += "H"; else result += " ";
		if (poly88.PSW() & 0x04) result += "P"; else result += " ";
		if (poly88.PSW() & 0x01) result += "C"; else result += " ";
		return result;
	}

	uint16_t BC() const override {
		return poly88.BC();
	}

	uint16_t DE() const override {
		return poly88.DE();
	}

	uint16_t HL() const override {
		return poly88.HL();
	}

	uint16_t SP() const override {
		return poly88.SP();
	}

	uint16_t PC() const override {
		return poly88.PC();
	}

	void KeyPress(uint8_t ch) override {
		poly88.KeyPress(ch);
	}

	bool Running() const override { return running; }
	void ToggleRunning() override { running = !running; }
	void SetCPUSpeed(int hz) override { /* not implemented */ }
};

