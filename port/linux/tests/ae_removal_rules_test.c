/* ae_removal_rules.c: the title and sentence of each removal reason the server writes */
#include <stdio.h>
#include <string.h>

#include "ae_removal_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_removal_words words;

	CHECK(ae_removal_words_of("away for 5 minutes (AFK)", &words) == AE_REMOVAL_AFK);
	CHECK(!strcmp(words.title, "KICKED FOR BEING AWAY"));
	CHECK(!strcmp(words.body, "You were idle for 5 minutes, so the server made room. You can join again."));
	ae_removal_words_of("away for 1 minute (AFK)", &words);
	CHECK(strstr(words.body, "idle for 1 minute,"));
	/* (an older server's words) */
	CHECK(ae_removal_words_of("away too long (AFK)", &words) == AE_REMOVAL_AFK && strstr(words.body, "idle for too long"));

	CHECK(ae_removal_words_of("kicked by the host", &words) == AE_REMOVAL_KICKED);
	CHECK(!strcmp(words.title, "KICKED BY THE HOST") && strstr(words.body, "You can join again.") && !strstr(words.body, " in "));
	ae_removal_words_of("kicked by the host (rejoin in 1:58)", &words);
	CHECK(strstr(words.body, "You can join again in 1:58."));
	CHECK(ae_removal_wait_seconds("kicked by the host (rejoin in 1:58)") == 118);
	CHECK(ae_removal_wait_seconds("voted out by the other players (rejoin in 1:30:00)") == 5400);
	CHECK(ae_removal_wait_seconds("kicked by the host") == -1);
	CHECK(ae_removal_wait_seconds("x (rejoin in abc)") == -1 && ae_removal_wait_seconds("x (rejoin in 1:99)") == -1);
	CHECK(ae_removal_wait_seconds(NULL) == -1);

	CHECK(ae_removal_words_of("voted out by the other players (rejoin in 10:00)", &words) == AE_REMOVAL_VOTED);
	CHECK(!strcmp(words.title, "VOTED OUT") && strstr(words.body, "voted you out") && strstr(words.body, "in 10:00."));
	ae_removal_words_of("voted out by the other players", &words);
	CHECK(strstr(words.body, "You can join again."));

	CHECK(ae_removal_words_of("banned for 30 minutes", &words) == AE_REMOVAL_BANNED && !strcmp(words.title, "BANNED"));
	CHECK(strstr(words.body, "for 30 minutes."));
	ae_removal_words_of("banned for 3 hours", &words);
	CHECK(strstr(words.body, "for 3 hours."));
	ae_removal_words_of("banned for 1 minute", &words);
	CHECK(strstr(words.body, "for 1 minute."));
	ae_removal_words_of("banned from this server", &words);
	CHECK(strstr(words.body, "banned you from this server.") && !strstr(words.body, " for "));

	CHECK(ae_removal_words_of("your game ran too fast", &words) == AE_REMOVAL_SPEED);
	CHECK(!strcmp(words.title, "REMOVED FOR GAME SPEED"));

	/* (anything else keeps the old words) */
	CHECK(ae_removal_words_of("some new reason", &words) == AE_REMOVAL_OTHER);
	CHECK(!strcmp(words.title, "Removed") && !strcmp(words.body, "You were removed from the server: some new reason."));
	CHECK(ae_removal_words_of(NULL, &words) == AE_REMOVAL_OTHER);
	{
		char long_why[400];

		memset(long_why, 'a', sizeof(long_why) - 1);
		long_why[sizeof(long_why) - 1] = 0;
		ae_removal_words_of(long_why, &words);
		CHECK(strlen(words.body) < sizeof(words.body));
	}
	if (!failures)
		printf("ae_removal_rules_test: ok\n");
	return failures ? 1 : 0;
}
