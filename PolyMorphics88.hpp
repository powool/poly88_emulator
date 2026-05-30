#pragma once
#include <atomic>
#include <algorithm>
#include <thread>
#include "EmulatorInterface.h"
#include "FileDialogBridge.hpp"
#include "Instructions.h"
#include "SymbolTable.hpp"
#include "poly88.h"

class PolyMorphics88 : public EmulatorInterface {
	std::atomic<int> cpuSpeed;
	std::atomic<int> cpuSleep;
	Poly88 poly88;
	std::shared_ptr<FileDialogBridge> fileDialogBridge;
	std::shared_ptr<SymbolTable> symbolTable;
	uint64_t machineCycle = 0;
	std::thread executionThread;
	std::atomic<bool> requestThreadExit = false;
	std::atomic<bool> running = false;
	int CpuSpeedToMicrosecondsSleep(int cpuSpeed) {
		// 100 -> 0 (microseconds)
		// 0 -> 1,000,000
		return 1000000 - (cpuSpeed * 10000);
	}
	void ExecutionThread() {
		int instructionIndex = 0;
		while(!requestThreadExit) {
			if (running) {
#if 0
				if (PC() > 0x400) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					std::cout << DumpState(instructionIndex++) << std::endl;
				}
#endif
				// let poly88 do any rate limiting on speed
				poly88.Run(machineCycle, true);
//				std::this_thread::sleep_for(std::chrono::microseconds(cpuSleep.load()));
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
	}
    public:
	PolyMorphics88(std::shared_ptr<FileDialogBridge> fileDialogBridge,
			MemoryInterfacePtr memory,
			std::shared_ptr<SymbolTable> symbolTable
		) :
		fileDialogBridge(fileDialogBridge),
		symbolTable(symbolTable),
		poly88(fileDialogBridge, memory)
	{
		poly88.Reset();
		poly88.InterruptEnable(false);
		executionThread = std::thread(&PolyMorphics88::ExecutionThread, this);
		cpuSpeed.store(100);
		cpuSleep.store(2);
	}

	~PolyMorphics88() {
		requestThreadExit = true;
		executionThread.join();
	}

	uint8_t GetMemoryByte(uint16_t address) const override {
		return poly88.ReadByte(address);
	}

	uint16_t GetMemoryInt(uint16_t address) const override {
		return poly88.ReadByte(address) | (poly88.ReadByte(address+1) << 8);
	}

	void PutMemoryByte(uint16_t address, uint8_t byte) {
		poly88.WriteByte(address, byte);
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
		if (running) {
			poly88.EnableTimer();
		} else {
			poly88.DisableTimer();
		}
	}

	void SetCpuSpeed(int percentage) override {
		cpuSpeed = std::clamp(percentage, 0, 100);
		cpuSleep.store(CpuSpeedToMicrosecondsSleep(cpuSpeed));
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
		return GetMemoryInt(HL());
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

	uint16_t BC(uint16_t bc) override {
		return poly88.BC(bc);
	}

	uint16_t DE() const override {
		return poly88.DE();
	}

	uint16_t DE(uint16_t de) override {
		return poly88.DE(de);
	}

	uint16_t HL() const override {
		return poly88.HL();
	}

	uint16_t HL(uint16_t hl) override {
		return poly88.HL(hl);
	}

	uint16_t SP() const override {
		return poly88.SP();
	}

	uint16_t SP(uint16_t sp) override {
		return poly88.SP(sp);
	}

	uint16_t PC() const override {
		return poly88.PC();
	}

	uint16_t PC(uint16_t pc) override {
		return poly88.PC(pc);
	}

	std::pair<std::string, uint16_t> Disassemble(uint16_t pc) override {
		return std::make_pair(poly88.Disassemble(pc), pc);
	}

	std::string DumpState(int instructionIndex) override {

		uint16_t starsp = GetMemoryInt(SP());
		uint8_t starpc = GetMemoryByte(PC());
		uint8_t starpc1 = GetMemoryByte(PC()+1);
		uint8_t starpc2 = GetMemoryByte(PC()+2);
		auto disassembly = instructions8085[starpc].AsString(
			PC(),
			starpc1,
			starpc2,
			*symbolTable,
			false);

		return std::format("{:08d} a:{:02X} m:{:02X} bc:{:04X} de:{:04X} hl:{:04X} sp:{:04X} *sp:{:04X}\t{:36s}",
			instructionIndex,
			A(),
			M(),
			BC(),
			DE(),
			HL(),
			SP(),
			starsp,
			disassembly);
	}

	void KeyPress(uint8_t ch) override {
		poly88.KeyPress(ch);
	}

	bool Running() const override { return running; }
	void ToggleRunning() override { running = !running; }
	void SetCPUSpeed(int hz) override { /* not implemented */ }
};
