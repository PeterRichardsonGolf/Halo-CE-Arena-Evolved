/* bots.h: computer players for local multiplayer games (port/linux/game/bots.c)

A bot is a player of a local (split screen) game whose controls come from
code instead of a controller: it is in the lobby, the scores and the game's
rules as any player is, spawns, respawns and scores. Each bot is the one
player of a machine of its own at the top of the machine indices
(BOTS_FIRST_MACHINE up), which no real machine takes: a split screen game has
only its own machine. The bots have no view and no controller: nothing is
drawn for them. */
#ifndef HALO_BOTS_H
#define HALO_BOTS_H

struct player_action;
struct network_player;

/* the most bots in one game: the Xbox's 15 (with one player, its 16) where the session has 16 machines; a desktop
build's 128 machines take 120 (owner 2026-10-26: OFFLINE games with as many bots as LAN's, and a soak of the most) */
#if HALO_PORT_MAXIMUM_NETWORK_MACHINES >= 128
#define BOTS_MAXIMUM 120
#else
#define BOTS_MAXIMUM 15
#endif
/* the first bot's machine index (the last machine indices of the session) */
#define BOTS_FIRST_MACHINE (HALO_PORT_MAXIMUM_NETWORK_MACHINES - BOTS_MAXIMUM)
#define BOTS_BRAINS 127 /* AE hook */

enum
{
	/* bots.teams: in a team game, the bots on both teams (evening them
	with the players), or all on the team the players are not on */
	_bots_teams_even = 0,
	_bots_teams_against,
};

/* the settings (bots.count, bots.skill, bots.teams) */
long bots_wanted_count(void);
short bots_skill(void);
/* a skill's level by its name (recruit up to demon; the normal level for any other) */
short bots_skill_named(char const *name);
short bots_teams(void);

/* a machine is a bot's */
int bots_machine_is_bot(long machine_index);
/* the network player for bot number `bot_index` (0 up): its machine,
controller, name and no team or colour yet */
void bots_network_player(long bot_index, struct network_player *player);

/* the game tick (players_update_before_game): every bot's controls this
tick, into the actions of the players by their index; nothing outside a
local game */
void bots_update_actions(struct player_action *actions);

/* a campaign AI squad member's controls this tick (ae_companion.c; ae_bots_squad.inc): its slot (the bot machine's number), its
player's datum index and the action it fills */
void bots_squad_drive(long slot, long player_index, struct player_action *action);
/* the squad brain's counts, one line of text */
void bots_squad_report_lines(float closest_pair);
/* the campaign squad's warp, last: whether a member's walk failed lately (its ticks since progress), a warp counted by reason
(AE_COMPANION_WARP_*, then 5 catch-up, 6 a co-op rescue) */
boolean bots_squad_member_stalled(long slot, long now, long *ticks);
void bots_squad_count_warp(int reason);
/* a member's controls' cost, milliseconds, in a game tick: the squad's busiest tick is its cost line's ms */
void bots_squad_note_cost(double milliseconds, long tick);
void bots_squad_cost_reset(void);
/* a squad member's brain starts again (its fight, path and goal forgotten), as for a new body */
void bots_squad_reset(long slot);
/* the squad brain's view for the companions' last resorts: the player under a roof (TRACK FOLLOW's test), a member cut off from
the player by a barrier, a member fighting */
int bots_squad_indoors(void);
int bots_squad_member_cut_off(long slot);
int bots_squad_member_fighting(long slot);

/* a map's start and end (players.c) */
void bots_initialize_for_new_map(void);
void bots_dispose_from_old_map(void);

#endif
