# poly88_emulator
Emulator for a 1976 Polymorphics Systems 88 microcomputer

This directory contains an emulator for a Polymorphics Systems Poly-88
microcomputer, circa 1976.  I wrote it for my amusement, in the late
90's and early 2000's, to learn some things about C++, as well as
taking a step towards preserving my experience with a Poly-88, from which
I learned just about everything important about computing.

Paul Anderson

---------------------------
contents:

README.md		this file
CMakeLists.txt	CMakefile for this program
POLY-88-EPROM	1K ROM monitor for poly-88
devices.cpp	methods for handling generic, simple 8080 devices
devices.h	class definition for generic devices
i8080.cpp	methods for 8080 emulator class object
i8080.h		class definition for 8080 emulator
memory.cpp	methods for memory class, including poly-88 specific
		memory mapped screen
memory.h	class definition for memory
poly88.cpp	main program for poly-88 specific emulator
poly88_devices.cpp	poly 88 specific i/o devices (keyboard, timer -
		will eventually have a tape interface)
poly88_devices.h	definitions of functions in poly88_devices.cc

To build:
	cmake -DCMAKE_BUILD_TYPE=Debug
	make

To run:
    from this directory, type:
    ./poly88

    It reads the EPROM from the file POLY-88-EPROM, then executes it.

To use:
    Comes up initially with a blank screen, waiting for a 'P' or a 'B'
    character to be typed for tape loading.

	When you type 'P', the terminal window will ask for a filename, which
	needs to be a Poly-88 cassette image file. On successful load, program
	execution usually begins at hex address 2000.

    To get to the poly-88 debugger, type ^Z, you are now in the front
    panel display:

PC 006F 0C 0C 7E B7 C2 6E 00 35
SP 0FF8 FF 6F 00 00 04 23 0C A4
HL 0C0C 00 00 00 FF 00 97 F8 00
DE 0C2C 78 00 C9 00 00 00 00 00
BC 0000 00 00 00 31 00 10 C3 00
AF FF84  M       ^

FFE0    00 00 00 00 00 00 00 00
FFE8    00 00 00 00 00 00 00 00
FFF0    00 00 00 00 00 00 00 00
FFF8    00 00 00 00 00 00 00 00
0000  > 31 00 10 C3 00 02 E1 E9
0008    F5 C5 D5 E5 2A 10 0C E9
0010    F5 C5 D5 E5 2A 12 0C E9
0018    F5 C5 D5 E5 2A 14 0C E9


    The upper panel shows registers, their values, then 8 bytes of memory
    that they point to.  The AF is the A register and the flags, which
    are spelled out just to the right of them.  In this example, the
    minus flag is set (M).

    The lower panel shows RAM, where the '>' character points to the
    current memory location (for memory updates, and a few other operations).

    To look at a memory location:

	L0c80
	(follow with a non-hex char to move window to 0x0c80).

    To move forward one byte, type space.

    To move backwards one byte, type backspace.

    To store a byte:
	cd
	(follow with a non-hex char, usually a space)

    To store a double type (reverse endian):
	j0c80
	(follow with a non-hex char, usually a space)

    To set a register:
	sa	points to stack frame storage for AF
	sb	point to BC
	sd	point to DE
	sh	point to HL
	sp	point to PC

    To resume execution at current PC, type g.

    To terminate emulator program, type ^A.

    To enter and run a program that reads and writes characters from the
    keyboard to the screen, do the following:

	lc80
	cd 20 0c cd 24 0c c3 80 0c
	spjc80
	g

	type characters for your amusement.  To return to the debugger,
	type ^Z.

Display
-------

The display implementation is intended to exactly replicate what I
saw when I was programming. Because we used a vintage black and
white TV studio monitor for the 1960's, it had very wide scan line
spacing, so I duplicated this by inserting blank lines between the
active rows of pixels representing the characters.
