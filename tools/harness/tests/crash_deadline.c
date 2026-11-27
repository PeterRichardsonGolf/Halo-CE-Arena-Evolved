#include "harness.h"
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
/* port/linux/src/ae_crash_deadline.h: a crash's report that waits forever (on a lock the crash holds) is ended by
SIGALRM at its default, killed by the signal, even where the dedicated server's own SIGALRM handler (which exits 0,
as for a quit's deadline) is installed and SIGALRM is blocked */

#include "under_test.inc"

static void server_alarm(int number) { (void)number; _exit(0); }

static void crash_report(int number)
{
	(void)number;
	ae_crash_deadline(1);
	for (;;)
		pause();
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	struct sigaction action;
	sigset_t blocked;

	CASE("stuck-report")
	{
		memset(&action, 0, sizeof(action));
		action.sa_handler = server_alarm;
		sigfillset(&action.sa_mask);
		sigaction(SIGALRM, &action, NULL);
		sigemptyset(&blocked);
		sigaddset(&blocked, SIGALRM);
		sigprocmask(SIG_BLOCK, &blocked, NULL);
		memset(&action, 0, sizeof(action));
		action.sa_handler = crash_report;
		action.sa_flags = SA_NODEFER;
		sigaction(SIGSEGV, &action, NULL);
		raise(SIGSEGV);
		return 3;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
