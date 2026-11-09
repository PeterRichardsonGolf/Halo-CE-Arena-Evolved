/*
AE_REMOVAL_RULES.C

The words of a removal (ae_removal_rules.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_removal_rules.h"

#define REJOIN_MARK "(rejoin in "

static int starts_with(char const *text, char const *prefix)
{
	return !strncmp(text, prefix, strlen(prefix));
}

long ae_removal_wait_seconds(char const *why)
{
	char const *at = why ? strstr(why, REJOIN_MARK) : NULL;
	char *end;
	long minutes, seconds;

	if (!at)
		return -1;
	at += strlen(REJOIN_MARK);
	minutes = strtol(at, &end, 10);
	if (end == at || *end != ':' || minutes < 0 || minutes > 9999)
		return -1;
	at = end + 1;
	seconds = strtol(at, &end, 10);
	if (end == at || seconds < 0 || seconds > 59)
		return -1;
	/* (H:MM:SS for a wait of an hour or more) */
	if (*end == ':')
	{
		long last;

		at = end + 1;
		last = strtol(at, &end, 10);
		if (end == at || last < 0 || last > 59 || seconds > 59)
			return -1;
		return minutes * 3600 + seconds * 60 + last;
	}
	return minutes * 60 + seconds;
}

/* "You can join again." with the wait when the server gave one (a minute and seconds, as the host's pause screen shows it) */
static void rejoin_sentence(long wait, char *out, int size)
{
	if (wait >= 3600)
		snprintf(out, (size_t)size, "You can join again in %ld:%02ld:%02ld.", wait / 3600, wait / 60 % 60, wait % 60);
	else if (wait > 0)
		snprintf(out, (size_t)size, "You can join again in %ld:%02ld.", wait / 60, wait % 60);
	else
		snprintf(out, (size_t)size, "You can join again.");
}

int ae_removal_words_of(char const *why, struct ae_removal_words *words)
{
	char again[48];

	memset(words, 0, sizeof(*words));
	if (!why)
		why = "";
	rejoin_sentence(ae_removal_wait_seconds(why), again, (int)sizeof(again));
	if (starts_with(why, "away "))
	{
		/* ("away for 5 minutes (AFK)"; an older server said "away too long (AFK)") */
		long minutes = starts_with(why, "away for ") ? atol(why + 9) : 0;

		words->kind = AE_REMOVAL_AFK;
		snprintf(words->title, sizeof(words->title), "KICKED FOR BEING AWAY");
		if (minutes > 0)
			snprintf(words->body, sizeof(words->body), "You were idle for %ld minute%s, so the server made room. You can "
				"join again.", minutes, minutes == 1 ? "" : "s");
		else
			snprintf(words->body, sizeof(words->body), "You were idle for too long, so the server made room. You can "
				"join again.");
	}
	else if (starts_with(why, "kicked by the host"))
	{
		words->kind = AE_REMOVAL_KICKED;
		snprintf(words->title, sizeof(words->title), "KICKED BY THE HOST");
		snprintf(words->body, sizeof(words->body), "An admin of this game removed you. %s", again);
	}
	else if (starts_with(why, "voted out"))
	{
		words->kind = AE_REMOVAL_VOTED;
		snprintf(words->title, sizeof(words->title), "VOTED OUT");
		snprintf(words->body, sizeof(words->body), "The other players voted you out of this game. %s", again);
	}
	else if (starts_with(why, "banned for ") || starts_with(why, "banned from"))
	{
		long amount = starts_with(why, "banned for ") ? atol(why + 11) : 0;

		words->kind = AE_REMOVAL_BANNED;
		snprintf(words->title, sizeof(words->title), "BANNED");
		if (amount > 0)
			snprintf(words->body, sizeof(words->body), "An admin banned you from this server for %ld %s. You can join "
				"again when the ban ends.", amount, strstr(why, "hour") ? (amount == 1 ? "hour" : "hours") :
				(amount == 1 ? "minute" : "minutes"));
		else
			snprintf(words->body, sizeof(words->body), "An admin banned you from this server. Ask its admins if you "
				"want back in.");
	}
	else if (starts_with(why, "your game ran too fast"))
	{
		words->kind = AE_REMOVAL_SPEED;
		snprintf(words->title, sizeof(words->title), "REMOVED FOR GAME SPEED");
		snprintf(words->body, sizeof(words->body), "Your game ran faster than the server's clock allows, so the server "
			"removed you and keeps you out for now.");
	}
	else
	{
		words->kind = AE_REMOVAL_OTHER;
		snprintf(words->title, sizeof(words->title), "Removed");
		snprintf(words->body, sizeof(words->body), "You were removed from the server: %s.", why);
	}
	return words->kind;
}
