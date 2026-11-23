#ifndef AE_MCC_CONVERT_POOL_H
#define AE_MCC_CONVERT_POOL_H

/*
AE_MCC_CONVERT_POOL.H

Worker threads that run an MCC load's conversion jobs (a sound's decoding and encoding) beside the thread that loads
it (ae_worker.h). One pool a process, used by one thread, the loader: it submits jobs in order and takes their results
in the same order, the oldest first, so whatever it does with a result happens in the order one thread would give.

A job is a function and its data. A worker runs it with no engine state: only the data, the C library's memory
(ae_mcc_convert_alloc, never cseries' tracked allocator, which is not for other threads) and pure code.

The pool holds at most job_cap jobs (queued, running or finished and not taken) and, past its first job, at most
byte_cap bytes: each job's bytes as submitted, then as its run reports them (input and output held).
*/

#include <stddef.h>

/* the C library's memory, for a job's buffers (any thread) */
void *ae_mcc_convert_alloc(size_t bytes);
void ae_mcc_convert_free(void *pointer);

/* a job: on a worker, 1 when done, 0 when it failed; *bytes is what it holds when it returns (it starts as submitted) */
typedef int (*ae_mcc_convert_run)(void *data, unsigned int *bytes);
/* a job's data let go: for a job never run, or run and not taken, when the pool stops */
typedef void (*ae_mcc_convert_discard)(void *data);

/* the pool started with this many workers (1 or more, at most AE_MCC_CONVERT_WORKERS_MOST); 0 when none started, or
when a stopped pool's workers are still running a job */
#define AE_MCC_CONVERT_WORKERS_MOST 8
#define AE_MCC_CONVERT_JOBS_MOST 16
int ae_mcc_convert_pool_start(int workers, int job_cap, unsigned int byte_cap);
int ae_mcc_convert_pool_running(void);

/* whether a job of these bytes may be submitted now; submit: 0 when not (the job is not the pool's then) */
int ae_mcc_convert_pool_room(unsigned int bytes);
int ae_mcc_convert_pool_submit(ae_mcc_convert_run run, void *data, unsigned int bytes);

/* the oldest job's data (NULL: none) and its wait: 1 when it is done, 0 when it failed, -1 when the milliseconds
passed first (it still runs), -2 with no job; pulse (if any) is called on the waiting thread every few milliseconds */
void *ae_mcc_convert_pool_oldest(void);
int ae_mcc_convert_pool_wait(unsigned int milliseconds, void (*pulse)(void));
/* the oldest job taken from the pool, once it is done or failed (the data is the caller's again); NULL otherwise */
void *ae_mcc_convert_pool_take(void);

/* what the pool holds now and the most it held since it started: jobs and bytes */
void ae_mcc_convert_pool_held(int *jobs, unsigned int *bytes, int *jobs_most, unsigned int *bytes_most);

/* the one drain: jobs not started are discarded, running ones are waited for (at most the milliseconds), finished
ones not taken are discarded, then the workers end. Returns the jobs still running at the deadline: their data is
left to them, the pool cannot start again until they end, and the next start discards it */
int ae_mcc_convert_pool_stop(unsigned int milliseconds, ae_mcc_convert_discard discard);

#endif
