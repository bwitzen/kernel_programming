/* Writes and then reads from region (at UTEMP) that should not be directly
 * mapped, only on-demand via pagefault/VMA. */

#include <inc/lib.h>
#include <inc/memlayout.h>

void umain(int argc, char **argv)
{
    int *foo_ro = UTEMP;
    int *foo_rw = UTEMP + PGSIZE;
    cprintf(" foo_ro = %p,  foo_rw = %p\n", foo_ro, foo_rw);
    cprintf("*foo_ro = %x, *foo_rw = %x\n", *foo_ro, *foo_rw);
    *foo_rw = 0xcafebabe;
    cprintf("*foo_rw = %x\n", *foo_rw);
    *foo_ro = 0xcafebabe; /* Should pgflt. */
    cprintf("*foo_ro = %x (this should not happen!!)\n", *foo_ro);
}
