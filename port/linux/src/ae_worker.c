/*
AE_WORKER.C

A detached worker thread and a lock for AE's game-side units, over POSIX threads (ae_worker.h): the platform layer
is built with them on every platform (Windows: port/windows/include/posix, port/windows/src/win32_posix.c).
*/

#include <pthread.h>
#include <stdlib.h>

#include "ae_worker.h"

struct ae_worker_lock
{
	pthread_mutex_t mutex;
};

struct ae_worker_lock *ae_worker_lock_new(void)
{
	struct ae_worker_lock *lock = (struct ae_worker_lock *)calloc(1, sizeof(*lock));

	if (lock && pthread_mutex_init(&lock->mutex, NULL) != 0)
	{
		free(lock);
		lock = NULL;
	}
	return lock;
}

void ae_worker_lock_take(struct ae_worker_lock *lock)
{
	if (lock)
		pthread_mutex_lock(&lock->mutex);
}

void ae_worker_lock_give(struct ae_worker_lock *lock)
{
	if (lock)
		pthread_mutex_unlock(&lock->mutex);
}

int ae_worker_start(void *(*run)(void *), void *argument)
{
	pthread_t thread;

	if (!run || pthread_create(&thread, NULL, run, argument) != 0)
		return 0;
	pthread_detach(thread);
	return 1;
}
