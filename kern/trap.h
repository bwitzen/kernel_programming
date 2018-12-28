/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct gatedesc idt[];
extern struct pseudodesc idt_pd;

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct pushregs *regs);
void print_trapframe(struct trapframe *tf);
void page_fault_handler(struct trapframe *);
void backtrace(struct trapframe *);

#endif /* JOS_KERN_TRAP_H */
