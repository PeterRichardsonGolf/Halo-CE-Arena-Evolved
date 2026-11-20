#include "harness.h"
/* port/windows/src/win32_server.c's posix_server_stdin_read when its reader thread cannot start: this read and every
later one say the input has ended (-1), so the console loop stops reading, rather than 0 (nothing yet) forever */

typedef void *HANDLE;
typedef unsigned long DWORD;
typedef struct { int unused; } CRITICAL_SECTION;
#define WINAPI
enum { INPUT_BUFFER_SIZE = 256 };
static CRITICAL_SECTION input_lock;
static char input_buffer[INPUT_BUFFER_SIZE];
static int input_length;
static int input_ended;
static int input_started;
static int thread_starts;
static DWORD WINAPI input_thread(void *unused) { return 0; }
static void InitializeCriticalSection(CRITICAL_SECTION *section) { }
static void EnterCriticalSection(CRITICAL_SECTION *section) { }
static void LeaveCriticalSection(CRITICAL_SECTION *section) { }
static void CloseHandle(HANDLE handle) { }
static HANDLE CreateThread(void *a, int b, DWORD (WINAPI *start)(void *), void *c, int d, void *e)
{
	thread_starts++;
	return NULL;
}
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	char buffer[16];

	CASE("no-thread")
	{
		CHECK(posix_server_stdin_read(buffer, sizeof(buffer)) == -1, "the first read");
		CHECK(posix_server_stdin_read(buffer, sizeof(buffer)) == -1, "a later read waits for input that never comes");
		CHECK(thread_starts == 1, "the thread started %d times", thread_starts);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
