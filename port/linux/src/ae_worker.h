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

#endif
