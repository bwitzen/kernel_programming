#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/vma.h>
#include <kern/sched.h>

static struct taskstate ts;

/*
 * For debugging, so print_trapframe can distinguish between printing a saved
 * trapframe and printing the current trapframe and print some additional
 * information in the latter case.
 */
static struct trapframe *last_tf;

/*
 * Interrupt descriptor table.  (Must be built at run time because shifted
 * function addresses can't be represented in relocation records.)
 */
struct gatedesc idt[256] = { { 0 } };
struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(excnames[0]))
        return excnames[trapno];
    if (trapno == T_SYSCALL)
        return "System call";
    return "(unknown trap)";
}

// C handles for the traphandlers set in trapentry.S
// used by trap_init()
void traphandler_de();
void traphandler_db();
void traphandler_bp();
void traphandler_of();
void traphandler_br();
void traphandler_ud();
void traphandler_nm();
void traphandler_df();
void traphandler_ts();
void traphandler_np();
void traphandler_ss();
void traphandler_gp();
void traphandler_pf();
void traphandler_mf();
void traphandler_ac();
void traphandler_mc();
void traphandler_xm();
void traphandler_ve();
void traphandler_syscall();
void irq_timer();
void irq_kbd();
void irq_serial();
void irq_spurious();
void irq_ide();
void irq_error();

void trap_init(void)
{
    extern struct segdesc gdt[];

    // inc/mmu.h
    SETGATE(idt[0], 0, GD_KT, traphandler_de, 0);
    SETGATE(idt[1], 0, GD_KT, traphandler_db, 0);
    // 2 is not used
    SETGATE(idt[3], 0, GD_KT, traphandler_bp, 3);
    SETGATE(idt[4], 0, GD_KT, traphandler_of, 0);
    SETGATE(idt[5], 0, GD_KT, traphandler_br, 0);
    SETGATE(idt[6], 0, GD_KT, traphandler_ud, 0);
    SETGATE(idt[7], 0, GD_KT, traphandler_nm, 0);
    SETGATE(idt[8], 0, GD_KT, traphandler_df, 0);
    // 9 is not used
    SETGATE(idt[10], 0, GD_KT, traphandler_ts, 0);
    SETGATE(idt[11], 0, GD_KT, traphandler_np, 0);
    SETGATE(idt[12], 0, GD_KT, traphandler_ss, 0);
    SETGATE(idt[13], 0, GD_KT, traphandler_gp, 0);
    SETGATE(idt[14], 0, GD_KT, traphandler_pf, 0);
    // 15 is not used
    SETGATE(idt[16], 0, GD_KT, traphandler_mf, 0);
    SETGATE(idt[17], 0, GD_KT, traphandler_ac, 0);
    SETGATE(idt[18], 0, GD_KT, traphandler_mc, 0);
    SETGATE(idt[19], 0, GD_KT, traphandler_xm, 0);
    SETGATE(idt[20], 0, GD_KT, traphandler_ve, 0);
    // 16..32 reserved
    // 33..47 ?
    SETGATE(idt[48], 0, GD_KT, traphandler_syscall, 3);

    SETGATE(idt[IRQ_OFFSET + IRQ_TIMER], 0, GD_KT, irq_timer, 0);
    SETGATE(idt[IRQ_OFFSET + IRQ_KBD], 0, GD_KT, irq_kbd, 0);
    SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL], 0, GD_KT, irq_serial, 0);
    SETGATE(idt[IRQ_OFFSET + IRQ_SPURIOUS], 0, GD_KT, irq_spurious, 0);
    SETGATE(idt[IRQ_OFFSET + IRQ_IDE], 0, GD_KT, irq_ide, 0);
    SETGATE(idt[IRQ_OFFSET + IRQ_ERROR], 0, GD_KT, irq_error, 0);


    /* Per-CPU setup */
    trap_init_percpu();
}

/* Initialize and load the per-CPU TSS and IDT. */
void trap_init_percpu(void)
{
    /* Setup a TSS so that we get the right stack when we trap to the kernel. */
    ts.ts_esp0 = KSTACKTOP;
    ts.ts_ss0 = GD_KD;

    /* Initialize the TSS slot of the gdt. */
    gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
                    sizeof(struct taskstate), 0);
    gdt[GD_TSS0 >> 3].sd_s = 0;

    /* Load the TSS selector (like other segment selectors, the bottom three
     * bits are special; we leave them 0). */
    ltr(GD_TSS0);

    /* Load the IDT. */
    lidt(&idt_pd);
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("TRAP frame at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    /* If this trap was a page fault that just happened (so %cr2 is meaningful),
     * print the faulting linear address. */
    if (tf == last_tf && tf->tf_trapno == T_PGFLT)
        cprintf("  cr2  0x%08x\n", rcr2());
    cprintf("  err  0x%08x", tf->tf_err);
    /* For page faults, print decoded fault error code:
     * U/K=fault occurred in user/kernel mode
     * W/R=a write/read caused the fault
     * PR=a protection violation caused the fault (NP=page not present). */
    if (tf->tf_trapno == T_PGFLT)
        cprintf(" [%s, %s, %s]\n",
            tf->tf_err & 4 ? "user" : "kernel",
            tf->tf_err & 2 ? "write" : "read",
            tf->tf_err & 1 ? "protection" : "not-present");
    else
        cprintf("\n");
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x\n", tf->tf_eflags);
    if ((tf->tf_cs & 3) != 0) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void print_regs(struct pushregs *regs)
{
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void trap_dispatch(struct trapframe *tf)
{
    /* Handle processor exceptions. */
    /* LAB 3: Your code here. */
    /* LAB 4: Update to handle more interrupts and syscall */

    /*
     * Handle spurious interrupts
     * The hardware sometimes raises these because of noise on the
     * IRQ line or other reasons. We don't care.
    */
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
        cprintf("Spurious interrupt on irq 7\n");
        //print_trapframe(tf);
        return;
    }

    /*
     * Handle clock interrupts. Don't forget to acknowledge the interrupt using
     * lapic_eoi() before calling the scheduler!
     * LAB 5: Your code here.
     */
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
        lapic_eoi();
        //cprintf("Timer interrupt\n");
        sched_yield(false);
        return;
    }

    //print_trapframe(tf);

    // redirect pagefaults
    if (tf->tf_trapno == 14) {
        page_fault_handler(tf);
        return;
    }

    // redirect int3 instruction
    if (tf->tf_trapno == 3) {
        monitor(tf);
        return;
    }

    // jos system call (0x30 = 48)
    if (tf->tf_trapno == 48) {
        int32_t r = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx,
                            tf->tf_regs.reg_ecx, tf->tf_regs.reg_ebx,
                            tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
        tf->tf_regs.reg_eax = r;
        return;
    }

    /* Unexpected trap: The user process or the kernel has a bug. */
    print_trapframe(tf);
    if (tf->tf_cs == GD_KT)
        panic("unhandled trap in kernel");
    else {
        env_destroy(curenv);
        return;
    }
}

void trap(struct trapframe *tf)
{
    /* The environment may have set DF and some versions of GCC rely on DF being
     * clear. */
    asm volatile("cld" ::: "cc");

    /* Check that interrupts are disabled.
     * If this assertion fails, DO NOT be tempted to fix it by inserting a "cli"
     * in the interrupt path. */
    assert(!(read_eflags() & FL_IF));

    cprintf("Incoming TRAP frame at %p\n", tf);

    if ((tf->tf_cs & 3) == 3) {
        /* Trapped from user mode. */
        assert(curenv);

        /* Copy trap frame (which is currently on the stack) into
         * 'curenv->env_tf', so that running the environment will restart at the
         * trap point. */
        curenv->env_tf = *tf;
        /* The trapframe on the stack should be ignored from here on. */
        tf = &curenv->env_tf;
    }

    /* Record that tf is the last real trapframe so print_trapframe can print
     * some additional information. */
    last_tf = tf;

    /* Dispatch based on what type of trap occurred */
    trap_dispatch(tf);

    /* If we made it to this point, then no other environment was scheduled, so
     * we should return to the current environment if doing so makes sense. */
    if (curenv && curenv->env_status == ENV_RUNNING)
        env_run(curenv);
    else
        sched_yield(false);
}

/*
 * Handles a page fault for an anonymous pagefault
 */
void resolve_anonymous(void *va, int perm) {
    struct page_info *pp = page_alloc(ALLOC_ZERO);
    if (!pp) cprintf("Pagefault -- page_alloc failure.\n");
    page_insert(curenv->env_pgdir, pp, (char *) ROUNDDOWN(va, PGSIZE),
                perm | PTE_U);
}

void page_fault_handler(struct trapframe *tf)
{
    uint32_t fault_va = rcr2();

    // filter kernel pagefault
    if (!(tf->tf_cs & 3))
        panic("Kernel page fault at va: %p!", fault_va);

    // usermode
    int slot = vma_seek(curenv, (void *) fault_va);
    struct vma *v = &curenv->env_vmas[slot];

    // faulted on invalid address
    if (slot < 0)
        cprintf("Pagefault -- No vma slot for %x.\n", fault_va);

    // faulted on write request
    else if ((v->perm & (PTE_W)) == PTE_W && ((tf->tf_err & PTE_W) == PTE_W)) {

        // check if pte exists
        pte_t *pte = NULL;
        struct page_info *pp_orig =
          page_lookup(curenv->env_pgdir, (void *) fault_va, &pte);

        // resolve COW pagefault
        if (pp_orig && (*pte & PTE_P) == PTE_P) {

            // if this is the last reference remaining, just use it in-place
            if (pp_orig->pp_ref == 1)
                *pte |= PTE_W;

            // if more references remain, make a physical copy to retain old one
            else {
                struct page_info *pp_copy = page_alloc(0);
                if (!pp_copy) cprintf("Pagefault -- page_alloc failure.\n");
                memcpy(page2kva(pp_copy), page2kva(pp_orig), PGSIZE);
                page_insert(curenv->env_pgdir, pp_copy,
                            (char *) ROUNDDOWN(fault_va, PGSIZE), v->perm);
            }

            return;
        }

        // resolve anonymous write pagefault
        resolve_anonymous((void *) fault_va, v->perm);
        return;
    }

    // faulted on write request for read-only, non-COW page
    else if ((tf->tf_err & PTE_W) == PTE_W)
        cprintf("Pagefault -- Write request on Read-Only page.\n");

    // faulted on a binary page
    else if (v->type == VMA_BINARY)
        cprintf("Pagefault -- Transfer of binary pages not supported.\n");

    // faulted on read request
    else if (v->type == VMA_ANON) {
        resolve_anonymous((void *) fault_va, v->perm);
        return;
    }

    // unexpected pagefault type
    else
        cprintf("Pagefault -- Unexpected params for this pagefault.\n");

    // destroy the environment that caused the fault
    cprintf("[%08x] user fault va %08x ip %08x\n", curenv->env_id, fault_va, tf->tf_eip);
    print_trapframe(tf);
    env_destroy(curenv);
}
