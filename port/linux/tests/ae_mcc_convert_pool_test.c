/*
AE_MCC_CONVERT_POOL_TEST.C

The MCC conversion workers (port/linux/game/ae_mcc_convert_pool.c): jobs submitted in order and taken in order give
the bytes one thread gives, with 1 to 4 workers, a cap of one job, a byte cap; a failed job (the first, one in the
middle, the last) is reported as failed and the others are not; a wait past its deadline returns and the job still
ends; the drain discards what was not taken, waits for running jobs, and a job still running at its deadline keeps
the pool from starting again until it ends. Built with ThreadSanitizer too (tools/test_ae_units.py).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../game/ae_mcc_convert_pool.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define JOB_BYTES 4096

struct job
{
	unsigned int seed;
	int fail;
	int hold; /* spins until released */
	unsigned char output[JOB_BYTES];
	int ran;
};

static volatile int release_hold;
static int discarded, held_ran;

static void make_bytes(unsigned int seed, unsigned char *output)
{
	unsigned int state = seed * 2654435761u + 1u, index;

	for (index = 0; index < JOB_BYTES; index++)
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		output[index] = (unsigned char)(state >> 24);
	}
}

static void nap(long nanoseconds)
{
	struct timespec wait;

	wait.tv_sec = 0;
	wait.tv_nsec = nanoseconds;
	nanosleep(&wait, NULL);
}

static int run_job(void *data, unsigned int *bytes)
{
	struct job *job = (struct job *)data;

	while (job->hold && !__atomic_load_n(&release_hold, __ATOMIC_ACQUIRE))
		nap(1000000);
	/* (uneven times, so later jobs often finish first) */
	nap((long)(job->seed % 7) * 200000L);
	make_bytes(job->seed, job->output);
	job->ran = 1;
	if (job->hold)
		__atomic_store_n(&held_ran, 1, __ATOMIC_RELEASE);
	*bytes += JOB_BYTES;
	return !job->fail;
}

static void discard_job(void *data)
{
	discarded++;
	free(data);
}

/* count jobs through the pool, submitted as room allows and taken in order; fail_at (-1: none) fails */
static void run_through(int workers, int job_cap, unsigned int byte_cap, int count, int fail_at)
{
	int submitted = 0, taken = 0, jobs_most;
	unsigned int bytes_most;

	CHECK(ae_mcc_convert_pool_start(workers, job_cap, byte_cap) == workers);
	while (taken < count)
	{
		struct job *job;
		int result;

		while (submitted < count && ae_mcc_convert_pool_room(JOB_BYTES))
		{
			job = (struct job *)calloc(1, sizeof(*job));
			job->seed = (unsigned int)submitted + 1u;
			job->fail = submitted == fail_at;
			CHECK(ae_mcc_convert_pool_submit(run_job, job, JOB_BYTES));
			submitted++;
		}
		job = (struct job *)ae_mcc_convert_pool_oldest();
		CHECK(job != NULL);
		if (!job)
			break;
		CHECK(job->seed == (unsigned int)taken + 1u);
		result = ae_mcc_convert_pool_wait(10000, NULL);
		CHECK(result == (taken == fail_at ? 0 : 1));
		CHECK(ae_mcc_convert_pool_take() == job);
		{
			unsigned char expected[JOB_BYTES];

			make_bytes((unsigned int)taken + 1u, expected);
			CHECK(job->ran && !memcmp(job->output, expected, JOB_BYTES));
		}
		free(job);
		taken++;
	}
	CHECK(ae_mcc_convert_pool_oldest() == NULL);
	ae_mcc_convert_pool_held(NULL, NULL, &jobs_most, &bytes_most);
	CHECK(jobs_most <= job_cap);
	/* (past the first job, what is held stays under the cap: a job's output counts once it ran) */
	CHECK(bytes_most <= (byte_cap > 2u * JOB_BYTES ? byte_cap + 2u * JOB_BYTES : 2u * JOB_BYTES));
	CHECK(ae_mcc_convert_pool_stop(10000, discard_job) == 0);
}

int main(void)
{
	int workers;
	struct job *held, *queued, *finished;

	/* (in order, byte for byte, 1 to 4 workers; then a cap of one job; then a byte cap of one job) */
	for (workers = 1; workers <= 4; workers++)
		run_through(workers, workers * 2, 64u << 20, 40, -1);
	run_through(3, 1, 64u << 20, 12, -1);
	run_through(3, 8, JOB_BYTES, 12, -1);
	/* (a failed job: the first, the middle, the last) */
	run_through(2, 4, 64u << 20, 9, 0);
	run_through(2, 4, 64u << 20, 9, 4);
	run_through(2, 4, 64u << 20, 9, 8);

	/* (nothing to submit to, wait for or take before a start, or after a stop) */
	CHECK(!ae_mcc_convert_pool_room(1));
	CHECK(!ae_mcc_convert_pool_submit(run_job, NULL, 1));
	CHECK(ae_mcc_convert_pool_wait(1, NULL) == -2);
	CHECK(ae_mcc_convert_pool_take() == NULL);
	CHECK(!ae_mcc_convert_pool_start(0, 1, 1));

	/* (a wait past its deadline returns -1; the job still ends and is taken) */
	release_hold = 0;
	CHECK(ae_mcc_convert_pool_start(1, 2, 64u << 20) == 1);
	held = (struct job *)calloc(1, sizeof(*held));
	held->seed = 5;
	held->hold = 1;
	CHECK(ae_mcc_convert_pool_submit(run_job, held, JOB_BYTES));
	CHECK(ae_mcc_convert_pool_wait(30, NULL) == -1);
	CHECK(ae_mcc_convert_pool_take() == NULL);
	__atomic_store_n(&release_hold, 1, __ATOMIC_RELEASE);
	CHECK(ae_mcc_convert_pool_wait(10000, NULL) == 1);
	CHECK(ae_mcc_convert_pool_take() == held);
	free(held);
	CHECK(ae_mcc_convert_pool_stop(10000, discard_job) == 0);

	/* (the drain: a finished job not taken and a queued one are discarded, a running one waited for) */
	release_hold = 0;
	discarded = 0;
	held_ran = 0;
	CHECK(ae_mcc_convert_pool_start(1, 4, 64u << 20) == 1);
	finished = (struct job *)calloc(1, sizeof(*finished));
	finished->seed = 1;
	CHECK(ae_mcc_convert_pool_submit(run_job, finished, JOB_BYTES));
	CHECK(ae_mcc_convert_pool_wait(10000, NULL) == 1);
	held = (struct job *)calloc(1, sizeof(*held));
	held->seed = 2;
	held->hold = 1;
	CHECK(ae_mcc_convert_pool_submit(run_job, held, JOB_BYTES));
	queued = (struct job *)calloc(1, sizeof(*queued));
	queued->seed = 3;
	CHECK(ae_mcc_convert_pool_submit(run_job, queued, JOB_BYTES));
	nap(30000000);
	/* (the held job runs past this deadline: it is left to its worker, and no new pool starts until it ends) */
	CHECK(ae_mcc_convert_pool_stop(50, discard_job) == 1);
	CHECK(discarded == 2);
	CHECK(!ae_mcc_convert_pool_start(1, 1, 1));
	__atomic_store_n(&release_hold, 1, __ATOMIC_RELEASE);
	{
		int tries;

		for (tries = 0; tries < 1000 && !ae_mcc_convert_pool_start(2, 2, 64u << 20); tries++)
			nap(5000000);
		CHECK(tries < 1000);
	}
	/* (the next start let the held job's data go: it had ended) */
	CHECK(__atomic_load_n(&held_ran, __ATOMIC_ACQUIRE));
	CHECK(discarded == 3);
	CHECK(ae_mcc_convert_pool_stop(10000, discard_job) == 0);
	{
		int jobs;
		unsigned int bytes;

		ae_mcc_convert_pool_held(&jobs, &bytes, NULL, NULL);
		CHECK(jobs == 0 && bytes == 0);
	}

	if (failures)
		return 1;
	printf("ae_mcc_convert_pool: all checks passed\n");
	return 0;
}
