/* System call stubs. */

#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/error.h>

static inline int32_t syscall(int num, int check, uint32_t a1, uint32_t a2,
        uint32_t a3, uint32_t a4, uint32_t a5)
{
    int32_t ret;

    /*
     * Generic system call: pass system call number in AX,
     * up to five parameters in DX, CX, BX, DI, SI.
     * Interrupt kernel with T_SYSCALL.
     *
     * The "volatile" tells the assembler not to optimize
     * this instruction away just because we don't use the
     * return value.
     *
     * The last clause tells the assembler that this can
     * potentially change the condition codes and arbitrary
     * memory locations.
     */

    asm volatile("int %1\n"
        : "=a" (ret)
        : "i" (T_SYSCALL),
          "a" (num),
          "d" (a1),
          "c" (a2),
          "b" (a3),
          "D" (a4),
          "S" (a5)
        : "cc", "memory");

    if(check && ret > 0)
        panic("syscall %d returned %d (> 0)", num, ret);

    return ret;
}

void sys_cputs(const char *s, size_t len)
{
    syscall(SYS_cputs, 0, (uint32_t)s, len, 0, 0, 0);
}

int sys_cgetc(void)
{
    return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0);
}

int sys_env_destroy(envid_t envid)
{
    return syscall(SYS_env_destroy, 1, envid, 0, 0, 0, 0);
}

envid_t sys_getenvid(void)
{
     return syscall(SYS_getenvid, 0, 0, 0, 0, 0, 0);
}

void *sys_vma_create(size_t size, int perm, int flags)
{
    return (void *)syscall(SYS_vma_create, 0, size, perm, flags, 0, 0);
}

int sys_vma_destroy(void *va, size_t size)
{
    return syscall(SYS_vma_destroy, 0, (uint32_t) va, size, 0, 0 ,0);
}

void sys_yield(void)
{
    syscall(SYS_yield, 0, 0, 0, 0, 0, 0);
}

int sys_wait(envid_t envid)
{
    int result = syscall(SYS_wait, 0, envid, 0, 0, 0, 0);
    if(result == 0) sys_yield();
    return result;
}

envid_t sys_fork(void)
{
    return syscall(SYS_fork, 0, 0, 0, 0, 0, 0);
}

envid_t sys_ipc_recv(void *dst, size_t *sz)
{
    //while(thisenv->env_msg == NULL) {
    //    cprintf("Looping till message\n");
    //    sys_yield();
    //}
    return syscall(SYS_ipc_recv, 0, (uint32_t)dst, (uint32_t)sz, 0, 0, 0);
}

int sys_ipv_send(envid_t envid, void *src, size_t sz)
{
    return syscall(SYS_ipc_send, 0, envid, (uint32_t)src, sz, 0, 0);
}

void *sys_shmem_alloc(size_t sz, int key) {
    return (void *) syscall(SYS_shmem_alloc, 0, sz, key, 0, 0, 0);
}

void *sys_shmem_attach(int key) {
    return (void *) syscall(SYS_shmem_attach, 0, key, 0, 0, 0, 0);
}
