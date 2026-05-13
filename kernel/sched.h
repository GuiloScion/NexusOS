/* sched.h -- preemptive round-robin scheduler.
 *
 * A task is a kernel-mode thread with its own 16 KiB stack. The PIT IRQ
 * fires every 10 ms; at each tick the scheduler saves the running task's
 * context (which is already laid out on its stack as an interrupt_frame_t)
 * and resumes the next runnable task in a circular ring. The idle task is
 * always present and is what kernel_main becomes once preemption begins.
 */
#ifndef NEXUS_SCHED_H
#define NEXUS_SCHED_H

#include "types.h"
#include "idt.h"

typedef void (*task_entry_t)(void);

typedef enum {
    TASK_RUNNABLE = 0,
    TASK_BLOCKED  = 1,
    TASK_ZOMBIE   = 2,
} task_state_t;

typedef struct task {
    uint64_t      saved_rsp;     /* RSP pointing into this task's saved frame */
    void         *stack_base;    /* low end of the 16 KiB allocation          */
    uint64_t      stack_size;
    uint64_t      id;
    const char   *name;
    task_state_t  state;
    struct task  *next;          /* circular runnable ring                    */
} task_t;

void     sched_init(void);
task_t  *task_create(const char *name, task_entry_t entry);
void     scheduler_tick(interrupt_frame_t *frame);    /* called from PIT IRQ  */
task_t  *sched_current(void);
uint64_t sched_switches(void);

#endif
