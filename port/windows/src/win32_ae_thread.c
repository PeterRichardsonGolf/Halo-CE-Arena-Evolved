/*
WIN32_AE_THREAD.C

pthread_join, which the Steam client (port/android/host/steam_*.c) uses: win32_posix.c's pthread_create reserves a slot here
first and hands the thread's handle to it (thread_slot_commit), and a join waits on that handle. (It used to open the thread again by its
identifier, which Windows may give to another thread once the first has ended: the join could then wait for the wrong
one for good.) A detached thread (PTHREAD_CREATE_DETACHED) takes no slot. When all 64 slots hold running threads, pthread_create
fails with EAGAIN, and no slot of a running thread is ever freed. A thread with no slot (joined, detached, or
ended and cleared) counts as ended.
*/

#include <windows.h>

#include "pthread.h"

enum { HANDLES = 64 };

/* a slot is free (used 0), reserved for a thread being made (used 1, no handle yet), or holds a joinable thread's
handle (used 1) */
static struct
{
	int used;
	DWORD identifier;
	HANDLE handle;
} kept[HANDLES];
static SRWLOCK kept_lock = SRWLOCK_INIT;

/* (the table's lock held) the slots of threads that have ended and were never joined, freed: a join of one of them
afterwards finds none, which is a thread joined. A running thread's slot is never freed */
static void kept_reclaim(void)
{
	int index;

	for (index = 0; index < HANDLES; index++)
		if (kept[index].used && kept[index].handle && WaitForSingleObject(kept[index].handle, 0) == WAIT_OBJECT_0)
		{
			CloseHandle(kept[index].handle);
			kept[index].handle = NULL;
			kept[index].used = 0;
		}
}

/* a slot for a thread about to be made: its index, or -1 when every slot holds a running thread (pthread_create then
fails with EAGAIN: no thread is made that nobody could join) */
int thread_slot_reserve(void)
{
	int index, found = -1;

	AcquireSRWLockExclusive(&kept_lock);
	for (index = 0; index < HANDLES && found < 0; index++)
		if (!kept[index].used)
			found = index;
	if (found < 0)
	{
		kept_reclaim();
		for (index = 0; index < HANDLES && found < 0; index++)
			if (!kept[index].used)
				found = index;
	}
	if (found >= 0)
	{
		kept[found].used = 1;
		kept[found].handle = NULL;
		kept[found].identifier = 0;
	}
	ReleaseSRWLockExclusive(&kept_lock);
	return found;
}

/* the reserved slot given up (the thread could not be made) */
void thread_slot_cancel(int slot)
{
	AcquireSRWLockExclusive(&kept_lock);
	kept[slot].used = 0;
	ReleaseSRWLockExclusive(&kept_lock);
}

/* the thread made: its handle owned by the slot, closed by a join, a detach, or once the thread has ended and the table
needs room */
void thread_slot_commit(int slot, DWORD identifier, HANDLE handle)
{
	int index;

	AcquireSRWLockExclusive(&kept_lock);
	/* (an identifier of an ended thread, made again: the old slot was that thread's, which is gone) */
	for (index = 0; index < HANDLES; index++)
		if (index != slot && kept[index].used && kept[index].handle && kept[index].identifier == identifier)
		{
			CloseHandle(kept[index].handle);
			kept[index].handle = NULL;
			kept[index].used = 0;
		}
	kept[slot].identifier = identifier;
	kept[slot].handle = handle;
	ReleaseSRWLockExclusive(&kept_lock);
}

/* the handle of a thread taken out of the table, or NULL */
static HANDLE thread_handle_take(DWORD identifier)
{
	HANDLE handle = NULL;
	int index;

	AcquireSRWLockExclusive(&kept_lock);
	for (index = 0; index < HANDLES; index++)
		if (kept[index].used && kept[index].handle && kept[index].identifier == identifier)
		{
			handle = kept[index].handle;
			kept[index].handle = NULL;
			kept[index].used = 0;
			break;
		}
	ReleaseSRWLockExclusive(&kept_lock);
	return handle;
}

void thread_handle_release(DWORD identifier)
{
	HANDLE handle = thread_handle_take(identifier);

	if (handle)
		CloseHandle(handle);
}

int pthread_join(pthread_t thread, void **result)
{
	HANDLE handle = thread_handle_take((DWORD)thread);

	if (result)
		*result = NULL;
	if (handle)
	{
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}
	return 0;
}
