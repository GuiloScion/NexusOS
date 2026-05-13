/* sched.c -- preemptive round-robin scheduler.
 *
 * Two entry points feed scheduler_tick():
 *   - PIT IRQ (vector 32) at 100 Hz, in pit.c.
 *   - The software interrupt INT 0x80 raised by sched_yield(), routed by
 *     interrupt_dispatch() in idt.c.
 *
 * Both arrive with a fully-formed interrupt_frame_t at the top of the
 * running task's stack. scheduler_tick:
 *   1. wakes any SLEEPING tasks whose wake_tick has elapsed,
 *   2. picks the next RUNNABLE task in ring order (skipping non-runnables),
 *   3. if a different task was picked, records the running task's frame
 *      address as its saved_rsp and tail-calls _switch_to(), which loads
 *      that RSP and pop/iretq's into the new task.
 *
 * scheduler_tick does not send EOI; the PIT IRQ handler does that itself
 * before calling in here. The yield path needs no EOI at all (INT 0x80 is
 * a software interrupt, not a PIC line).
 */

#include "sched.h"
#include "kmalloc.h"
#include "string.h"
#include "console.h"
#include "io.h"
#include "pit.h"

#define KSTACK_SIZE   (16 * 1024)
#define KERNEL_CS     0x18               /* matches the 64-bit code seg in boot.asm */
#define KERNEL_SS     0x00               /* long mode allows null SS at CPL 0      */
#define RFLAGS_BASE   0x202              /* reserved bit 1 set, IF=1               */

extern void _switch_to(uint64_t new_rsp) __attribute__((noreturn));

static task_t   idle_task;
static task_t  *current      = NULL;
static uint64_t next_id      = 0;
static uint64_t switch_count = 0;

void sched_init(void) {
    idle_task.saved_rsp  = 0;
    idle_task.stack_base = NULL;
    idle_task.stack_size = 0;
    idle_task.id         = next_id++;
    idle_task.name       = "idle";
    idle_task.state      = TASK_RUNNABLE;
    idle_task.wake_tick  = 0;
    idle_task.wait_next  = NULL;
    idle_task.next       = &idle_task;
    current = &idle_task;
    console_puts("[sched] init, idle task id=0\n");
}

task_t *task_create(const char *name, task_entry_t entry) {
    task_t  *t     = (task_t  *)kmalloc(sizeof(task_t));
    if (!t) return NULL;
    uint8_t *stack = (uint8_t *)kmalloc(KSTACK_SIZE);
    if (!stack) { kfree(t); return NULL; }

    interrupt_frame_t *f = (interrupt_frame_t *)
        (stack + KSTACK_SIZE - sizeof(interrupt_frame_t));
    memset(f, 0, sizeof(*f));
    f->rip    = (uint64_t)entry;
    f->cs     = KERNEL_CS;
    f->rflags = RFLAGS_BASE;
    f->rsp    = (uint64_t)(stack + KSTACK_SIZE);
    f->ss     = KERNEL_SS;

    t->saved_rsp  = (uint64_t)f;
    t->stack_base = stack;
    t->stack_size = KSTACK_SIZE;
    t->id         = next_id++;
    t->name       = name;
    t->state      = TASK_RUNNABLE;
    t->wake_tick  = 0;
    t->wait_next  = NULL;

    cli();
    t->next       = current->next;
    current->next = t;
    sti();

    console_puts("[sched] task '"); console_puts(name);
    console_puts("' id="); console_put_dec(t->id);
    console_puts(" stack="); console_put_hex((uint64_t)stack);
    console_puts("\n");
    return t;
}

/* Walk the ring; for any SLEEPING task whose wake_tick has been reached,
 * move it back to RUNNABLE. Called every tick. */
static void wake_sleepers(void) {
    uint64_t now = pit_ticks();
    task_t *t = current;
    do {
        if (t->state == TASK_SLEEPING && now >= t->wake_tick) {
            t->state     = TASK_RUNNABLE;
            t->wake_tick = 0;
        }
        t = t->next;
    } while (t != current);
}

/* Pick the next RUNNABLE task strictly after `current` in ring order.
 * If only `current` is runnable, returns NULL (stay). If `current` itself
 * is no longer runnable (just yielded/slept/blocked), this is guaranteed
 * to find idle in the ring and return it. */
static task_t *pick_next(void) {
    task_t *n = current->next;
    while (n != current) {
        if (n->state == TASK_RUNNABLE) return n;
        n = n->next;
    }
    return NULL;
}

void scheduler_tick(interrupt_frame_t *frame) {
    if (!current) return;                 /* called before sched_init */

    wake_sleepers();

    task_t *n = pick_next();
    if (!n) return;                       /* nobody else runnable */

    current->saved_rsp = (uint64_t)frame;
    current = n;
    switch_count++;
    _switch_to(current->saved_rsp);
}

void sched_yield(void) {
    __asm__ volatile ("int $0x80");
}

void sched_sleep_ms(uint64_t ms) {
    /* PIT is configured for 100 Hz, so 1 tick = 10 ms. Round up so a
     * caller asking for "sleep 5 ms" gets at least one tick. */
    uint64_t ticks_to_wait = (ms + 9) / 10;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    cli();
    current->wake_tick = pit_ticks() + ticks_to_wait;
    current->state     = TASK_SLEEPING;
    sti();
    sched_yield();                        /* fall asleep; resumed when woken */
}

task_t  *sched_current(void)  { return current; }
uint64_t sched_switches(void) { return switch_count; }
