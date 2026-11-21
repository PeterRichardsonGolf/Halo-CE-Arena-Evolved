/*
AE_WORKER.C

A detached worker thread and a lock for AE's game-side units, over POSIX threads (ae_worker.h): the platform layer
is built with them on every platform (Windows: port/windows/include/posix, port/windows/src/win32_posix.c).
*/

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

void ae_worker_sleep(unsigned milliseconds)
{
	struct timespec duration;

	duration.tv_sec = (time_t)(milliseconds / 1000);
	duration.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	nanosleep(&duration, NULL);
}

struct ae_worker_signal
{
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	unsigned int wakes;
};

struct ae_worker_signal *ae_worker_signal_new(void)
{
	struct ae_worker_signal *signal = (struct ae_worker_signal *)calloc(1, sizeof(*signal));

	if (!signal)
		return NULL;
	if (pthread_mutex_init(&signal->mutex, NULL) != 0)
	{
		free(signal);
		return NULL;
	}
	if (pthread_cond_init(&signal->condition, NULL) != 0)
	{
		pthread_mutex_destroy(&signal->mutex);
		free(signal);
		return NULL;
	}
	return signal;
}

unsigned int ae_worker_signal_seen(struct ae_worker_signal *signal)
{
	unsigned int seen;

	if (!signal)
		return 0;
	pthread_mutex_lock(&signal->mutex);
	seen = signal->wakes;
	pthread_mutex_unlock(&signal->mutex);
	return seen;
}

void ae_worker_signal_wait(struct ae_worker_signal *signal, unsigned int seen, unsigned milliseconds)
{
	struct timespec until;

	if (!signal)
	{
		ae_worker_sleep(milliseconds);
		return;
	}
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_sec += (time_t)(milliseconds / 1000);
	until.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
	if (until.tv_nsec >= 1000000000L)
	{
		until.tv_sec++;
		until.tv_nsec -= 1000000000L;
	}
	pthread_mutex_lock(&signal->mutex);
	while (signal->wakes == seen)
	{
		if (pthread_cond_timedwait(&signal->condition, &signal->mutex, &until) != 0)
			break;
	}
	pthread_mutex_unlock(&signal->mutex);
}

void ae_worker_signal_wake(struct ae_worker_signal *signal)
{
	if (!signal)
		return;
	pthread_mutex_lock(&signal->mutex);
	signal->wakes++;
	pthread_cond_broadcast(&signal->condition);
	pthread_mutex_unlock(&signal->mutex);
}

unsigned int ae_worker_milliseconds(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (unsigned int)((unsigned long long)now.tv_sec * 1000ULL + (unsigned long long)(now.tv_nsec / 1000000L));
}

int ae_worker_cores(void)
{
	long cores = sysconf(_SC_NPROCESSORS_ONLN);

	return cores > 0 ? (int)cores : 1;
}

static pthread_t main_thread;
static volatile int main_known;

void ae_worker_main_set(void)
{
	main_thread = pthread_self();
	main_known = 1;
}

int ae_worker_on_main(void)
{
	return !main_known || pthread_equal(pthread_self(), main_thread);
}
