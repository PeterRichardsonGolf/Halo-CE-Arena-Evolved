#ifndef AE_CRASH_DEADLINE_H
#define AE_CRASH_DEADLINE_H

/* a genuine crash's report (memory_watch.c's fault handler) writes debug.txt and walks the stack, neither of which is
safe in a signal handler: a crash inside malloc or stdio holds their locks, and the report waits on them forever (the
game, or a dedicated server, frozen rather than ended). The report is given seconds to finish: SIGALRM, at its
default (the dedicated server's own handler would exit 0, as for a quit's deadline), then ends the process, killed by
a signal, as a crash is. Only async-signal-safe calls. */
#include <signal.h>
#include <string.h>
#include <unistd.h>

static inline void ae_crash_deadline(unsigned int seconds)
{
	struct sigaction action;
	sigset_t alarm_only;

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, NULL);
	sigemptyset(&alarm_only);
	sigaddset(&alarm_only, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &alarm_only, NULL);
	alarm(seconds);
}

/* the stack walk's own setup (backtrace loads its unwinder, and allocates, on its first call) done before any crash
(backtrace as the including file declares it: execinfo.h, or the port's own where there is none) */
static inline void ae_crash_backtrace_ready(void)
{
	void *frame;

	backtrace(&frame, 1);
}

#endif
