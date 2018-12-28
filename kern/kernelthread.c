/**
 * Notes: library shares a lot of functionality with env
 * Whereas envs are created with icodes, kernelthreads are created with a C
 * function
 *
 * env_run() can be used to run a kernelthread
 * env_free() can be used to destroy a kernelthread
 */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/monitor.h>
#include <kern/vma.h>
#include <kern/spinlock.h>
#include <kern/sched.h>
#include <kern/kernelthread.h>

/**
 * Spawns a new kernel thread. This function is very similar to env_create, but
 * it does not load in some icode and it does not prepare the dummy VMAs for
 * lab4. It also sets the structure different (disable interupts, etc...)
 */
void kernelthread_create() {
    // allocate environment
    struct env *e;
    if (env_alloc(&e, 0) < 0)
        panic("env_create: could not allocate an environment\n");

    cprintf("\033[0;31m kernel thread created id=%x \033[0m\n", e->env_id);

    // initialize vma
    vma_init(e);

    // generate kernel stack
    struct page_info *pp = page_alloc(ALLOC_ZERO);
    if (!pp) panic("page_alloc");

    // set values
    e->env_tf.tf_ds = GD_KD;
    e->env_tf.tf_es = GD_KD;
    e->env_tf.tf_cs = GD_KT;
    e->env_tf.tf_ss = GD_KD;
    e->env_tf.tf_esp = (size_t) (page2kva(pp) + PGSIZE);
    void (*sp) (void) = &spinner;   // TODO temp code: get the function pointer to spinner
    e->env_tf.tf_eip = (size_t) sp; // TODO: Point to C-function this kernel thread will do
    e->env_tf.tf_eflags &= ~FL_IF;  // disable interupts

    // set environment type
    e->env_type = ENV_TYPE_KERNELTHREAD;
    e->env_pgdir = kern_pgdir;
}

/**
 * Forces the kernelthread to yield.
 */
void kernelthread_yield() {
    void *addr = __builtin_return_address(0);

    cprintf("\033[0;31m kernel thread yielding \033[0m\n");

    if (curenv->env_type != ENV_TYPE_KERNELTHREAD)
        panic("Called kernelthread_yield, but curenv is not a kernel thread.");

    curenv->env_status = ENV_RUNNABLE;

    // push our own trapframe
    // reverse order of appearance in trap.h
    __asm __volatile(
        "pushl %%ss\n"      // ss
        "pushl %%esp\n"     // esp
        "pushfl\n"          // eflags
        "pushl %%cs\n"      // cs
        "pushl %0\n"        // eip -- __builtin_return_address
        "pushl $0\n"        // err
        "pushl $0\n"        // trapno
        "pushl %%ds\n"      // ds
        "pushl %%es\n"      // es
        "pushal\n"          // registers
        : : "m" (addr) : "memory"
    );

    // goto normal scheduler
    cprintf("\033[0;31m now going to sched_yield(true) \033[0m\n");
    sched_yield(true);
}

/**
 * If detecting a swap from ENV_TYPE_USER to ENV_TYPE_KERNELTHREAD, run a
 * special pop_tf function to respect the changes to iret's functioning
 */
void kernelthread_pop_tf(struct trapframe *tf)
{
    cprintf("\033[0;31m kern pop tf \033[0m\n");

    __asm __volatile(
        "movl %0,%%esp\n"
        "popal\n"
        "popl %%es\n"
        "popl %%ds\n"
        "addl $0x10,%%esp\n"  // <- Fixed the alignment issue mentioned in mail
        "popl %%esp\n"        // <- Tried to pop ESP but it doesnt work
        "iret"
        : : "g" (tf) : "memory"
    );

    panic("iret failed");  /* mostly to placate the compiler */
}

/**
 * Simple test function for kernel thread
 */
void spinner() {
    cprintf("\033[0;31m kernel thread running \033[0m\n");
    size_t i = 0;
    while (true) {
        //cprintf("Kernel thread spinning... %d\n", i);
        i++;
        if(i % 150 == 0 && i != 0) {
            kernelthread_yield();
        }
    }
}
