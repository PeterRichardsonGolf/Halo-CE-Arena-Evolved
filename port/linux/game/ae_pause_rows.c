/*
AE_PAUSE_ROWS.C

The AE pause menu's rows (ae_pause_rows.h). The help lines are the approved mockups' (mockups-040/pause/work/pause.py),
said the way each row's engine call works (team_change.c's CHANGE TEAMS, menu_functions.c's END GAME).
*/

#include <stddef.h>

#include "ae_pause_rows.h"

static int team_game(struct ae_pause_facts const *facts)
{
	return facts->game == AE_PAUSE_MULTIPLAYER && facts->teams;
}

short ae_pause_rows(struct ae_pause_facts const *facts, short *rows, short maximum)
{
	short list[AE_PAUSE_ROWS_MAXIMUM], count = 0, index;

	list[count++] = AE_PAUSE_RESUME;
	list[count++] = AE_PAUSE_SETTINGS;
	switch (facts->game)
	{
	case AE_PAUSE_MULTIPLAYER:
		if (team_game(facts))
			list[count++] = AE_PAUSE_TEAMS;
		list[count++] = AE_PAUSE_SCOREBOARD;
		if (facts->host)
		{
			list[count++] = AE_PAUSE_HOST_HEADER;
			if (facts->lobby)
			{
				list[count++] = AE_PAUSE_OPTIONS;
				list[count++] = AE_PAUSE_MAP;
				/* (0.7.0, the owner: the map and the options picked here want a restart to be any use) */
				list[count++] = AE_PAUSE_RESTART_GAME;
			}
			if (facts->rounds)
				list[count++] = AE_PAUSE_RESTART_ROUND;
			list[count++] = AE_PAUSE_BOTS;
			list[count++] = AE_PAUSE_END;
		}
		break;
	case AE_PAUSE_CAMPAIGN:
		list[count++] = AE_PAUSE_REVERT;
		break;
	case AE_PAUSE_COOP:
		list[count++] = AE_PAUSE_FRIENDLY_FIRE;
		break;
	default:
		break;
	}
	list[count++] = AE_PAUSE_QUIT;
	for (index = 0; index < count && index < maximum; index++)
		rows[index] = list[index];
	return index;
}

const char *ae_pause_row_label(int row, struct ae_pause_facts const *facts)
{
	switch (row)
	{
	case AE_PAUSE_RESUME: return "RESUME";
	case AE_PAUSE_SETTINGS: return "SETTINGS";
	case AE_PAUSE_TEAMS: return "CHANGE TEAMS";
	case AE_PAUSE_SCOREBOARD: return "SCOREBOARD";
	case AE_PAUSE_HOST_HEADER: return "GAME (HOST)";
	case AE_PAUSE_OPTIONS: return "GAME OPTIONS";
	case AE_PAUSE_MAP: return "CHANGE MAP";
	case AE_PAUSE_BOTS: return "BOTS";
	case AE_PAUSE_END: return "END GAME";
	case AE_PAUSE_RESTART_GAME: return "RESTART GAME";
	case AE_PAUSE_RESTART_ROUND: return "RESTART ROUND";
	case AE_PAUSE_FRIENDLY_FIRE: return "FRIENDLY FIRE";
	case AE_PAUSE_REVERT: return "REVERT TO SAVED";
	case AE_PAUSE_QUIT: return facts && facts->game == AE_PAUSE_CAMPAIGN ? "SAVE AND QUIT" : "QUIT";
	}
	return "";
}

const char *ae_pause_row_help(int row, struct ae_pause_facts const *facts)
{
	switch (row)
	{
	case AE_PAUSE_RESUME: return "Back to the game.";
	case AE_PAUSE_SETTINGS: return "Your controls, view, HUD and audio. Changes apply live.";
	case AE_PAUSE_TEAMS: return "Switch to the other team: you respawn on it (round games: at your next death).";
	case AE_PAUSE_SCOREBOARD: return "The full scoreboard: kills, deaths, assists, objective.";
	case AE_PAUSE_OPTIONS: return "The host changes the gametype's rules; they apply next game.";
	case AE_PAUSE_MAP: return "Pick the next map: this game ends, and the lobby comes back on it.";
	case AE_PAUSE_BOTS: return "How many bots play and how well: at once, in this game too.";
	case AE_PAUSE_END: return "End this game for everyone and go to the post-game screen.";
	case AE_PAUSE_RESTART_GAME: return "Start this game again now, for everyone, with the options and map picked here.";
	case AE_PAUSE_RESTART_ROUND: return "Play this round again, for everyone: no one scores it.";
	case AE_PAUSE_FRIENDLY_FIRE: return "Whether your shots hurt the other players, on this PC, from now on.";
	case AE_PAUSE_REVERT: return "Back to the last checkpoint.";
	case AE_PAUSE_QUIT:
		if (facts && facts->game == AE_PAUSE_CAMPAIGN)
			return "Save at the last checkpoint and go back to the main menu.";
		if (facts && facts->game == AE_PAUSE_COOP)
			return "Leave the co-op game: every player on this PC leaves.";
		return facts && facts->local_players > 1 ? "Leave this game. In split screen: this player only." :
			"Leave this game.";
	}
	return "";
}

int ae_pause_row_look(int row)
{
	switch (row)
	{
	case AE_PAUSE_HOST_HEADER: return AE_PAUSE_LOOK_HEADER;
	case AE_PAUSE_SETTINGS: case AE_PAUSE_TEAMS: case AE_PAUSE_SCOREBOARD: case AE_PAUSE_OPTIONS: case AE_PAUSE_MAP:
	case AE_PAUSE_BOTS:
		return AE_PAUSE_LOOK_SUB;
	case AE_PAUSE_FRIENDLY_FIRE: return AE_PAUSE_LOOK_VALUE;
	}
	return AE_PAUSE_LOOK_ACTION;
}

int ae_pause_row_confirm(int row, struct ae_pause_facts const *facts, const char **title, const char **body,
	const char **yes)
{
	switch (row)
	{
	case AE_PAUSE_TEAMS:
		*title = "Change teams?";
		*body = "You die now, not counted, and respawn on the other team (round games: at your next death). The "
			"host's rule may keep the teams as they are.";
		*yes = "CHANGE TEAMS";
		return 1;
	case AE_PAUSE_END:
		*title = "End the game?";
		*body = "The game ends for everyone now, as its time limit would. The post-game screen comes next.";
		*yes = "END GAME";
		return 1;
	case AE_PAUSE_MAP:
		*title = "End the game?";
		*body = "This game ends for everyone now. The lobby comes back on the map you picked.";
		*yes = "END GAME";
		return 1;
	case AE_PAUSE_RESTART_GAME:
		*title = "Restart the game?";
		*body = "This game ends for everyone now and starts again at once, with the options and the map picked "
			"here. The scores start from zero.";
		*yes = "RESTART";
		return 1;
	case AE_PAUSE_RESTART_ROUND:
		*title = "Restart the round?";
		*body = "Everyone goes back to their spawn and this round starts again. No one scores it.";
		*yes = "RESTART ROUND";
		return 1;
	case AE_PAUSE_REVERT:
		*title = "Revert to saved?";
		*body = "You go back to the last checkpoint. What you did since it is lost.";
		*yes = "REVERT";
		return 1;
	case AE_PAUSE_QUIT:
		if (facts && facts->game == AE_PAUSE_CAMPAIGN)
		{
			*title = "Save and quit?";
			*body = "Your game is saved at the last checkpoint, and the main menu comes back.";
			*yes = "SAVE AND QUIT";
		}
		else if (facts && facts->game == AE_PAUSE_ALONE)
		{
			*title = "Quit?";
			*body = "This map played alone is not saved. The main menu comes back.";
			*yes = "QUIT";
		}
		else if (facts && facts->game == AE_PAUSE_COOP)
		{
			*title = "Leave the game?";
			*body = "Every player on this PC leaves the co-op game.";
			*yes = "LEAVE";
		}
		else
		{
			*title = "Leave the game?";
			*body = facts && facts->local_players > 1 ? "This player leaves the game; the others on this PC stay." :
				"You leave this game.";
			*yes = "LEAVE";
		}
		return 1;
	}
	return 0;
}

short ae_pause_row_skip(short const *rows, short count, short focus, short direction)
{
	short step = direction < 0 ? -1 : 1, at;

	if (focus < 0 || focus >= count || ae_pause_row_look(rows[focus]) != AE_PAUSE_LOOK_HEADER)
		return focus;
	for (at = (short)(focus + step); at >= 0 && at < count; at = (short)(at + step))
	{
		if (ae_pause_row_look(rows[at]) != AE_PAUSE_LOOK_HEADER)
			return at;
	}
	for (at = (short)(focus - step); at >= 0 && at < count; at = (short)(at - step))
	{
		if (ae_pause_row_look(rows[at]) != AE_PAUSE_LOOK_HEADER)
			return at;
	}
	return focus;
}
