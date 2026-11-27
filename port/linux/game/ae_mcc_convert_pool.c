/*
AE_MCC_CONVERT_POOL.C

The conversion workers of an MCC load (ae_mcc_convert_pool.h). One static pool: its lock and signals are made once and
kept, so a worker's last touch of them (after the pool let it go) is always of live memory. Jobs sit in a ring in
submission order; a worker takes the oldest queued one, the loader takes the oldest finished one.
*/

#include <stdlib.h>
#include <string.h>

#include "ae_mcc_convert_pool.h"
#include "../src/ae_worker.h"

enum
{
	JOB_FREE,
	JOB_QUEUED,
	JOB_RUNNING,
	JOB_DONE,
	JOB_FAILED
};

struct pool_job
{
	ae_mcc_convert_run run;
	void *data;
	unsigned int bytes;
	int state;
};

static struct
{
	struct ae_worker_lock *lock;
	struct ae_worker_signal *work, *done;
	int made;
	int running, stopping;
	int live;
	int job_cap;
	unsigned int byte_cap, bytes, bytes_most;
	int count, count_most;
	unsigned int head;
	ae_mcc_convert_discard discard;
	struct pool_job jobs[AE_MCC_CONVERT_JOBS_MOST];
} pool;

void *ae_mcc_convert_alloc(size_t bytes)
{
	return malloc(bytes ? bytes : 1);
}

void ae_mcc_convert_free(void *pointer)
{
	free(pointer);
}

static struct pool_job *job_at(int position)
{
	return &pool.jobs[(pool.head + (unsigned int)position) % AE_MCC_CONVERT_JOBS_MOST];
}

static void *worker(void *argument)
{
	(void)argument;
	for (;;)
	{
		struct pool_job *job = NULL;
		unsigned int seen = ae_worker_signal_seen(pool.work);
		int position;

		ae_worker_lock_take(pool.lock);
		for (position = 0; position < pool.count; position++)
		{
			if (job_at(position)->state == JOB_QUEUED)
			{
				job = job_at(position);
				break;
			}
		}
		if (job)
		{
			ae_mcc_convert_run run = job->run;
			void *data = job->data;
			unsigned int bytes = job->bytes;
			int ok;

			job->state = JOB_RUNNING;
			ae_worker_lock_give(pool.lock);
			ok = run(data, &bytes);
			ae_worker_lock_take(pool.lock);
			pool.bytes = pool.bytes - job->bytes + bytes;
			if (pool.bytes > pool.bytes_most)
				pool.bytes_most = pool.bytes;
			job->bytes = bytes;
			job->state = ok ? JOB_DONE : JOB_FAILED;
			ae_worker_lock_give(pool.lock);
			ae_worker_signal_wake(pool.done);
			continue;
		}
		if (pool.stopping)
		{
			/* (the last touch of the pool: the lock and signals outlive every worker) */
			pool.live--;
			ae_worker_lock_give(pool.lock);
			ae_worker_signal_wake(pool.done);
			return NULL;
		}
		ae_worker_lock_give(pool.lock);
		ae_worker_signal_wait(pool.work, seen, 50);
	}
}

int ae_mcc_convert_pool_start(int workers, int job_cap, unsigned int byte_cap)
{
	int started = 0, index, live;

	if (pool.running || workers < 1)
		return 0;
	if (!pool.made)
	{
		pool.lock = ae_worker_lock_new();
		pool.work = ae_worker_signal_new();
		pool.done = ae_worker_signal_new();
		if (!pool.lock || !pool.work || !pool.done)
			return 0;
		pool.made = 1;
	}
	ae_worker_lock_take(pool.lock);
	live = pool.live;
	ae_worker_lock_give(pool.lock);
	if (live)
		return 0;
	/* (a job left running by the last drain has ended since: its data is let go now) */
	for (index = 0; index < AE_MCC_CONVERT_JOBS_MOST; index++)
	{
		if ((pool.jobs[index].state == JOB_DONE || pool.jobs[index].state == JOB_FAILED) && pool.discard)
			pool.discard(pool.jobs[index].data);
	}
	if (workers > AE_MCC_CONVERT_WORKERS_MOST)
		workers = AE_MCC_CONVERT_WORKERS_MOST;
	if (job_cap < 1)
		job_cap = 1;
	if (job_cap > AE_MCC_CONVERT_JOBS_MOST)
		job_cap = AE_MCC_CONVERT_JOBS_MOST;
	memset(pool.jobs, 0, sizeof(pool.jobs));
	pool.head = 0;
	pool.count = pool.count_most = 0;
	pool.bytes = pool.bytes_most = 0;
	pool.job_cap = job_cap;
	pool.byte_cap = byte_cap;
	pool.stopping = 0;
	for (index = 0; index < workers; index++)
	{
		ae_worker_lock_take(pool.lock);
		pool.live++;
		ae_worker_lock_give(pool.lock);
		if (!ae_worker_start(worker, NULL))
		{
			ae_worker_lock_take(pool.lock);
			pool.live--;
			ae_worker_lock_give(pool.lock);
			break;
		}
		started++;
	}
	if (!started)
		return 0;
	pool.running = 1;
	return started;
}

int ae_mcc_convert_pool_running(void)
{
	return pool.running;
}

int ae_mcc_convert_pool_room(unsigned int bytes)
{
	int room;

	if (!pool.running)
		return 0;
	ae_worker_lock_take(pool.lock);
	room = pool.count < pool.job_cap && (!pool.count || (pool.bytes <= pool.byte_cap && bytes <= pool.byte_cap - pool.bytes));
	ae_worker_lock_give(pool.lock);
	return room;
}

int ae_mcc_convert_pool_submit(ae_mcc_convert_run run, void *data, unsigned int bytes)
{
	struct pool_job *job;

	if (!run || !ae_mcc_convert_pool_room(bytes))
		return 0;
	ae_worker_lock_take(pool.lock);
	job = job_at(pool.count);
	job->run = run;
	job->data = data;
	job->bytes = bytes;
	job->state = JOB_QUEUED;
	pool.count++;
	pool.bytes += bytes;
	if (pool.count > pool.count_most)
		pool.count_most = pool.count;
	if (pool.bytes > pool.bytes_most)
		pool.bytes_most = pool.bytes;
	ae_worker_lock_give(pool.lock);
	ae_worker_signal_wake(pool.work);
	return 1;
}

void *ae_mcc_convert_pool_oldest(void)
{
	void *data = NULL;

	if (!pool.running)
		return NULL;
	ae_worker_lock_take(pool.lock);
	if (pool.count)
		data = job_at(0)->data;
	ae_worker_lock_give(pool.lock);
	return data;
}

int ae_mcc_convert_pool_wait(unsigned int milliseconds, void (*pulse)(void))
{
	unsigned int start = ae_worker_milliseconds();

	if (!pool.running)
		return -2;
	for (;;)
	{
		int state = JOB_FREE;
		unsigned int seen = ae_worker_signal_seen(pool.done), waited;

		ae_worker_lock_take(pool.lock);
		if (pool.count)
			state = job_at(0)->state;
		ae_worker_lock_give(pool.lock);
		if (state == JOB_FREE)
			return -2;
		if (state == JOB_DONE)
			return 1;
		if (state == JOB_FAILED)
			return 0;
		waited = ae_worker_milliseconds() - start;
		if (waited >= milliseconds)
			return -1;
		if (pulse)
			pulse();
		ae_worker_signal_wait(pool.done, seen, milliseconds - waited < 10 ? milliseconds - waited : 10);
	}
}

void *ae_mcc_convert_pool_take(void)
{
	struct pool_job *job;
	void *data = NULL;

	if (!pool.running)
		return NULL;
	ae_worker_lock_take(pool.lock);
	job = job_at(0);
	if (pool.count && (job->state == JOB_DONE || job->state == JOB_FAILED))
	{
		data = job->data;
		pool.bytes -= job->bytes;
		memset(job, 0, sizeof(*job));
		pool.head = (pool.head + 1) % AE_MCC_CONVERT_JOBS_MOST;
		pool.count--;
	}
	ae_worker_lock_give(pool.lock);
	return data;
}

void ae_mcc_convert_pool_held(int *jobs, unsigned int *bytes, int *jobs_most, unsigned int *bytes_most)
{
	if (pool.made)
		ae_worker_lock_take(pool.lock);
	if (jobs)
		*jobs = pool.count;
	if (bytes)
		*bytes = pool.bytes;
	if (jobs_most)
		*jobs_most = pool.count_most;
	if (bytes_most)
		*bytes_most = pool.bytes_most;
	if (pool.made)
		ae_worker_lock_give(pool.lock);
}

int ae_mcc_convert_pool_stop(unsigned int milliseconds, ae_mcc_convert_discard discard)
{
	unsigned int start = ae_worker_milliseconds();
	int position, running, live;

	if (!pool.running)
		return 0;
	/* (jobs not started are the loader's again at once: no worker takes them once stopping is seen) */
	ae_worker_lock_take(pool.lock);
	pool.stopping = 1;
	for (position = 0; position < pool.count; position++)
	{
		struct pool_job *job = job_at(position);

		if (job->state == JOB_QUEUED)
		{
			if (discard)
				discard(job->data);
			pool.bytes -= job->bytes;
			job->state = JOB_FREE;
			job->data = NULL;
		}
	}
	ae_worker_lock_give(pool.lock);
	ae_worker_signal_wake(pool.work);
	for (;;)
	{
		unsigned int seen = ae_worker_signal_seen(pool.done), waited;

		running = 0;
		ae_worker_lock_take(pool.lock);
		for (position = 0; position < pool.count; position++)
			running += job_at(position)->state == JOB_RUNNING;
		live = pool.live;
		ae_worker_lock_give(pool.lock);
		if (!running && !live)
			break;
		waited = ae_worker_milliseconds() - start;
		if (waited >= milliseconds)
			break;
		ae_worker_signal_wake(pool.work);
		ae_worker_signal_wait(pool.done, seen, milliseconds - waited < 10 ? milliseconds - waited : 10);
	}
	ae_worker_lock_take(pool.lock);
	for (position = 0; position < pool.count; position++)
	{
		struct pool_job *job = job_at(position);

		if (job->state == JOB_DONE || job->state == JOB_FAILED)
		{
			if (discard)
				discard(job->data);
			pool.bytes -= job->bytes;
			job->state = JOB_FREE;
			job->data = NULL;
		}
	}
	if (!running)
	{
		pool.count = 0;
		pool.head = 0;
		pool.bytes = 0;
	}
	pool.discard = discard;
	ae_worker_lock_give(pool.lock);
	pool.running = 0;
	return running;
}
