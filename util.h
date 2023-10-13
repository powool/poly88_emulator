#pragma once

#include <iomanip>

namespace util {

struct hex
{
	hex(int _width = 2, char _fill = '0') : fill(_fill), width(_width) {;}

	char fill;
	int width;
};

struct reset
{
};

}

inline std::ostream& operator<<(std::ostream &stream, const util::hex &a)
{
	stream.setf(std::ios_base::hex, std::ios::basefield);
	stream.fill(a.fill);
	stream.width(a.width);
	return stream;
}

inline std::ostream& operator<<(std::ostream &stream, const util::reset &a)
{
	stream.fill(' ');
	stream.width(0);
	stream.setf(std::ios_base::dec, std::ios::basefield);
	return stream;
}
