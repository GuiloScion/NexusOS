/* sync.c -- mutex, semaphore, condvar built on the scheduler's BLOCKED
 *           state and the explicit yield (INT 0x80).
 *
 * Concurrency model: single CPU + preemptive scheduler. The only way one
 * task observes another's writes is across a context switch. By performing
 * all primitive-state updates under cli/sti, every critical section is made
 * atomic with respect to the PIT IRQ, the only thing that could otherwise
 * preempt it. INT 0x80 (sched_yield) does not require IF=1, so it can yield
 * from inside a critical section by releasing IF only after the state
 * change is committed.
 *
 * Hand-off semantics for mutex_unlock:
 *   When the lock is contended, the unlocker transfers ownership directly
 *   to the next waiter rather than clearing `locked` and letting a race
 *   decide. This eliminates a class of livelock / unfair-wakeup bugs at
 *   the cost of slightly more code in unlock.
 */

#include "sync.h"
#include "io.h"

extern task_t *sched_current(void);

/* ---------- mutex ----------------------------------------------------- */

void mutex_init(mutex_t *m) {
    m->locked    = false;
    m->owner     = NULL;
    m->wait_head = NULL;
}

void mutex_lock(mutex_t *m) {
    task_t *self = sched_current();

    cli();
    if (!m->locked) {
        m->locked = true;
        m->owner  = self;
        sti();
        return;
    }
  
    self->wait_next = NULL;
    if (!m->wait_head) {
        m->wait_head = self;
    } else {
        task_t *t = m->wait_head;
        while (t->wait_next) t = t->wait_next;
        t->wait_next = self;
    }
    self->state = TASK_BLOCKED;
    sti();
    sched_yield();
}

void mutex_unlock(mutex_t *m) {
    cli();
    if (m->wait_head) {
        task_t *t       = m->wait_head;
        m->wait_head    = t->wait_next;
        t->wait_next    = NULL;
        m->owner        = t;
        t->state        = TASK_RUNNABLE;
    } else {
        m->locked = false;
        m->owner  = NULL;
    }
    sti();
}

/* ---------- semaphore ------------------------------------------------- */

void sem_init(semaphore_t *s, int64_t initial) {
    s->count     = initial;
    s->wait_head = NULL;
}

void sem_wait(semaphore_t *s) {
    task_t *self = sched_current();

    cli();
    if (s->count > 0) {
        s->count--;
        sti();
        return;
    }
    self->wait_next = NULL;
    if (!s->wait_head) {
        s->wait_head = self;
    } else {
        task_t *t = s->wait_head;
        while (t->wait_next) t = t->wait_next;
        t->wait_next = self;
    }
    self->state = TASK_BLOCKED;
    sti();
    sched_yield();
}

void sem_post(semaphore_t *s) {
    cli();
    if (s->wait_head) {
        task_t *t    = s->wait_head;
        s->wait_head = t->wait_next;
        t->wait_next = NULL;
        t->state     = TASK_RUNNABLE;

    } else {
        s->count++;
    }
    sti();
}

/* ---------- condvar --------------------------------------------------- */

void cond_init(condvar_t *c) {
    c->wait_head = NULL;
}

void cond_wait(condvar_t *c, mutex_t *m) {
    task_t *self = sched_current();

    cli();
    /* enqueue on the condvar */
    self->wait_next = NULL;
    if (!c->wait_head) {
        c->wait_head = self;
    } else {
        task_t *t = c->wait_head;
        while (t->wait_next) t = t->wait_next;
        t->wait_next = self;
    }
    self->state = TASK_BLOCKED;

    /* inline mutex_unlock (no nested cli/sti) so the release + sleep is
     * atomic with respect to other tasks */
    if (m->wait_head) {
        task_t *t       = m->wait_head;
        m->wait_head    = t->wait_next;
        t->wait_next    = NULL;
        m->owner        = t;
        t->state        = TASK_RUNNABLE;
    } else {
        m->locked = false;
        m->owner  = NULL;
    }
    sti();

    sched_yield();
    mutex_lock(m);
}

void cond_signal(condvar_t *c) {
    cli();
    if (c->wait_head) {
        task_t *t    = c->wait_head;
        c->wait_head = t->wait_next;
        t->wait_next = NULL;
        t->state     = TASK_RUNNABLE;
    }
    sti();
}

void cond_broadcast(condvar_t *c) {
    cli();
    task_t *t = c->wait_head;
    c->wait_head = NULL;
    while (t) {
        task_t *nxt = t->wait_next;
        t->wait_next = NULL;
        t->state     = TASK_RUNNABLE;
        t = nxt;
    }
    sti();
}
