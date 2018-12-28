#include <inc/lib.h>

void umain(int argc, char **argv)
{
    //cprintf("Start waiting on %08x\n", thisenv->env_id + 1);
    int result = sys_wait(thisenv->env_id + 1);
    if(result == 0) {
        cprintf("Done waiting\n");
    } else {
        cprintf("Env does not exist\n");
    }
}