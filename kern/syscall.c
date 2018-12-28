/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/vma.h>

/*
 * Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors.
 */
static void sys_cputs(const char *s, size_t len)
{
    /* Check that the user has permission to read memory [s, s+len).
     * Destroy the environment if not. */

    /* LAB 3: Your code here. */
    user_mem_assert(curenv, s, len, PTE_U);

    /* Print the string supplied by the user. */
    cprintf("%.*s", len, s);
}

/*
 * Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting.
 */
static int sys_cgetc(void)
{
    return cons_getc();
}

/* Returns the current environment's envid. */
static envid_t sys_getenvid(void)
{
    return curenv->env_id;
}

/*
 * Destroy a given environment (possibly the currently running environment).
 *
 * Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 */
static int sys_env_destroy(envid_t envid)
{
    int r;
    struct env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
        return r;
    if (e == curenv)
        cprintf("[%08x] exiting gracefully\n", curenv->env_id);
    else
        cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
    env_destroy(e);
    return 0;
}

/*
 * Deschedule current environment and pick a different one to run.
 */
static void sys_yield(void)
{
    sched_yield(true);
}

static int sys_wait(envid_t envid)
{
    struct env *wait;
    int result = envid2env(envid, &wait, 0);
    if(result != 0) return -1;
    cprintf("wait on id: %d status %d\n", wait->env_id, wait->env_status);
    curenv->env_wait_env = envid;
    return 0;
}

static int sys_fork(void)
{
    // attempt to create new environment
    struct env *new = NULL;
    if (env_alloc(&new, curenv->env_id))
        return -1;

    // copy parent VMA into child VMA
    vma_init(new);
    memcpy(new->env_vmas, curenv->env_vmas, VMA_LENGTH * sizeof(struct vma));

    // copy all VMA-backed pages into child pgdir
    for (size_t i = 0; i < VMA_LENGTH; i++) {
        if (curenv->env_vmas[i].type == VMA_UNUSED)
            continue;

        // copy page by page
        void *start = ROUNDDOWN(curenv->env_vmas[i].va, PGSIZE);
        void *end = ROUNDUP(curenv->env_vmas[i].va + curenv->env_vmas[i].len, PGSIZE);

        for (void *addr = start; addr < end; addr += PGSIZE) {
            pte_t *pte = NULL;
            struct page_info *pp = page_lookup(curenv->env_pgdir, addr, &pte);
            if (!pp) continue;

            // copy pages copied from parent to child as COW
            int perm = curenv->env_vmas[i].perm & ~PTE_W;
            page_insert(new->env_pgdir, pp, addr, perm);

            // also mark parent's own pages as COW
            *pte &= ~PTE_W;
            tlb_invalidate(curenv->env_pgdir, addr);
        }
    }

    // copy parent registers into child registers
    memcpy(&new->env_tf, &curenv->env_tf, sizeof(struct trapframe));

    // syscall return value for child
    new->env_tf.tf_regs.reg_eax = 0;

    // syscall return value for parent
    return new->env_id;
}

/*
 * Creates a new anonymous mapping somewhere in the virtual address space.
 *
 * Supported flags:
 *     MAP_POPULATE
 *
 * Returns the address to the start of the new mapping, on success,
 * or -1 if request could not be satisfied.
 */
static void *sys_vma_create(size_t size, int perm, int flags, int key)
{
    // freespace finder
    char *mem = vma_find_mem(curenv, size);
    if (!mem) return (void *) -1;

    // insert the VMA block
    struct vma *v = vma_new(curenv, mem, size, perm, NULL, NULL);
    if (v < 0) return (void *) -1;
    v->shmem_key = key;

    // populate by triggering a page fault on each created VMA page
    // the compiler doesn't seem to particularly like this, though
    // it forces the { } even though they aren't needed here?
    /*
    if (flags & MAP_POPULATE) {
        for (int i = 0; i < size; i += PGSIZE) {
            volatile int z = *(mem + i);
        }
    }
    */

    return mem;
}

/*
 * Unmaps the specified range of memory starting at
 * virtual address 'va', 'size' bytes long.
 */
static int sys_vma_destroy(void *va, size_t size)
{
   vma_rmv(curenv, va, size, VMA_DESTROY_PHYS);
   return 0;
}

static envid_t sys_ipc_recv(void *dst, size_t *sz) {
    panic("Not implemented yet\n");
    return -1;
}

static int sys_ipc_send(envid_t envid, void *src, size_t sz) {
    panic("Not implemented yet\n");
    return 0;
}

/*
 * Allocates shared memory locked with a key. Any process who knows the key can
 * attach and use the memory. Key 0 is not allowed. A key currently in use is
 * also not allowed.
 */
static void *sys_shmem_alloc(size_t sz, int key) {
    // reject zero key
    if (key == 0)
        return NULL;

    // reject duplicate key
    return sys_vma_create(sz, PTE_W, 0, key);
}

/*
 * Attach to a shared memory location.
 */
static void *sys_shmem_attach(int key) {
    // search in each environment's VMA list to find the shared memory
    for (size_t e = 0; e < NENV; e++) {
        for (size_t v = 0; v < VMA_LENGTH; v++) {
            // shared memory found!
            if (envs[e].env_vmas[v].shmem_key == key) {
                // add to my VMA
                vma_new(curenv, envs[e].env_vmas[v].va, envs[e].env_vmas[v].len,
                        envs[e].env_vmas[v].perm, NULL, NULL);

                // add a link to each page
                void *start = ROUNDDOWN(curenv->env_vmas[v].va, PGSIZE);
                void *end = ROUNDUP(curenv->env_vmas[v].va + curenv->env_vmas[v].len, PGSIZE);

                for (void *addr = start; addr < end; addr += PGSIZE) {
                    pte_t *pte = NULL;
                    struct page_info *pp = page_lookup(envs[e].env_pgdir, addr, &pte);
                    if (!pp) continue;
                    page_insert(curenv->env_pgdir, pp, addr, envs[e].env_vmas[v].perm);
                }

                // return the address of the shared VMA
                return envs[e].env_vmas[v].va;
            }
        }
    }

    // memory with this key not found
    return NULL;
}

/* Dispatches to the correct kernel function, passing the arguments. */
int32_t syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3,
        uint32_t a4, uint32_t a5)
{
    switch (syscallno) {
        case SYS_cputs:
            sys_cputs((char *)a1, a2);
            return 0;
        case SYS_cgetc:
            return sys_cgetc();
        case SYS_getenvid:
            return sys_getenvid();
        case SYS_env_destroy:
            return sys_env_destroy(a1);
        case SYS_vma_create:
            return (int) sys_vma_create(a1, a2, a3, 0);
        case SYS_vma_destroy:
            return (int) sys_vma_destroy((void *) a1, a2);
        case SYS_wait:
            return sys_wait(a1);
        case SYS_yield:
            sys_yield();
            return 0;
        case SYS_fork:
            return sys_fork();
        case SYS_ipc_recv:
            return sys_ipc_recv((void *)a1, (size_t *)a2);
        case SYS_ipc_send:
            return sys_ipc_send(a1, (void *)a2, a3);
        case SYS_shmem_alloc:
            return (uint32_t) sys_shmem_alloc(a1, a2);
        case SYS_shmem_attach:
            return (uint32_t) sys_shmem_attach(a1);
        default:
            return -E_NO_SYS;
    }
}
