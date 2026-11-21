/*
AE_WORKER.H

A detached worker thread and a lock for AE's game-side units (ae_glue_setup_maps.c's map facts), from the platform
layer (ae_worker.c). The game's units never include <pthread.h> themselves: the Windows builds give them no POSIX
headers (port/windows/include/posix is the platform layer's alone), and in the 32-bit Linux build <pthread.h> brings
the C library's <limits.h>, whose LONG_MAX macro breaks cseries.h's enum of the same name. Plain types only.
*/

#ifndef __AE_WORKER_H
#define __AE_WORKER_H

struct ae_worker_lock;

/* a new lock (never freed: one a unit, for the game's life); NULL when none could be made */
struct ae_worker_lock *ae_worker_lock_new(void);
void ae_worker_lock_take(struct ae_worker_lock *lock);
void ae_worker_lock_give(struct ae_worker_lock *lock);
/* run(argument) on a thread of its own, detached; 0 when no thread started (run is not called then) */
int ae_worker_start(void *(*run)(void *), void *argument);
/* the calling thread waits this long (a worker's idle wait) */
void ae_worker_sleep(unsigned milliseconds);

/* a signal to wait on and wake (a lock and a condition variable, never freed: a unit makes one and keeps it); NULL
when none could be made. A waiter reads seen before it looks at what it waits for, then waits: it returns at once
when a wake came since seen, else after a wake or the milliseconds, whichever comes first (with NULL: a sleep) */
struct ae_worker_signal;
struct ae_worker_signal *ae_worker_signal_new(void);
unsigned int ae_worker_signal_seen(struct ae_worker_signal *signal);
void ae_worker_signal_wait(struct ae_worker_signal *signal, unsigned int seen, unsigned milliseconds);
void ae_worker_signal_wake(struct ae_worker_signal *signal);
/* a clock for deadlines: milliseconds that only go forward from any start, wrapping at 2^32 (compare by
subtraction) */
unsigned int ae_worker_milliseconds(void);
/* the processors online, at least 1 */
int ae_worker_cores(void);
/* the calling thread remembered as the game's, and whether the calling thread is it (1 when none is remembered) */
void ae_worker_main_set(void);
int ae_worker_on_main(void);

#endif
