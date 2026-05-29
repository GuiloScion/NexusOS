# 8. Multitasking: a scheduler and synchronization

[← Memory management](07-memory-management.md) · [Home](README.md) · [Next: Storage →](09-storage.md)

A single thread of execution can only do one thing. To run a shell *and*
background work *and*, later, multiple programs, the kernel needs **multitasking**
— the illusion that several tasks run at once on one CPU. The trick: switch
between them so fast it looks simultaneous. This chapter builds a scheduler and
the locks that make sharing data between tasks safe.

## What a "task" is

A task (or thread) is really just: a **saved set of registers** (including the
instruction pointer and stack pointer) plus its **own stack**. To run a task,
you load its registers; to pause it, you save them. Switching tasks = save the
current registers, load another's.

NexusOS represents this as a `task_t` (see `kernel/sched.h`) holding the saved
stack pointer, a state (RUNNABLE / BLOCKED / SLEEPING), and bookkeeping links.

## The context switch

The heart of it is a tiny assembly routine that saves the callee-saved
registers onto the current stack, swaps the stack pointer to the next task, and
restores *its* registers (`kernel/sched_switch.asm`). Because the instruction
pointer is part of what's saved/restored (via the return address on the stack),
when you "return" you're suddenly running the *other* task. It feels like magic
the first time; it's really just careful stack manipulation.

Creating a task means allocating a stack and faking an initial saved state so
that the first switch "returns" into the task's entry function:

```c
task_create("worker", worker_main);   // NexusOS API
```

## Scheduling policy: round-robin

*Which* task runs next? The simplest fair policy is **round-robin**: cycle
through runnable tasks, giving each a turn. NexusOS keeps a list of tasks and, on
each scheduling decision, picks the next runnable one.

Two ways a switch happens:

- **Cooperative:** a task calls `sched_yield()` to voluntarily give up the CPU.
- **Preemptive:** the **timer interrupt** (Chapter 6) fires and the handler
  forces a switch — so even a task that never yields can't hog the CPU. This is
  why the PIT heartbeat matters.

`sched_sleep_ms()` blocks a task until enough timer ticks have passed — useful
for tasks that wake periodically.

## The hard part: shared data and races

The moment two tasks (or a task and an interrupt) touch the same variable, you
have a **race condition**. Classic example: two tasks both do `counter++`. That's
really *read, add, write* — if a switch happens mid-sequence, an update can be
lost, and `counter` ends up wrong.

You need **mutual exclusion**: a way to say "only one task in this critical
section at a time."

## Synchronization primitives

NexusOS's `kernel/sync.c` provides three, all built on the scheduler's BLOCKED
state:

- **Mutex** — one owner at a time. `mutex_lock` either takes the lock or *blocks*
  the task (parks it, switches away) until the owner `mutex_unlock`s and hands
  off to the next waiter.
- **Semaphore** — a counter. `sem_wait` decrements (blocking at zero), `sem_post`
  increments (waking a waiter). Great for "producer/consumer" patterns and for an
  interrupt to wake a task (the disk driver uses one).
- **Condition variable** — wait for a condition, signaled by another task.

The implementation trick on a single CPU: wrap the tiny state updates in
`cli`/`sti` (disable interrupts) so they're atomic with respect to the timer
interrupt — the only thing that could preempt them.

## Proving it works

How do you know your locking is correct? **Test the invariant.** NexusOS runs two
demo tasks that each increment their own counter and a shared one under a mutex.
The invariant: `counter_a + counter_b == shared` (give or take one if sampled
mid-update). The `tasks` shell command checks it live:

```
switches:     1273302
counter_a:    144426
counter_b:    144425
shared:       288851
a+b - shared: 0          <- if this stays 0, the mutex held
```

Seeing that hold after a million context switches — including on real hardware —
is real evidence your scheduler is fair and your locks are sound. Build a test
like this; it catches subtle bugs that "it seemed to work" never will.

## Where this leads

With multitasking, the kernel can run a shell as the idle task while background
work proceeds, and (Chapter 11) a compositor task can redraw the screen
independently. It's also the foundation for real **processes** later (Chapter
13) — a process is a task plus its own address space and privilege level.

Next, a different kind of waiting-on-hardware: reading from a disk.

[← Memory management](07-memory-management.md) · [Home](README.md) · [Next: Storage →](09-storage.md)
