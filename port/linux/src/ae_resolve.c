/*
AE_RESOLVE.C

See ae_resolve.h. The lookup runs on a detached thread that owns a copy of the name; the shared bookkeeping
(game/ae_resolve_state.c) is guarded by one mutex.
*/

#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ae_resolve.h"
#include "posix.h"
#include "../game/ae_resolve_state.h"

struct lookup
{
	unsigned long ticket;
	char host[128];
};

static struct ae_resolve_state state;

#if defined(HALO_ANDROID) || defined(__EMSCRIPTEN__)
/* (Android's guest and the web build have no SDL threads: the lookup runs where it is asked, as before 0.7.0) */
#define AE_RESOLVE_NO_THREADS 1
static int lock = 0;
#define SDL_LockMutex(mutex) ((void)(mutex))
#define SDL_UnlockMutex(mutex) ((void)(mutex))

static int lock_get(void)
{
	if (!lock)
	{
		lock = 1;
		ae_resolve_state_init(&state);
	}
	return 1;
}
#else
static SDL_Mutex *lock;

static int lock_get(void)
{
	if (!lock)
	{
		lock = SDL_CreateMutex();
		ae_resolve_state_init(&state);
	}
	return lock != NULL;
}
#endif

static int lookup_thread(void *data)
{
	struct lookup *lookup = data;
	unsigned long address = (unsigned long)posix_resolve_ipv4(lookup->host);

	SDL_LockMutex(lock);
	ae_resolve_state_finish(&state, lookup->ticket, address);
	SDL_UnlockMutex(lock);
	free(lookup);
	return 0;
}

int ae_resolve_start(const char *host)
{
	struct lookup *lookup;
	SDL_Thread *thread;

	if (!host || !host[0] || strlen(host) >= sizeof(lookup->host) || !lock_get())
		return 0;
	lookup = calloc(1, sizeof(*lookup));
	if (!lookup)
		return 0;
	memcpy(lookup->host, host, strlen(host));
	SDL_LockMutex(lock);
	lookup->ticket = ae_resolve_state_begin(&state);
	SDL_UnlockMutex(lock);
#ifdef AE_RESOLVE_NO_THREADS
	thread = NULL;
#else
	thread = SDL_CreateThread(lookup_thread, "name lookup", lookup);
#endif
	if (!thread)
	{
		/* (no thread: looked up here, as before) */
		lookup_thread(lookup);
		return 1;
	}
#ifndef AE_RESOLVE_NO_THREADS
	SDL_DetachThread(thread);
#endif
	return 1;
}

int ae_resolve_poll(unsigned long *address)
{
	int result;

	if (!lock_get())
		return -1;
	SDL_LockMutex(lock);
	result = ae_resolve_state_poll(&state, address);
	SDL_UnlockMutex(lock);
	return result;
}

void ae_resolve_cancel(void)
{
	if (!lock_get())
		return;
	SDL_LockMutex(lock);
	ae_resolve_state_cancel(&state);
	SDL_UnlockMutex(lock);
}
