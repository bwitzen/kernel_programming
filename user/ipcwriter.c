/* Test if ipc works */

#include <inc/lib.h>

void umain(int argc, char **argv)
{
    envid_t parent_id = thisenv->env_id - 1;

    cprintf("writer id: %d sending to: %d\n", thisenv->env_id, parent_id);

    int test_val = 0xdead;
    sys_yield();
    sys_ipv_send(parent_id, &test_val, sizeof(int));
    
}