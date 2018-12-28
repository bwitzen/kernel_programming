#ifndef JOS_KERN_KT_H
#define JOS_KERN_KT_H

#include <kern/trap.h>

void kernelthread_create();
void kernelthread_yield();
void kernelthread_pop_tf(struct trapframe *tf);
void spinner();

#endif // JOS_KERN_KT_H
