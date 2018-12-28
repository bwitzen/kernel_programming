/*
 * Called from entry.S to get us going.
 * entry.S already took care of defining envs, pages, uvpd, and uvpt.
 */

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct env *thisenv;
const char *binaryname = "<unknown>";

void libmain(int argc, char **argv)
{
    /* Set thisenv to point at our env structure in envs[].
     * LAB 3: Your code here. */
    thisenv = &envs[ENVX(sys_getenvid())];

    /* Save the name of the program so that panic() can use it. */
    if (argc > 0)
        binaryname = argv[0];

    /* Call user main routine. */
    umain(argc, argv);

    /* Exit gracefully. */
    exit();
}
