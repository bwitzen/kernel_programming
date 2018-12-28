/* For the ASLR bonus, tests relocations. */

#include <inc/lib.h>

char *foo = "foo";

void umain(int argc, char **argv) {
    cprintf("foo: %p\n", foo); /* invalid ptr if foo global is not relocated */
    cprintf("&sys_cputs: %p\n", &sys_cputs); /* also invalid */
    panic("die"); /* fails when binaryname global is not relocated properly */
}

