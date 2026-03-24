#pragma once
#include <cstdint>
#include <string>

class EmulatorInterface {
    public:
	virtual uint8_t GetMemoryByte(uint16_t address) const = 0;
	virtual uint8_t GetMemoryInt(uint16_t address) const = 0;

	virtual void RunOneInstruction() = 0;
	virtual void Reset() = 0;
	virtual void RunStop(bool runStop) = 0;

	virtual bool Halted() const = 0;
	virtual bool InterruptEnable() const = 0;

	virtual uint8_t A() const = 0;
	virtual uint8_t M() const = 0;
	virtual std::string PSW() const = 0;
	virtual uint16_t BC() const = 0;
	virtual uint16_t DE() const = 0;
	virtual uint16_t HL() const = 0;
	virtual uint16_t SP() const = 0;
	virtual uint16_t PC() const = 0;
	virtual void KeyPress(uint8_t ch) = 0;
	virtual bool Running() const = 0;
	virtual void ToggleRunning() = 0;
	virtual void SetCPUSpeed(int hz) = 0;
};
