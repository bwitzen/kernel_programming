#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

static uint64_t last_tsc = 0;

void sched_halt(void);

/*
 * Choose a user environment to run and run it.
 */
void sched_yield(bool force)
{

    /*
     * Implement simple round-robin scheduling.
     *
     * Search through 'envs' for an ENV_RUNNABLE environment in
     * circular fashion starting just after the env this CPU was
     * last running.  Switch to the first such environment found.
     *
     * If no envs are runnable, but the environment previously
     * running on this CPU is still ENV_RUNNING, it's okay to
     * choose that environment.
     *
     * Never choose an environment that's currently running (on
     * another CPU, if we had, ie., env_status == ENV_RUNNING).
     * If there are
     * no runnable environments, simply drop through to the code
     * below to halt the cpu.
     *
     * LAB 5: Your code here.
     */

    //cprintf("Switching to next env.\n");
    struct env *current;
    if(curenv == NULL || curenv->env_status == ENV_FREE) {
        size_t start_index = 0;
        if(curenv && curenv->env_status == ENV_FREE) { 
            //cprintf("env id: %d\n", curenv->env_id);
            start_index = ENVX(curenv->env_id) + 1;
        }
        for(size_t i = 0; i < NENV; ++i) {
            current = &envs[(start_index + i) % NENV];
            if (current->env_status == ENV_RUNNABLE) {
                if(current->env_wait_env >= 0) {
                    struct env *wait; 
                    int result = envid2env(current->env_wait_env, &wait, 0);
                    if(result != 0) {
                        curenv->env_wait_env = ENV_NOT_WAITING;
                    } else {
                        if(wait->env_status != ENV_RUNNABLE && wait->env_status != ENV_RUNNING) {
                            curenv->env_wait_env = ENV_NOT_WAITING;
                        } else {
                            continue;
                        }
                    }
                }
                //cprintf("Running env %d\n", (start_index + i) % NENV);
                //unlock_kernel();
                env_run(current);
            }
        }
    } else {

        uint64_t time_ran;
        if (last_tsc == 0) {
            last_tsc = read_tsc();
            time_ran = 0;
        } else {
            uint64_t temp_tsc = read_tsc();
            if(temp_tsc < last_tsc) {
                time_ran = temp_tsc;
            } else {
                time_ran = temp_tsc - last_tsc;
            }
            //cprintf("Ran for %lld time units\n", time_ran);
            last_tsc = temp_tsc;
        }
    
        if(force || curenv->env_time_slice < time_ran) {
            //cprintf("On to the next from %d\n", curenv->env_time_slice);
            curenv->env_time_slice = ENV_TIME_SLICE;
            if(curenv && curenv->env_status == ENV_RUNNING) curenv->env_status = ENV_RUNNABLE;
        } else {
            //cprintf("Keep doing this one\n");
            curenv->env_time_slice -= time_ran;
            env_run(curenv);
        }

        struct env *next = &envs[ENVX(curenv->env_id) + 1];
        if(next->env_status == ENV_RUNNABLE && next->env_wait_env < 0) {
            env_run(next);
        } else {
            //next = NULL;
            for(size_t i = 0; i < NENV; ++i) {
                //cprintf("checking %d with type %d\n", (i + ENVX(curenv->env_id) + 1) % NENV, envs[(i + ENVX(curenv->env_id) + 1) % NENV].env_status);
                current = &envs[(i + ENVX(curenv->env_id) + 1) % NENV];
                if (current->env_status == ENV_RUNNABLE) {
                    if(current->env_wait_env >= 0) {
                        struct env *wait; 
                        int result = envid2env(current->env_wait_env, &wait, 0);
                        if(result != 0) {
                            curenv->env_wait_env = ENV_NOT_WAITING;
                        } else {
                            if(wait->env_status != ENV_RUNNABLE && wait->env_status != ENV_RUNNING) {
                                curenv->env_wait_env = ENV_NOT_WAITING;
                            } else {
                                continue;
                            }
                        }
                    }
                    next = current;
                    break;
                } else {
                    //if(current->env_status != ENV_FREE) 
                    //    cprintf("env %d with status %d\n", i, current->env_status);
                }
            }
        }
        if (next != NULL) {
            cprintf("Run next env %d\n", ENVX(next->env_id));
            env_run(next);
        }
            
        //cprintf("Running next env %d with status: %d\n", ENVX(next->env_id), next->env_status);
    }

    cprintf("Schedule halt\n");
    /* sched_halt never returns */
    sched_halt();
}

/*
 * Halt this CPU when there is nothing to do. Wait until the timer interrupt
 * wakes it up. This function never returns.
 */
void sched_halt(void)
{
    int i;

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor. */
    for (i = 0; i < NENV; i++) {
        if ((envs[i].env_status == ENV_RUNNABLE ||
             envs[i].env_status == ENV_RUNNING ||
             envs[i].env_status == ENV_DYING))
            break;
    }
    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        while (1)
            monitor(NULL);
    }

    /* Mark that no environment is running on this CPU */
    curenv = NULL;
    lcr3(PADDR(kern_pgdir));

    /* Mark that this CPU is in the HALT state, so that when
     * timer interupts come in, we know we should re-acquire the
     * big kernel lock */
    xchg(&thiscpu->cpu_status, CPU_HALTED);

    /* Release the big kernel lock as if we were "leaving" the kernel */
    unlock_kernel();

    /* Reset stack pointer, enable interrupts and then halt. */
    asm volatile (
        "movl $0, %%ebp\n"
        "movl %0, %%esp\n"
        "pushl $0\n"
        "pushl $0\n"
        "sti\n"
        "hlt\n"
    : : "a" (thiscpu->cpu_ts.ts_esp0));
}
