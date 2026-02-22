#include "Keyboard.h"

KeyBoard::KeyBoard()
{
}

bool KeyBoard::DataAvailable(uint16_t size)
{
	return keys.size() != 0;
}

uint8_t KeyBoard::ReadByte()
{
	if(keys.size())
	{
		lastKey = keys.front();
		keys.pop();
	}

	return lastKey;
}

void KeyBoard::WriteByte(uint8_t byte)
{
	keys.push(byte);
}
