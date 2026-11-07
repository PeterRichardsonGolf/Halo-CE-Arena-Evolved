/* ae_worker.c: the platform layer's detached worker and lock (the map facts' worker, ae_glue_setup_maps.c) */
#include <stdio.h>
#include <time.h>

#include "ae_worker.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

enum { WORKERS = 4, STEPS = 20000 };

static struct ae_worker_lock *lock;
static long counter;
static int finished;

static void *count(void *argument)
{
	int step;

	(void)argument;
	for (step = 0; step < STEPS; step++)
	{
		ae_worker_lock_take(lock);
		counter++;
		ae_worker_lock_give(lock);
	}
	ae_worker_lock_take(lock);
	finished++;
	ae_worker_lock_give(lock);
	return NULL;
}

int main(void)
{
	struct timespec pause = { 0, 1000000 };
	int index, waited, done;

	/* (no run: nothing started) */
	CHECK(ae_worker_start(NULL, NULL) == 0);
	/* (a NULL lock: take and give do nothing) */
	ae_worker_lock_take(NULL);
	ae_worker_lock_give(NULL);
	lock = ae_worker_lock_new();
	CHECK(lock != NULL);
	for (index = 0; index < WORKERS; index++)
		CHECK(ae_worker_start(count, NULL) == 1);
	/* (detached: waited for by their count, 10 s at most) */
	for (waited = 0, done = 0; waited < 10000; waited++)
	{
		ae_worker_lock_take(lock);
		done = finished;
		ae_worker_lock_give(lock);
		if (done == WORKERS)
			break;
		nanosleep(&pause, NULL);
	}
	CHECK(done == WORKERS);
	/* (every step under the lock: none lost) */
	CHECK(counter == (long)WORKERS * STEPS);
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
