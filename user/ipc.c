/* Test if ipc works */

#include <inc/lib.h>

void umain(int argc, char **argv)
{
    envid_t parent_id = thisenv->env_id;
    envid_t child_id;

    child_id = fork();
    if (child_id < 0)
        panic("fork went wrong\n");
    
    if (child_id == 0) {
        /* Child */
        child_id = thisenv->env_id;
        int test_val = 0xdead;
        sys_yield();
        sys_ipv_send(parent_id, &test_val, sizeof(int));
    } else {
        /* Parent */
        int test_val;
        uint32_t size;
        envid_t recv_from = sys_ipc_recv(&test_val, &size);
        assert(test_val == 0xdead);
    }
}