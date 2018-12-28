#include <inc/lib.h>

void umain(int argc, char **argv)
{
    envid_t id;
    id = fork();

    if (id < 0) {
        panic("Fork failure\n");
    }
    else if (id == 0) {
        char *ourdata = sys_shmem_attach(0x42);
        cprintf("[y] Connected to %p\n", ourdata);
        assert(ourdata[50] == 0x77);
        assert(ourdata[111] == 0x57);
        cprintf("[y] Succesfully read the data placed by x!\n");
        ourdata[50] += 1;
        ourdata[111] += 1;
        cprintf("[y] Done.\n");
    }
    else {
        char *mydata = sys_shmem_alloc(500, 0x42);
        cprintf("[x] Creating shared memory at %p..\n", mydata);
        mydata[50] = 0x77;
        mydata[111] = 0x57;
        sys_wait(id); // wait for remote to utilize the data
        assert(mydata[50] == 0x78);
        assert(mydata[111] == 0x58);
        cprintf("[x] Shared memory was succesfully altered by y!\n");
        cprintf("[x] Done.\n");
    }
}
