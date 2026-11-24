/*
AE_COOP_RULES.C

See ae_coop_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_coop_rules.h"

void ae_coop_seats_reset(struct ae_coop_seats *seats)
{
	memset(seats, 0, sizeof(*seats));
	seats->seated[0] = 1;
}

short ae_coop_players(struct ae_coop_seats const *seats)
{
	short controller, count = 0;

	for (controller = 0; controller < AE_COOP_CONTROLLERS; controller++)
		count = (short)(count + (seats->seated[controller] != 0));
	return count;
}

short ae_coop_second(struct ae_coop_seats const *seats)
{
	short controller;

	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
		if (seats->seated[controller])
			return controller;
	return -1;
}

unsigned ae_coop_pad_mask(struct ae_coop_seats const *seats)
{
	unsigned mask = 0;
	short controller;

	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
		mask |= seats->seated[controller] ? 1u << controller : 0;
	return mask;
}

short ae_coop_controller_at(struct ae_coop_seats const *seats, short place)
{
	short controller, at = 0;

	for (controller = 0; controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (!seats->seated[controller])
			continue;
		if (at++ == place)
			return controller;
	}
	return -1;
}

int ae_coop_start_joins(struct ae_coop_seats const *seats, short controller)
{
	return controller >= 1 && controller < AE_COOP_CONTROLLERS && !seats->seated[controller] &&
		ae_coop_players(seats) + seats->picking < AE_COOP_PLAYERS_MAX;
}

int ae_coop_join(struct ae_coop_seats *seats, short controller)
{
	if (controller < 1 || controller >= AE_COOP_CONTROLLERS)
		return AE_COOP_JOIN_BAD_CONTROLLER;
	if (seats->seated[controller])
		return AE_COOP_JOIN_SEATED;
	if (ae_coop_players(seats) >= AE_COOP_PLAYERS_MAX)
		return AE_COOP_JOIN_FULL;
	seats->seated[controller] = 1;
	seats->adding = 0;
	return AE_COOP_JOIN_OK;
}

int ae_coop_leave(struct ae_coop_seats *seats, short controller)
{
	if (controller < 1 || controller >= AE_COOP_CONTROLLERS || !seats->seated[controller])
		return 0;
	seats->seated[controller] = 0;
	seats->adding = 0;
	return 1;
}

int ae_coop_back_is_seat(struct ae_coop_seats const *seats, short controller)
{
	return controller >= 1 && controller < AE_COOP_CONTROLLERS && seats->seated[controller];
}

int ae_coop_lone_pad_joins(struct ae_coop_seats const *seats, short controller, int keyboard_used, int pad_start_held)
{
	return controller == 0 && keyboard_used && pad_start_held && ae_coop_second(seats) < 0 && seats->picking == 0;
}

int ae_coop_continue_allowed(struct ae_coop_seats const *seats)
{
	return ae_coop_players(seats) <= 1;
}

const char *ae_coop_players_text(short players)
{
	return players >= 4 ? "4 PLAYERS" : players == 3 ? "3 PLAYERS" : players == 2 ? "2 PLAYERS" : "1 PLAYER";
}

int ae_coop_second_controller_ok(short controller)
{
	return controller >= 1 && controller < AE_COOP_CONTROLLERS;
}

int ae_coop_start_check(struct ae_coop_seats const *seats, int player1_profile, unsigned missing)
{
	short controller;

	if (!player1_profile)
		return AE_COOP_ERR_NO_PROFILE;
	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (!seats->seated[controller])
			continue;
		if (!ae_coop_second_controller_ok(controller))
			return AE_COOP_ERR_SECOND_CONTROLLER;
		if (missing & (1u << controller))
			return AE_COOP_ERR_SECOND_PROFILE;
	}
	return AE_COOP_ERR_NONE;
}

void ae_coop_error_text(int error, const char **title, const char **body)
{
	switch (error)
	{
	case AE_COOP_ERR_NO_PROFILE:
		*title = "The level did not start";
		*body = "No profile for player 1.";
		break;
	case AE_COOP_ERR_SECOND_CONTROLLER:
		*title = "The level did not start";
		*body = "Every other player plays on their own controller, not player 1's.";
		break;
	case AE_COOP_ERR_SECOND_PROFILE:
		*title = "The level did not start";
		*body = "A player has no profile. Press B on their controller and join again.";
		break;
	default:
		*title = "";
		*body = "";
		break;
	}
}

/* ---------- online co-op */

int ae_coop_host_network(int internet_play_on)
{
	return internet_play_on ? AE_COOP_NET_ONLINE : AE_COOP_NET_LAN;
}

short ae_coop_host_maximum(short engine_maximum)
{
	return engine_maximum > 0 && engine_maximum < AE_COOP_ONLINE_PLAYERS_MAX ? engine_maximum : AE_COOP_ONLINE_PLAYERS_MAX;
}

static char const *const difficulty_words[4] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

short ae_coop_level_of_map(const char *map)
{
	static char const *const files[10] = { "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40" };
	char const *name = map ? map : "";
	char const *slash;
	short level;

	if (strchr(name, '@'))
		return -1;
	while ((slash = strpbrk(name, "\\/")) != NULL)
		name = slash + 1;
	for (level = 0; level < 10; level++)
	{
		size_t at;

		for (at = 0; files[level][at]; at++)
		{
			char c = name[at];

			if (c >= 'A' && c <= 'Z')
				c = (char)(c + 32);
			if (c != files[level][at])
				break;
		}
		if (!files[level][at] && !name[at])
			return level;
	}
	return -1;
}

void ae_coop_listing_type(short difficulty, char *text, int size)
{
	if (difficulty >= 0 && difficulty < 4)
		snprintf(text, (size_t)size, "CO-OP \xE2\x80\xA2 %s", difficulty_words[difficulty]);
	else
		snprintf(text, (size_t)size, "CO-OP");
}

static int same_start(const char *text, const char *word)
{
	size_t length = strlen(word), at;

	for (at = 0; at < length; at++)
	{
		char c = text[at];

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if (c != word[at])
			return 0;
	}
	return 1;
}

int ae_coop_gametype_is_coop(const char *gametype)
{
	return gametype && same_start(gametype, "co-op") && (gametype[5] == 0 || gametype[5] == ' ');
}

short ae_coop_difficulty_from_gametype(const char *gametype)
{
	static char const *const words[4] = { "easy", "normal", "heroic", "legend" };
	short difficulty;

	if (!ae_coop_gametype_is_coop(gametype) || gametype[5] != ' ')
		return -1;
	for (difficulty = 0; difficulty < 4; difficulty++)
	{
		if (same_start(gametype + 6, words[difficulty]))
			return difficulty;
	}
	return -1;
}

int ae_coop_listing_kept(int cooperative, int public_game, int lan)
{
	return !cooperative || (public_game && !lan);
}

void ae_coop_host_error_text(int error, const char **title, const char **body)
{
	switch (error)
	{
	case AE_COOP_HOST_ERR_NO_PROFILE:
		*title = "The co-op game did not open";
		*body = "No profile for player 1.";
		break;
	case AE_COOP_HOST_ERR_HOST:
		*title = "The co-op game did not open";
		*body = "This PC could not host a game. Its network port may be in use.";
		break;
	case AE_COOP_HOST_ERR_TIMEOUT:
		*title = "The co-op game did not open";
		*body = "This PC's game did not open its lobby in 10 seconds.";
		break;
	case AE_COOP_HOST_ERR_LEVEL:
		*title = "The co-op game did not open";
		*body = "That level can't be played as co-op in this build.";
		break;
	case AE_COOP_HOST_ERR_PEER:
		*title = "The co-op game did not open";
		*body = "A machine that does not play co-op is in the game. Leave it and host again.";
		break;
	case AE_COOP_HOST_ERR_IN_GAME:
		*title = "The co-op game did not open";
		*body = "Leave the game you are in first.";
		break;
	default:
		*title = "";
		*body = "";
		break;
	}
}

void ae_coop_host_left_text(const char **title, const char **body)
{
	*title = "The host left the game.";
	*body = "You will return to the menus.";
}
