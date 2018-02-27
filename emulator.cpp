
#include "poly88.h"
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

int debug = FALSE;

/*
 *
 * keyboard is easy, since it is parallel interface (no setup)
 *  status flag on one port, data on other
 * uart is harder, since it gets setup and control bytes via control port
 *  still has status port, data port
 *
 * timer has divider setup, but otherwise just causes interrupts
 *
 *
 * interrupts in unix come via:
 * SIGIO on file descriptors
 *  -> read all data available?, buffer, then fake interrupts
 *  -> if real one comes while doing fake ones, have to read in new data
 * interval timer
 *  -> set some interrupt pending flag
 *
 */
int main(int argc, char **argv)
{
	try {
		Poly88 poly88;

		poly88.LoadROM("POLY-88-EPROM");
		poly88.Debug(false);

		if(argc == 2)
			poly88.LoadRAM(argv[1]);

		poly88.Command();
	} catch(std::exception &e) {
		std::cerr << "caught exception: " << e.what() << std::endl;
		exit(1);
	}
}
