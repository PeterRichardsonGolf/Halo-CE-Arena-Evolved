#include "harness.h"
#include <setjmp.h>
/* server_netlist.c's netlist thread after sv_vpn_fetch (server_netlist_fetch_now) on a server publishing to several
masters: each master's netlist asked once, then the thread waits; with one master the same. The real thread loop
runs here: fetch_one records which master it would ask, the wait ends the run. */

typedef struct SDL_Mutex SDL_Mutex;
typedef struct SDL_Condition SDL_Condition;
typedef struct SDL_Thread SDL_Thread;
#define SDLCALL
#include "config.inc"

static jmp_buf done;
static int asked[16], asked_count, waits;
static unsigned long long clock_now = 100000;

static unsigned long long now_seconds(void) { return clock_now; }
static void SDL_LockMutex(SDL_Mutex *mutex) { }
static void SDL_UnlockMutex(SDL_Mutex *mutex) { }
static void SDL_BroadcastCondition(SDL_Condition *condition) { }
static void SDL_WaitConditionTimeout(SDL_Condition *condition, SDL_Mutex *mutex, int ms)
{
	waits++;
	longjmp(done, 1);
}
static void fetch_one(int index)
{
	CHECK(asked_count < 8, "the thread keeps asking without a wait: masters asked %d times", asked_count);
	asked[asked_count++] = index;
}
#include "under_test.inc"

static void run_thread(int masters)
{
	int index;

	netlist.started = 1;
	netlist.master_count = masters;
	for (index = 0; index < masters; index++)
		netlist.masters[index].next_fetch = clock_now + 3000;
	asked_count = waits = 0;
	server_netlist_fetch_now();
	if (!setjmp(done))
		netlist_thread_main(NULL);
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	int index;

	CASE("two-masters")
	{
		run_thread(2);
		CHECK(asked_count == 2 && asked[0] == 0 && asked[1] == 1, "asked %d times, first %d", asked_count, asked[0]);
		CHECK(waits == 1, "no wait after each master was asked once");
		return 0;
	}
	CASE("four-masters")
	{
		run_thread(4);
		CHECK(asked_count == 4, "asked %d times", asked_count);
		for (index = 0; index < 4; index++)
			CHECK(asked[index] == index, "master %d asked as %d", index, asked[index]);
		return 0;
	}
	CASE("one-master")
	{
		run_thread(1);
		CHECK(asked_count == 1 && asked[0] == 0, "asked %d times", asked_count);
		return 0;
	}
	CASE("not-due")
	{
		/* with no fetch_now, nothing due: straight to the wait */
		netlist.started = 1;
		netlist.master_count = 2;
		netlist.masters[0].next_fetch = netlist.masters[1].next_fetch = clock_now + 3000;
		asked_count = waits = 0;
		if (!setjmp(done))
			netlist_thread_main(NULL);
		CHECK(asked_count == 0 && waits == 1, "asked %d before its time", asked_count);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
