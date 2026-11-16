/*
AE_JOIN_HELPER_RULES.C

The words of the join helper (ae_join_helper_rules.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_join_helper_rules.h"
#include "ae_silent_rules.h"
#include "ae_strings.h"

int ae_join_helper_cause_of_rejection(int code)
{
	/* (69: the weapon cache refused this copy, with its case in the high byte: ae_silent_rules.h; update the packs) */
	if (code > 0 && ae_cache_rejection_case((unsigned int)code) >= 0)
		return AE_JH_UPDATE_PACKS;
	/* (74: held out for the minutes in the high byte) */
	if (code > 0 && ae_held_rejection_minutes((unsigned int)code) >= 0)
		return AE_JH_HELD;
	switch (code)
	{
	case 0: return AE_JH_UPDATE_VERSION;       /* version too old: this copy is the old one */
	case 1: return AE_JH_HOST_OLDER;           /* version too new: the host's is */
	/* 2 (a join token the host refused): the modded discs' own words, ae_disc_join.c */
	case 3: return AE_JH_PASSWORD;
	case 4: return AE_JH_FULL;
	case 5: return AE_JH_CLOSED;
	case 6: case 7: case 13:
		return AE_JH_TURNED_AWAY;
	/* (0.7.9: the VPN policy's own words, network_game_protocol.h: 8 no pass shown, 9 no VPN, 10 the pass did not check,
	11 a young account, 12 the account is in the game already) */
	case 8: case 10: return AE_JH_DISCORD_NEEDED;
	case 9: return AE_JH_VPN_BLOCKED;
	case 11: return AE_JH_DISCORD_TOO_NEW;
	case 12: return AE_JH_DISCORD_IN_USE;
	/* (73: a tunneled server got no hardware id from this copy, ae_silent_rules.h) */
	case 73: return AE_JH_NO_ID;
	/* (68: the rounds' refusal, 75 AE grenade types: ae_silent_rules.h) */
	case 64: case 67: case 68: case 75: return AE_JH_UPDATE_MODE;
	/* (71: the playlist takes Arena Evolved players only: not a version to update to) */
	case 71: return AE_JH_AE_ONLY;
	case 76: return AE_JH_NO_PLAYER;
	case 77: return AE_JH_ADMIN_SLOTS;
	case 65: case 66: return AE_JH_UPDATE_PACKS;
	case 72: return AE_JH_NEEDS_MCC;
	default: return AE_JH_GENERIC;
	}
}

int ae_join_helper_cause_of_reason(int reason)
{
	switch (reason)
	{
	case AE_STR_ERR_VERSION: case AE_STR_ERR_INVITE_VERSION: return AE_JH_UPDATE_VERSION;
	case AE_STR_ERR_HOST_UPDATE: return AE_JH_HOST_OLDER;
	case AE_STR_ERR_NO_ANSWER: return AE_JH_NO_ANSWER;
	case AE_STR_ERR_REFUSED: return AE_JH_TURNED_AWAY;
	case AE_STR_ERR_HOST_CLOSED: return AE_JH_HOST_CLOSED;
	case AE_STR_ERR_LOBBY_FULL: return AE_JH_FULL;
	case AE_STR_ERR_NO_GAME: return AE_JH_NOT_FOUND;
	default: return AE_JH_GENERIC;
	}
}

static void set(char *to, int size, char const *text)
{
	snprintf(to, (size_t)size, "%s", text ? text : "");
}

void ae_join_helper_workshop_item(char const *map, char *item, int size)
{
	char const *at = map ? strstr(map, "@ws") : NULL;
	int length = 0;

	if (size <= 0)
		return;
	item[0] = 0;
	if (!at)
		return;
	at += 3;
	while (at[length] >= '0' && at[length] <= '9' && length + 1 < size)
	{
		item[length] = at[length];
		length++;
	}
	item[length] = 0;
}

void ae_join_helper_workshop_link(char const *item, char *link, int size)
{
	if (size <= 0)
		return;
	link[0] = 0;
	if (item && item[0])
		snprintf(link, (size_t)size, "https://steamcommunity.com/sharedfiles/filedetails/?id=%s", item);
}

void ae_join_helper_steam_link(char const *item, char *link, int size)
{
	char page[120];

	if (size <= 0)
		return;
	link[0] = 0;
	ae_join_helper_workshop_link(item, page, (int)sizeof(page));
	if (page[0])
		snprintf(link, (size_t)size, "steam://openurl/%s", page);
}

void ae_join_helper_words(int cause, char const *detail, char const *map, struct ae_join_helper_words *words)
{
	memset(words, 0, sizeof(*words));
	words->action_kind = AE_JH_ACTION_OK;
	set(words->action, (int)sizeof(words->action), "OK");
	switch (cause)
	{
	case AE_JH_UPDATE_PACKS:
		set(words->title, (int)sizeof(words->title), "UPDATE TO JOIN");
		set(words->reason, (int)sizeof(words->reason), "This game uses weapon packs this copy does not have. Update "
			"Arena Evolved to get them, then join again.");
		set(words->note, (int)sizeof(words->note), "WEAPON PACKS must be on in SETTINGS > SYSTEM > FILES.");
		words->action_kind = AE_JH_ACTION_UPDATE;
		set(words->action, (int)sizeof(words->action), "UPDATE");
		break;
	case AE_JH_UPDATE_VERSION:
		set(words->title, (int)sizeof(words->title), "UPDATE TO JOIN");
		set(words->reason, (int)sizeof(words->reason), "The host is on a newer version of Arena Evolved than this PC. "
			"Update, then join again.");
		set(words->note, (int)sizeof(words->note), "Your saves and settings stay as they are.");
		words->action_kind = AE_JH_ACTION_UPDATE;
		set(words->action, (int)sizeof(words->action), "UPDATE");
		break;
	case AE_JH_UPDATE_MODE:
		set(words->title, (int)sizeof(words->title), "UPDATE TO JOIN");
		set(words->reason, (int)sizeof(words->reason), "This game plays a mode this version of Arena Evolved does not "
			"have. Update, then join again.");
		set(words->note, (int)sizeof(words->note), "Your saves and settings stay as they are.");
		words->action_kind = AE_JH_ACTION_UPDATE;
		set(words->action, (int)sizeof(words->action), "UPDATE");
		break;
	case AE_JH_HOST_OLDER:
		set(words->title, (int)sizeof(words->title), "THE HOST IS BEHIND");
		set(words->reason, (int)sizeof(words->reason), detail && detail[0] ? detail : "The host is on an older "
			"version than this PC. Ask them to update, then join again.");
		break;
	case AE_JH_WORKSHOP:
	{
		char item[24];

		ae_join_helper_workshop_item(map, item, (int)sizeof(item));
		set(words->title, (int)sizeof(words->title), "GET THE MAP FIRST");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "JOIN AGAIN");
		if (!item[0])
		{
			set(words->reason, (int)sizeof(words->reason), "This game plays a map from the Steam Workshop. Subscribe to "
				"it in Steam: Steam downloads it, then you can join.");
			set(words->note, (int)sizeof(words->note), "Subscribed already? Steam may still be downloading it.");
			break;
		}
		/* (0.7.9: the page opened for the player, Subscribe one click there; JOIN AGAIN once Steam has it) */
		set(words->reason, (int)sizeof(words->reason), "This game plays a map from the Steam Workshop. OPEN IN STEAM "
			"shows it in Steam: click Subscribe, and Steam downloads it.");
		set(words->note, (int)sizeof(words->note), "When Steam's download is done, JOIN AGAIN.");
		words->action_kind = AE_JH_ACTION_OPEN_STEAM;
		set(words->action, (int)sizeof(words->action), "OPEN IN STEAM");
		words->second_kind = AE_JH_ACTION_AGAIN;
		set(words->second, (int)sizeof(words->second), "JOIN AGAIN");
		break;
	}
	case AE_JH_MAP:
		set(words->title, (int)sizeof(words->title), "YOU NEED THE MAP");
		set(words->reason, (int)sizeof(words->reason), detail && detail[0] ? detail : "This PC does not have the map "
			"this game plays.");
		break;
	case AE_JH_NEEDS_MCC:
		set(words->title, (int)sizeof(words->title), "YOU NEED MCC");
		set(words->reason, (int)sizeof(words->reason), "This game needs MCC files. Install Halo: The Master Chief "
			"Collection (Steam) on this PC, or join another game.");
		/* (0.7.9: its store page opened for the player) */
		words->action_kind = AE_JH_ACTION_GET_MCC;
		set(words->action, (int)sizeof(words->action), "GET MCC");
		words->second_kind = AE_JH_ACTION_OK;
		set(words->second, (int)sizeof(words->second), "NOT NOW");
		break;
	case AE_JH_MCC_OFF:
		set(words->title, (int)sizeof(words->title), "MCC FILES ARE OFF");
		set(words->reason, (int)sizeof(words->reason), "This game plays MCC maps. MCC is in Steam on this PC, but "
			"Arena Evolved isn't using its files.");
		set(words->note, (int)sizeof(words->note), "SETTINGS > SYSTEM > FILES > MCC FILES turns them off again.");
		words->action_kind = AE_JH_ACTION_MCC_ON;
		set(words->action, (int)sizeof(words->action), "USE MCC FILES");
		break;
	case AE_JH_AE_ONLY:
		set(words->title, (int)sizeof(words->title), "FOR ARENA EVOLVED PLAYERS");
		set(words->reason, (int)sizeof(words->reason), "This server takes Arena Evolved players only. Join it with "
			"the newest Arena Evolved.");
		break;
	case AE_JH_NO_ID:
		set(words->title, (int)sizeof(words->title), "THE SERVER CAN'T TELL THIS PC APART");
		set(words->reason, (int)sizeof(words->reason), "The game sent this server no machine id, and it needs one to "
			"tell its players apart. Update Arena Evolved, or join another game.");
		words->action_kind = AE_JH_ACTION_UPDATE;
		set(words->action, (int)sizeof(words->action), "UPDATE");
		break;
	case AE_JH_VPN_BLOCKED:
		set(words->title, (int)sizeof(words->title), "NO VPNS ON THIS SERVER");
		set(words->reason, (int)sizeof(words->reason), "This server does not let VPN connections in. Turn the VPN "
			"off, then join again.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_DISCORD_NEEDED:
		set(words->title, (int)sizeof(words->title), "A DISCORD ACCOUNT IS NEEDED");
		set(words->reason, (int)sizeof(words->reason), "This server wants a linked Discord account from connections "
			"like this one, and this PC showed none that checks out. Link one, then join again.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_DISCORD_TOO_NEW:
		set(words->title, (int)sizeof(words->title), "DISCORD ACCOUNT TOO NEW");
		set(words->reason, (int)sizeof(words->reason), "This server only takes Discord accounts older than the one "
			"this PC showed. Join with an older account.");
		break;
	case AE_JH_DISCORD_IN_USE:
		set(words->title, (int)sizeof(words->title), "DISCORD ACCOUNT ALREADY PLAYING");
		set(words->reason, (int)sizeof(words->reason), "That Discord account is in this game on another PC. Leave "
			"there first, or join with another account.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_HELD:
	{
		int minutes = detail ? atoi(detail) : 0;

		set(words->title, (int)sizeof(words->title), "HELD OUT FOR NOW");
		if (minutes > 0)
			snprintf(words->reason, sizeof(words->reason), "The server removed this PC a moment ago and keeps it out "
				"for up to %d more minute%s. Join again after that.", minutes, minutes == 1 ? "" : "s");
		else
			set(words->reason, (int)sizeof(words->reason), "The server removed this PC a moment ago and keeps it out "
				"for a while. Join again after that.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	}
	case AE_JH_NO_PLAYER:
		set(words->title, (int)sizeof(words->title), "THE SERVER TIMED THIS PC OUT");
		set(words->reason, (int)sizeof(words->reason), "No player of this PC joined the game within 15 seconds. "
			"Join again.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_ADMIN_SLOTS:
		set(words->title, (int)sizeof(words->title), "THE LAST PLACES ARE FOR ADMINS");
		set(words->reason, (int)sizeof(words->reason), "The server keeps its last places for its admins. Try again "
			"when someone leaves.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_NO_ANSWER:
		set(words->title, (int)sizeof(words->title), "THE HOST DIDN'T ANSWER");
		set(words->reason, (int)sizeof(words->reason), "Nothing came back from the host. It may be offline, or "
			"something between you and it is in the way.");
		set(words->note, (int)sizeof(words->note), "Still nothing? Ask the host for a new invite.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "RETRY");
		break;
	case AE_JH_FULL:
		set(words->title, (int)sizeof(words->title), "THE GAME IS FULL");
		set(words->reason, (int)sizeof(words->reason), "It takes no more players right now. Try again in a moment: "
			"someone may leave.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_CLOSED:
		set(words->title, (int)sizeof(words->title), "THE GAME IS CLOSED");
		set(words->reason, (int)sizeof(words->reason), "It is not taking players. The host may have closed it, or "
			"the game may be under way.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_PASSWORD:
		set(words->title, (int)sizeof(words->title), "PASSWORD NEEDED");
		set(words->reason, (int)sizeof(words->reason), "This game has a password, and the one this PC gave was not "
			"right. Ask the host for it.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_TURNED_AWAY:
		set(words->title, (int)sizeof(words->title), "THE HOST TURNED THIS PC AWAY");
		set(words->reason, (int)sizeof(words->reason), "The host's game did not let this PC in. It can be a ban, a "
			"name the host does not allow, or a limit on players from one address.");
		break;
	case AE_JH_HOST_CLOSED:
		set(words->title, (int)sizeof(words->title), "THE HOST CLOSED THE GAME");
		set(words->reason, (int)sizeof(words->reason), "The game was closed while this PC was joining it.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	case AE_JH_NOT_FOUND:
		set(words->title, (int)sizeof(words->title), "GAME NOT FOUND");
		set(words->reason, (int)sizeof(words->reason), "The game is not there any more, or it no longer takes this "
			"invite.");
		set(words->note, (int)sizeof(words->note), "Ask the host for a new invite.");
		words->action_kind = AE_JH_ACTION_AGAIN;
		set(words->action, (int)sizeof(words->action), "TRY AGAIN");
		break;
	default:
		set(words->title, (int)sizeof(words->title), "CAN'T JOIN");
		set(words->reason, (int)sizeof(words->reason), detail && detail[0] ? detail : "The join did not work.");
		break;
	}
}
