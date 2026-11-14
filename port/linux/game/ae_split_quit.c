/*
AE_SPLIT_QUIT.C

See ae_split_quit.h. What is keyed by the local player lets go as the player leaves the list:
- the views: main.c builds them each frame from the list (local_player_get_next), and players.c recounts it each tick, so
  the others' views grow with no more done here;
- the pause: ae_glue_pause_still_on asks local_player_exists, so ae_screen_pause_tick closes the seat;
- rumble: rumble_update stops the motor of a local player with no player; its own state is cleared here;
- the control state: player_control_new_unit(NONE), as players.c does when it moves a controller's player;
- the kill feed's and medals' per-view state of the slot is cleared, and the player is not prejoined to the next game
  (player_ui_local_player_left_multiplayer_game, as the pause's QUIT does too);
- the AE per-player settings are read for a view's local player between ae_config_player_enter and its leave, nothing
  stays set; the lone pad of a later controller stays theirs (menu_functions.c pc_menu_split_players);
- the network player and the player datum stay, so the scoreboard and the carnage report list the player as quit.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/player_rumble.h"
#include "memory/data.h"
#include "game/players.h"
#include "interface/player_ui.h"

#include "ae_killfeed.h"
#include "ae_medals.h"
#include "ae_split_quit.h"
#include "ae_split_quit_rules.h"
#include "network_coop.h"

void platform_log(char const *format, ...);
/* (players.c's: the header has no declaration of it) */
void local_player_set_player_index(short local_player_index, long player_index);

void ae_split_quit_player_quit(long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	short local_player_index, other;
	int local_players = 0;

	if (!player)
		return;
	local_player_index = player->local_player_index;
	/* (the live list: two quits in one tick leave the last one on it; players.c's count is the last tick's) */
	for (other = local_player_get_next(NONE); other != NONE; other = local_player_get_next(other))
		local_players++;
	if (!ae_split_quit_leaves(local_player_index != NONE && local_player_get_player_index(local_player_index) == player_index,
		local_players, network_coop_active(), game_connection() == _game_connection_film_playback))
	{
		return;
	}
	rumble_player_clear(local_player_index);
	player_control_new_unit(local_player_index, NONE);
	local_player_set_player_index(local_player_index, NONE);
	/* (not prejoined to the next game, as the pause's QUIT already does; and the slot handed to a player who joins later
	holds none of this view's rows: the kill feed's and medals' own state is per local player) */
	player_ui_local_player_left_multiplayer_game(local_player_index);
	ae_killfeed_reset_local(local_player_index);
	ae_medal_display_reset_local(local_player_index);
	platform_log("split screen: local player %d quit, the other %d keep the screen", (int)local_player_index + 1,
		local_players - 1);
}
