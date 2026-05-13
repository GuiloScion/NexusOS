/* sync.h -- kernel-mode synchronization primitives.
 *
 * All three primitives are non-recursive and provide FIFO-on-release
 * semantics: when a holder releases, the waiter at the head of the list
 * becomes runnable (and, for mutex, takes ownership directly, there is
 * no race window during which a late-arriving task can steal the lock).
 *
 * All blocking is voluntary, a task that needs to wait calls
 * sched_yield() while marked BLOCKED and is later unblocked by some other
 * task calling unlock/post/signal. Sleeping (sched_sleep_ms) and blocking
 * use different task states so the scheduler can distinguish them.
 *
 * NOT safe for use from inside an interrupt handler (none of the lock
 * paths are reentrant). Use these only from task context.
 */
#ifndef NEXUS_SYNC_H
#define NEXUS_SYNC_H

#include "types.h"
#include "sched.h"

typedef struct {
    bool      locked;
    task_t   *owner;
    task_t   *wait_head;
} mutex_t;

typedef struct {
    int64_t   count;
    task_t   *wait_head;
} semaphore_t;

typedef struct {
    task_t   *wait_head;
} condvar_t;

void mutex_init   (mutex_t *m);
void mutex_lock   (mutex_t *m);
void mutex_unlock (mutex_t *m);

void sem_init     (semaphore_t *s, int64_t initial);
void sem_wait     (semaphore_t *s);
void sem_post     (semaphore_t *s);

void cond_init    (condvar_t *c);
void cond_wait    (condvar_t *c, mutex_t *m);   /* m must be held */
void cond_signal  (condvar_t *c);               /* wake one waiter   */
void cond_broadcast(condvar_t *c);              /* wake all waiters  */

#endif
