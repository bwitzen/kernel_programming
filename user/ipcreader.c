/* Test if ipc reader works */

#include <inc/lib.h>

void umain(int argc, char **argv)
{
    cprintf("ENV ID of the reader is: %d\n", thisenv->env_id);

    int test_val = 0;
    uint32_t size;
    envid_t recv_from = sys_ipc_recv(&test_val, &size);
    cprintf("test val: %p\n", test_val);
    assert(test_val == 0xdead);
}