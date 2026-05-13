/* sched.c -- preemptive round-robin scheduler.
 *
 * Context-switch mechanism:
 *   When the PIT IRQ fires, the existing isr_common stub has already pushed
 *   the full interrupt_frame_t (15 GPRs + vector/error + iretq frame) onto
 *   the currently-running task's kernel stack. interrupt_dispatch calls the
 *   registered IRQ0 handler, which calls scheduler_tick(frame).
 *
 *   If a switch is warranted, scheduler_tick:
 *     1. records the *address of frame* as current->saved_rsp,
 *     2. updates `current` to point at the next runnable task,
 *     3. sends EOI to the PIC (we're about to skip the normal return path),
 *     4. tail-calls _switch_to() which loads RSP from the new task's
 *        saved_rsp and re-runs the same pop-then-iretq sequence that
 *        isr_common would have. iretq jumps to wherever that task was when
 *        it was last preempted (or, for a brand-new task, to its entry
 *        point: see task_create()).
 *
 *   If no switch is warranted (only idle exists), scheduler_tick returns
 *   normally and interrupt_dispatch handles EOI as usual.
 */

#include "sched.h"
#include "kmalloc.h"
#include "string.h"
#include "console.h"
#include "io.h"
#include "pic.h"

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
    idle_task.saved_rsp  = 0;        /* sentinel: this task is currently running */
    idle_task.stack_base = NULL;
    idle_task.stack_size = 0;
    idle_task.id         = next_id++;
    idle_task.name       = "idle";
    idle_task.state      = TASK_RUNNABLE;
    idle_task.next       = &idle_task;
    current = &idle_task;

    console_puts("[sched] init, idle task id=0\n");
}

task_t *task_create(const char *name, task_entry_t entry) {
    task_t  *t     = (task_t  *)kmalloc(sizeof(task_t));
    if (!t) return NULL;
    uint8_t *stack = (uint8_t *)kmalloc(KSTACK_SIZE);
    if (!stack) { kfree(t); return NULL; }

    /* Lay out an interrupt frame at the top of the stack so the very first
     * _switch_to into this task pops zeroed GPRs and iretqs to `entry`
     * with interrupts enabled. */
    interrupt_frame_t *f = (interrupt_frame_t *)
        (stack + KSTACK_SIZE - sizeof(interrupt_frame_t));
    memset(f, 0, sizeof(*f));
    f->rip    = (uint64_t)entry;
    f->cs     = KERNEL_CS;
    f->rflags = RFLAGS_BASE;
    f->rsp    = (uint64_t)(stack + KSTACK_SIZE);
    f->ss     = KERNEL_SS;
    /* vector and error_code are skipped by the `add rsp,16` in _switch_to. */

    t->saved_rsp  = (uint64_t)f;
    t->stack_base = stack;
    t->stack_size = KSTACK_SIZE;
    t->id         = next_id++;
    t->name       = name;
    t->state      = TASK_RUNNABLE;

    /* Splice into the ring just after `current`. Disabling interrupts here
     * is paranoia -- scheduler_tick only reads `next`, never modifies it --
     * but it's cheap insurance. */
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

void scheduler_tick(interrupt_frame_t *frame) {
    if (!current) return;                          /* before sched_init() */
    if (current->next == current) return;          /* only idle exists    */

    /* Walk the ring forward from current->next, looking for the next
     * RUNNABLE task. If the only RUNNABLE task is `current` itself, stay. */
    task_t *n = current->next;
    while (n != current && n->state != TASK_RUNNABLE) n = n->next;
    if (n == current) return;

    current->saved_rsp = (uint64_t)frame;
    current = n;
    switch_count++;

    pic_send_eoi(0);          /* normal dispatch path won't run; do it here */
    _switch_to(current->saved_rsp);
}

task_t  *sched_current(void)  { return current; }
uint64_t sched_switches(void) { return switch_count; }
