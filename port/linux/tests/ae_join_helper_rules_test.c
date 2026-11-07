/* ae_join_helper_rules.c: the join helper's words and the action each cause offers */
#include <stdio.h>
#include <string.h>

#include "ae_join_helper_rules.h"
#include "ae_strings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_join_helper_words words;
	char item[24], link[120];
	int cause;

	/* every host rejection code and AE's own */
	CHECK(ae_join_helper_cause_of_rejection(0) == AE_JH_UPDATE_VERSION);
	CHECK(ae_join_helper_cause_of_rejection(1) == AE_JH_HOST_OLDER);
	CHECK(ae_join_helper_cause_of_rejection(3) == AE_JH_PASSWORD);
	CHECK(ae_join_helper_cause_of_rejection(4) == AE_JH_FULL);
	CHECK(ae_join_helper_cause_of_rejection(5) == AE_JH_CLOSED);
	CHECK(ae_join_helper_cause_of_rejection(6) == AE_JH_TURNED_AWAY);
	CHECK(ae_join_helper_cause_of_rejection(13) == AE_JH_TURNED_AWAY);
	CHECK(ae_join_helper_cause_of_rejection(64) == AE_JH_UPDATE_MODE);
	CHECK(ae_join_helper_cause_of_rejection(65) == AE_JH_UPDATE_PACKS);
	CHECK(ae_join_helper_cause_of_rejection(66) == AE_JH_UPDATE_PACKS);
	CHECK(ae_join_helper_cause_of_rejection(67) == AE_JH_UPDATE_MODE);
	CHECK(ae_join_helper_cause_of_rejection(68) == AE_JH_UPDATE_MODE);
	CHECK(ae_join_helper_cause_of_rejection(69) == AE_JH_UPDATE_PACKS);
	CHECK(ae_join_helper_cause_of_rejection(69 | (1 << 8)) == AE_JH_UPDATE_PACKS);
	CHECK(ae_join_helper_cause_of_rejection(69 | (5 << 8)) == AE_JH_UPDATE_PACKS);
	CHECK(ae_join_helper_cause_of_rejection(99) == AE_JH_GENERIC);
	CHECK(ae_join_helper_cause_of_rejection(-1) == AE_JH_GENERIC);
	CHECK(ae_join_helper_cause_of_reason(AE_STR_ERR_NO_ANSWER) == AE_JH_NO_ANSWER);
	CHECK(ae_join_helper_cause_of_reason(AE_STR_ERR_VERSION) == AE_JH_UPDATE_VERSION);
	CHECK(ae_join_helper_cause_of_reason(AE_STR_ERR_HOST_UPDATE) == AE_JH_HOST_OLDER);
	CHECK(ae_join_helper_cause_of_reason(AE_STR_ERR_NOT_INVITE) == AE_JH_GENERIC);
	/* every cause has a title, a reason and one action; the updates offer UPDATE, the retries an again */
	for (cause = 0; cause < AE_JH_CAUSES; cause++)
	{
		ae_join_helper_words(cause, "words", "map@ws2899770689", &words);
		CHECK(words.title[0] && words.reason[0] && words.action[0]);
	}
	ae_join_helper_words(AE_JH_UPDATE_PACKS, NULL, NULL, &words);
	CHECK(words.action_kind == AE_JH_ACTION_UPDATE && !strcmp(words.action, "UPDATE") && !words.second[0]);
	ae_join_helper_words(AE_JH_NO_ANSWER, NULL, NULL, &words);
	CHECK(words.action_kind == AE_JH_ACTION_AGAIN && !strcmp(words.action, "RETRY"));
	ae_join_helper_words(AE_JH_FULL, NULL, NULL, &words);
	CHECK(words.action_kind == AE_JH_ACTION_AGAIN && !strcmp(words.action, "TRY AGAIN"));
	/* a Workshop map: JOIN AGAIN and the link button, only when the map names its item */
	ae_join_helper_words(AE_JH_WORKSHOP, NULL, "frigid keep@ws2899770689", &words);
	CHECK(!strcmp(words.title, "GET THE MAP FIRST") && !strcmp(words.action, "JOIN AGAIN") &&
		!strcmp(words.second, "COPY WORKSHOP LINK"));
	ae_join_helper_words(AE_JH_WORKSHOP, NULL, "frigid keep", &words);
	CHECK(!words.second[0]);
	/* the game's own words kept for the generic cause and a map */
	ae_join_helper_words(AE_JH_GENERIC, "Internet play is off.", NULL, &words);
	CHECK(!strcmp(words.reason, "Internet play is off.") && words.action_kind == AE_JH_ACTION_OK);
	ae_join_helper_words(AE_JH_MAP, "Needs maps_pe/badcreek.map to join", NULL, &words);
	CHECK(strstr(words.reason, "badcreek") != NULL);
	/* the link */
	ae_join_helper_workshop_item("frigid keep@ws2899770689", item, (int)sizeof(item));
	CHECK(!strcmp(item, "2899770689"));
	ae_join_helper_workshop_link(item, link, (int)sizeof(link));
	CHECK(!strcmp(link, "https://steamcommunity.com/sharedfiles/filedetails/?id=2899770689"));
	ae_join_helper_workshop_item("blood gulch", item, (int)sizeof(item));
	CHECK(!item[0]);
	ae_join_helper_workshop_link("", link, (int)sizeof(link));
	CHECK(!link[0]);
	ae_join_helper_workshop_item("a@ws123456789012345678901234567890", item, 6);
	CHECK(strlen(item) == 5);
	if (failures)
		return 1;
	printf("ae_join_helper_rules ok\n");
	return 0;
}
