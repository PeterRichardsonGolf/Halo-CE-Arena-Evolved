/*
AE_STEAM_POLICY.H

(AE) Small rules of the Steam client that are checked on their own (tools/test_steam_client.py): what a refused
token logon means, and the log's text.
*/

#ifndef AE_STEAM_POLICY_H
#define AE_STEAM_POLICY_H

#include "steam_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* what a refused token logon's EResult says of the saved sign-in (Steam's EResult list) */
enum
{
	/* the token is dead: 5 InvalidPassword, 15 AccessDenied, 26 Revoked, 27 Expired. It is forgotten. */
	STEAM_LOGON_DEAD = 0,
	/* Steam is limiting or cannot answer now: 3 NoConnection, 10 Busy, 16 Timeout, 20 ServiceUnavailable,
	25 LimitExceeded, 35-38 connect/handshake/IO/remote-disconnect, 44 ServiceReadOnly, 48 TryAnotherCM, 55
	RemoteCallFailed, 84 RateLimitExceeded. The sign-in is kept, and tried again later. */
	STEAM_LOGON_BUSY = 1,
	/* anything else (2 Fail, unknown): the sign-in is kept, the error shown */
	STEAM_LOGON_OTHER = 2
};

static inline int steam_logon_kind(int result)
{
	switch (result)
	{
	case 5:
	case 15:
	case 26:
	case 27:
		return STEAM_LOGON_DEAD;
	case 3:
	case 10:
	case 16:
	case 20:
	case 25:
	case 35:
	case 36:
	case 37:
	case 38:
	case 44:
	case 48:
	case 55:
	case 84:
		return STEAM_LOGON_BUSY;
	default:
		return STEAM_LOGON_OTHER;
	}
}

/* a Workshop item's install: refused when its files total more than 16 GB (real items run to a few GB, the CE Digsite Multiplayer item is 2.73 GB;
the free-space check and the manifest rules do the real work, this only bounds an absurd one), and when the disk has not their size, a tenth over and
64 MB. 0 go on, 1 too large, 2 no room */
#define STEAM_ITEM_MAXIMUM_BYTES ((unsigned long long)16 << 30)
enum { STEAM_ITEM_OK = 0, STEAM_ITEM_TOO_LARGE = 1, STEAM_ITEM_NO_ROOM = 2 };

static inline int steam_item_size_check(unsigned long long total, unsigned long long free_bytes)
{
	if (total > STEAM_ITEM_MAXIMUM_BYTES)
		return STEAM_ITEM_TOO_LARGE;
	if (free_bytes < total + total / 10 + ((unsigned long long)64 << 20))
		return STEAM_ITEM_NO_ROOM;
	return STEAM_ITEM_OK;
}

/* a sign-in cancelled after the password went out: Steam may still have begun it (ae_steam_rules.c shows these words
for a message that holds "deny it") */
#define STEAM_CANCELLED_SENT_MESSAGE "Signing in was cancelled. If Steam asks you to approve 'Arena Evolved', deny it."

#define STEAM_BUSY_MESSAGE "Steam is busy: try again in a few minutes."

/* text from the network made safe for a log line: a control character (a line break among them) or a byte
becomes "?" */
static inline void steam_log_clean(char *text)
{
	for (; *text; text++)
		if ((unsigned char)*text < 32 || (unsigned char)*text == 127)
			*text = '?';
}

/* the password for the logon: one heap buffer (never the stack, which a crash dump holds), filled by get at the
last moment (once the account's RSA key is in), and wiped by steam_password_release as soon as it is encrypted.
NULL when get refuses */
#define STEAM_PASSWORD_BUFFER 256

static inline char *steam_password_take(int (*get)(char *out, size_t size))
{
	char *password = calloc(1, STEAM_PASSWORD_BUFFER);

	if (password && !get(password, STEAM_PASSWORD_BUFFER))
	{
		steam_wipe(password, STEAM_PASSWORD_BUFFER);
		free(password);
		password = NULL;
	}
	return password;
}

static inline void steam_password_release(char *password)
{
	if (!password)
		return;
	steam_wipe(password, STEAM_PASSWORD_BUFFER);
	free(password);
}

/* a sign-in cancelled or timed out after the password went out: Steam may still have begun it */
#define STEAM_TIMEOUT_SENT_MESSAGE "Steam did not answer in time. If Steam asks you to approve 'Arena Evolved', deny it."

/* ---------- the back-off after a busy Steam: 60 s, doubling to 10 min; every logon (a queued download's, a sign-in)
waits it out, and a success clears it. now is steam_milliseconds(); all in milliseconds */
#define STEAM_BACKOFF_FIRST 60000ul
#define STEAM_BACKOFF_LONGEST 600000ul

struct steam_backoff
{
	unsigned long until;
	unsigned long step; /* 0: none recorded */
};

static inline void steam_backoff_busy(struct steam_backoff *backoff, unsigned long now)
{
	backoff->step = !backoff->step ? STEAM_BACKOFF_FIRST :
		backoff->step * 2ul > STEAM_BACKOFF_LONGEST ? STEAM_BACKOFF_LONGEST : backoff->step * 2ul;
	backoff->until = now + backoff->step;
}

/* the milliseconds left to wait: 0 when none */
static inline unsigned long steam_backoff_remaining(struct steam_backoff const *backoff, unsigned long now)
{
	return backoff->step && (long)(backoff->until - now) > 0 ? backoff->until - now : 0ul;
}

static inline void steam_backoff_clear(struct steam_backoff *backoff)
{
	backoff->step = 0;
	backoff->until = 0;
}

/* "Steam is busy: try again in 2 minutes." (the time left, rounded up) */
static inline void steam_busy_text(char *out, size_t size, unsigned long remaining)
{
	unsigned long seconds = (remaining + 999ul) / 1000ul;
	unsigned long minutes;

	if (!seconds)
		seconds = 1ul;
	minutes = (seconds + 59ul) / 60ul;
	if (seconds < 60ul)
		snprintf(out, size, "Steam is busy: try again in %lu second%s.", seconds, seconds == 1ul ? "" : "s");
	else
		snprintf(out, size, "Steam is busy: try again in %lu minute%s.", minutes, minutes == 1ul ? "" : "s");
}

/* whether the saved session file has a sealed sign-in in it (a "sealed=" line): then a lost secret-store key must
never be answered with a new key, which would leave that sign-in unreadable for good */
static inline int steam_session_file_sealed(char const *path)
{
	char line[16];
	FILE *file = fopen(path, "r");
	int found = 0, start = 1;

	if (!file)
		return 0;
	while (fgets(line, sizeof(line), file))
	{
		if (start && !strncmp(line, "sealed=", 7))
			found = 1;
		start = strchr(line, '\n') != NULL;
	}
	fclose(file);
	return found;
}

#endif
