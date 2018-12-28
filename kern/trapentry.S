/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)                                                 \
    .globl name;            /* define global symbol for 'name' */              \
    .type name, @function;  /* symbol type is function */                      \
    .align 2;               /* align function definition */                    \
    name:                   /* function starts here */                         \
    pushl $(num);                                                              \
    jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)                                            \
    .globl name;                                                               \
    .type name, @function;                                                     \
    .align 2;                                                                  \
    name:                                                                      \
    pushl $0;                                                                  \
    pushl $(num);                                                              \
    jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 * see intel manual 3-6-page 188
 */
TRAPHANDLER_NOEC(traphandler_de, 0)
TRAPHANDLER_NOEC(traphandler_db, 1)
// 2 is not used
TRAPHANDLER_NOEC(traphandler_bp, 3)
TRAPHANDLER_NOEC(traphandler_of, 4)
TRAPHANDLER_NOEC(traphandler_br, 5)
TRAPHANDLER_NOEC(traphandler_ud, 6)
TRAPHANDLER_NOEC(traphandler_nm, 7)
TRAPHANDLER(traphandler_df, 8)
// 9 is not used
TRAPHANDLER(traphandler_ts, 10)
TRAPHANDLER(traphandler_np, 11)
TRAPHANDLER(traphandler_ss, 12)
TRAPHANDLER(traphandler_gp, 13)
TRAPHANDLER(traphandler_pf, 14)
// 15 is not used
TRAPHANDLER_NOEC(traphandler_mf, 16)
TRAPHANDLER(traphandler_ac, 17)
TRAPHANDLER_NOEC(traphandler_mc, 18)
TRAPHANDLER_NOEC(traphandler_xm, 19)
TRAPHANDLER_NOEC(traphandler_ve, 20)
// 21..32 reversed
// 33..47 ?
TRAPHANDLER_NOEC(traphandler_syscall, 48)
TRAPHANDLER_NOEC(irq_timer, IRQ_OFFSET + IRQ_TIMER)
TRAPHANDLER_NOEC(irq_kbd, IRQ_OFFSET + IRQ_KBD)
TRAPHANDLER_NOEC(irq_serial, IRQ_OFFSET + IRQ_SERIAL)
TRAPHANDLER_NOEC(irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS)
TRAPHANDLER_NOEC(irq_ide, IRQ_OFFSET + IRQ_IDE)
TRAPHANDLER_NOEC(irq_error, IRQ_OFFSET + IRQ_ERROR)

_alltraps:
    pushl   %ds       // + "match definition of trapframe" (inc/trap.h)
    pushl   %es       // | order is regs/es/ds
    pushal            // | so push in reverse order
    pushl   $GD_KD    // +
    popl    %ds       // | "update ds and es to $GD_KD"
    pushl   $GD_KD    // |
    popl    %es       // |
    pushl   %esp      // + push esp on stack as arg of trap
    call    trap      // + call trap.c-trap()
