
/* This is needed, otherwise the hello.o does not load
   correctly. Needs further investigation, possibly related to the
   additional items in the symbol table.
 */
unsigned char dummy = 0x11;
