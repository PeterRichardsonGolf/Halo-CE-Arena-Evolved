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
	CHECK(ae_join_helper_cause_of_rejection(72) == AE_JH_NEEDS_MCC);
	/* (0.7.9) MCC here but off: its own words and the one button that turns it on */
	{
		struct ae_join_helper_words mcc_off;

		ae_join_helper_words(AE_JH_MCC_OFF, NULL, NULL, &mcc_off);
		CHECK(!strcmp(mcc_off.title, "MCC FILES ARE OFF"));
		CHECK(mcc_off.action_kind == AE_JH_ACTION_MCC_ON && !strcmp(mcc_off.action, "USE MCC FILES"));
		CHECK(mcc_off.reason[0] && !mcc_off.second[0]);
	}
	/* (0.7.9) the codes that had no words of their own: VPN and Discord (8-12), the Arena Evolved audience (71), no machine
	id (73), a hold with its minutes (74), no player in time (76), the admins' places (77); grenades (75) and rounds (68) */
	CHECK(ae_join_helper_cause_of_rejection(7) == AE_JH_TURNED_AWAY && ae_join_helper_cause_of_rejection(13) == AE_JH_TURNED_AWAY);
	CHECK(ae_join_helper_cause_of_rejection(8) == AE_JH_DISCORD_NEEDED && ae_join_helper_cause_of_rejection(10) == AE_JH_DISCORD_NEEDED);
	CHECK(ae_join_helper_cause_of_rejection(9) == AE_JH_VPN_BLOCKED);
	CHECK(ae_join_helper_cause_of_rejection(11) == AE_JH_DISCORD_TOO_NEW);
	CHECK(ae_join_helper_cause_of_rejection(12) == AE_JH_DISCORD_IN_USE);
	CHECK(ae_join_helper_cause_of_rejection(71) == AE_JH_AE_ONLY);
	CHECK(ae_join_helper_cause_of_rejection(73) == AE_JH_NO_ID);
	CHECK(ae_join_helper_cause_of_rejection(74 | (2 << 8)) == AE_JH_HELD);
	CHECK(ae_join_helper_cause_of_rejection(74) == AE_JH_GENERIC);
	CHECK(ae_join_helper_cause_of_rejection(75) == AE_JH_UPDATE_MODE);
	CHECK(ae_join_helper_cause_of_rejection(76) == AE_JH_NO_PLAYER);
	CHECK(ae_join_helper_cause_of_rejection(77) == AE_JH_ADMIN_SLOTS);
	ae_join_helper_words(AE_JH_HELD, "2", NULL, &words);
	CHECK(strstr(words.reason, "2 more minutes") && words.action_kind == AE_JH_ACTION_AGAIN);
	ae_join_helper_words(AE_JH_HELD, "1", NULL, &words);
	CHECK(strstr(words.reason, "1 more minute."));
	ae_join_helper_words(AE_JH_VPN_BLOCKED, NULL, NULL, &words);
	CHECK(strstr(words.reason, "VPN"));
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
	/* a Workshop map (0.7.9): OPEN IN STEAM first, JOIN AGAIN second, only when the map names its item */
	ae_join_helper_words(AE_JH_WORKSHOP, NULL, "frigid keep@ws2899770689", &words);
	CHECK(!strcmp(words.title, "GET THE MAP FIRST") && !strcmp(words.action, "OPEN IN STEAM") &&
		words.action_kind == AE_JH_ACTION_OPEN_STEAM && !strcmp(words.second, "JOIN AGAIN") &&
		words.second_kind == AE_JH_ACTION_AGAIN);
	ae_join_helper_words(AE_JH_WORKSHOP, NULL, "frigid keep", &words);
	CHECK(!words.second[0] && !strcmp(words.action, "JOIN AGAIN") && words.action_kind == AE_JH_ACTION_AGAIN);
	/* no MCC (0.7.9): GET MCC opens its store page, NOT NOW closes */
	ae_join_helper_words(AE_JH_NEEDS_MCC, NULL, NULL, &words);
	CHECK(!strcmp(words.action, "GET MCC") && words.action_kind == AE_JH_ACTION_GET_MCC &&
		!strcmp(words.second, "NOT NOW") && words.second_kind == AE_JH_ACTION_OK);
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
	/* the Steam app's own page for it (a browser link inside steam://openurl/) */
	ae_join_helper_steam_link("2899770689", link, (int)sizeof(link));
	CHECK(!strcmp(link, "steam://openurl/https://steamcommunity.com/sharedfiles/filedetails/?id=2899770689"));
	ae_join_helper_steam_link("", link, (int)sizeof(link));
	CHECK(!link[0]);
	CHECK(!strcmp(AE_JH_MCC_STORE_LINK, "steam://store/976730"));
	ae_join_helper_workshop_item("a@ws123456789012345678901234567890", item, 6);
	CHECK(strlen(item) == 5);
	if (failures)
		return 1;
	printf("ae_join_helper_rules ok\n");
	return 0;
}
