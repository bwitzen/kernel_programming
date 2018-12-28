/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>

envid_t fork(void)
{    
    int retval = sys_fork();
    if (retval == 0)
      thisenv = &envs[ENVX(sys_getenvid())];
    return retval;
}
