/* sched.h -- preemptive round-robin scheduler with sleep + sync hooks.
 *
 * Task states:
 *   RUNNABLE  -- in the ring, scheduler will pick.
 *   SLEEPING  -- has a wake_tick; PIT wakes it when ticks >= wake_tick.
 *   BLOCKED   -- waiting on a sync primitive; some other task will unblock.
 *   ZOMBIE   -- exited; reaper (future) will clean up.
 *
 * sync.c reaches into wait_next + state directly when enqueuing/dequeuing
 * tasks on mutex/semaphore/condvar waiter lists, so those fields are part
 * of the public task layout.
 */
#ifndef NEXUS_SCHED_H
#define NEXUS_SCHED_H

#include "types.h"
#include "idt.h"

typedef void (*task_entry_t)(void);

typedef enum {
    TASK_RUNNABLE = 0,
    TASK_SLEEPING = 1,
    TASK_BLOCKED  = 2,
    TASK_ZOMBIE   = 3,
} task_state_t;

typedef struct task {
    uint64_t       saved_rsp;
    void          *stack_base;
    uint64_t       stack_size;
    uint64_t       id;
    const char    *name;
    task_state_t   state;
    uint64_t       wake_tick;     /* for SLEEPING tasks; 0 otherwise */
    struct task   *next;          /* circular runnable ring          */
    struct task   *wait_next;     /* sync primitive waiter list      */
} task_t;

void     sched_init(void);
task_t  *task_create(const char *name, task_entry_t entry);
void     scheduler_tick(interrupt_frame_t *frame);

/* Voluntary CPU yield (triggers a software interrupt). */
void     sched_yield(void);

/* Block the calling task for `ms` milliseconds. */
void     sched_sleep_ms(uint64_t ms);

task_t  *sched_current(void);
uint64_t sched_switches(void);

#endif
