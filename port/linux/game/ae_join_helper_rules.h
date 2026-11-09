/*
AE_JOIN_HELPER_RULES.H

The words of the join helper (0.7.5; mockups-075-toggles/final/6): what a refused join says and which one action it
offers. Pure C, for the unit test (port/linux/tests/ae_join_helper_rules_test.c). ae_join_helper.c shows them.
*/

#ifndef AE_JOIN_HELPER_RULES_H
#define AE_JOIN_HELPER_RULES_H

/* why a join did not happen */
enum
{
	AE_JH_GENERIC,          /* the words the game already had (detail) */
	AE_JH_UPDATE_PACKS,     /* the host's game uses weapon packs this copy has not */
	AE_JH_UPDATE_VERSION,   /* the host is on a newer version */
	AE_JH_UPDATE_MODE,      /* the host's game uses a mode this version has not */
	AE_JH_HOST_OLDER,       /* the host is on an older version: nothing to update here */
	AE_JH_WORKSHOP,         /* a Steam Workshop map this PC has not */
	AE_JH_MAP,              /* another map this PC has not, and cannot get */
	AE_JH_NO_ANSWER,
	AE_JH_FULL,
	AE_JH_CLOSED,
	AE_JH_PASSWORD,
	AE_JH_TURNED_AWAY,
	AE_JH_HOST_CLOSED,
	AE_JH_NOT_FOUND,
	AE_JH_NEEDS_MCC,        /* the host's game plays MCC's maps and this PC has not MCC's files (code 72) */
	AE_JH_MCC_OFF,          /* (0.7.9) code 72, but Steam's MCC is on this PC with its files turned off */
	AE_JH_AE_ONLY,          /* (0.7.9) code 71: the server takes Arena Evolved players only */
	AE_JH_NO_ID,            /* (0.7.9) code 73: a server behind a tunnel got no machine id from this PC */
	AE_JH_VPN_BLOCKED,      /* (0.7.9) code 9: the server lets no VPN in */
	AE_JH_DISCORD_NEEDED,   /* (0.7.9) codes 8 and 10: a Discord account is needed, or the one shown did not check */
	AE_JH_DISCORD_TOO_NEW,  /* (0.7.9) code 11 */
	AE_JH_DISCORD_IN_USE,   /* (0.7.9) code 12 */
	AE_JH_HELD,             /* (0.7.9) code 74: kicked a moment ago, held out; the minutes ride the code (detail) */
	AE_JH_NO_PLAYER,        /* (0.7.9) code 76: joined the lobby and added no player in time */
	AE_JH_ADMIN_SLOTS,      /* (0.7.9) code 77: the last places are kept for the server's admins */
	AE_JH_CAUSES
};

/* the one action a cause offers ((0.7.9) OPEN_STEAM: the Workshop item's page in the Steam app; GET_MCC: MCC's store
page in it) */
enum { AE_JH_ACTION_OK, AE_JH_ACTION_UPDATE, AE_JH_ACTION_AGAIN, AE_JH_ACTION_MCC_ON, AE_JH_ACTION_OPEN_STEAM,
	AE_JH_ACTION_GET_MCC };

/* MCC's store page in the Steam app (Halo: The Master Chief Collection is app 976730) */
#define AE_JH_MCC_STORE_LINK "steam://store/976730"

struct ae_join_helper_words
{
	char title[48];
	char reason[320];
	char note[120];
	char action[24];
	int action_kind;
	/* a second button ((0.7.9) a Workshop map's JOIN AGAIN, no MCC's NOT NOW), "" none; and what it does */
	char second[28];
	int second_kind;
};

/* the cause of a host's rejection code (network_game_protocol.h's, and AE's own 64 to 77; a held word's high byte is
its minutes: ae_silent_rules.h) */
int ae_join_helper_cause_of_rejection(int code);
/* the cause of a join failure's reason (ae_strings.h's AE_STR_ERR_* ids) */
int ae_join_helper_cause_of_reason(int reason);
/* the words of a cause; detail: the game's own words for AE_JH_GENERIC and AE_JH_MAP, the minutes ("7") for AE_JH_HELD (may
be NULL), map the Workshop
map's name (<name>@ws<item>) for AE_JH_WORKSHOP */
void ae_join_helper_words(int cause, char const *detail, char const *map, struct ae_join_helper_words *words);
/* the Workshop item's number in a map name ("name@ws2899770689" -> "2899770689"), "" when it has none */
void ae_join_helper_workshop_item(char const *map, char *item, int size);
/* the item's page for a browser ("" without an item) */
void ae_join_helper_workshop_link(char const *item, char *link, int size);
/* (0.7.9) the item's page in the Steam app, where Subscribe is one click (steam://openurl/ and the browser link; "" without
an item) */
void ae_join_helper_steam_link(char const *item, char *link, int size);

#endif
