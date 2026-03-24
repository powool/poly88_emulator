// source: https://gist.github.com/raxoft/b90b7844341fe79744fe

// The entry point prototypes. You will likely want to put them to a header file instead.

const char * z80_disassemble( const unsigned char * & in, char * args, uint address = 0, bool hexadecimal = false, uint * target_address = nullptr ) ;
uint z80_disassemble_size( const unsigned char * in ) ;
