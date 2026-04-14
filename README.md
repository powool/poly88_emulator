# poly88_emulator
Emulator for a 1976 Polymorphics Systems 88 microcomputer

This directory contains an emulator for a Polymorphics Systems Poly-88
microcomputer, circa 1976.  I wrote it for my amusement, in the late
90's and early 2000's, to learn some things about C++, as well as
taking a step towards preserving my experience with a Poly-88, from which
I learned just about everything important about computing.

Paul Anderson

---------------------------

Contents:
---------

~~~~~~~~
README.md		this file
CMakeLists.txt	CMakefile for this program
devices.cpp	methods for handling generic, simple 8080 devices
devices.h	class definition for generic devices
i8080.cpp	methods for 8080 emulator class object
i8080.h		class definition for 8080 emulator
poly88.cpp	main program for poly-88 specific emulator
poly88_devices.cpp	poly 88 specific i/o devices (keyboard, timer - tape)
poly88_devices.h	definitions of functions in poly88_devices.cc
~~~~~~~~

Building:
---------
	cmake -DCMAKE_BUILD_TYPE=Debug
	make

Running:
--------

    from this directory, type:
    ./Debug/poly88

To use:
    Initially, the emulator is not running. Start it by clicking the
    "Stopped" button to start.

    I usually hit caps lock to ensure all chacters are entered in
    upper case, since almost all commands are expected to be
    upper case.

    Comes up initially with a blank screen, waiting for a 'P' or a 'B'
    character to be typed for tape loading.

	When you type 'P', a file picking dialog box will open. It expected
    to open a file with a .CAS suffix, which represents Poly-88
    tape format binary files.

    Many, but not all programs, will automatically start, usually
    at address 2000H.

    To get to the poly-88 debugger, type ^Z, you are now in the front
    panel display:

~~~~~~~~
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
~~~~~~~~


    The upper panel shows registers, their values, then 8 bytes of memory
    that they point to.  The AF is the A register and the flags, which
    are spelled out just to the right of them.  In this example, the
    minus flag is set (M).

    The lower panel shows RAM, where the '>' character points to the
    current memory location (for memory updates, and a few other operations).

    To look at a memory location:

	L0C80
	(follow with a non-hex char to move window to 0x0c80).

    To move forward one byte, type space.

    To move backwards one byte, type backspace.

    To store a byte:
	CD   (or any other two hex digits)
	(follow with a non-hex char, usually a space)

    To store a 16 bit word (little endian):
	J0C80
	(follow with a non-hex char, usually a space)

    To set a register:
	SA	points to stack frame storage for AF
	SB	point to BC
	SD	point to DE
	SH	point to HL
	SP	point to PC

    To resume execution at current PC, type G.

    To enter and run a program that reads and writes characters from the
    keyboard to the screen, do the following:

	LC80
	CD 20 0C CD 24 0C C3 80 0C
	SPJC80
	G

	type characters for your amusement.  To return to the debugger,
	type ^Z.
