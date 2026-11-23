/*
AE_PGCR_STATS.H

The post-game report's numbers, counted on the host (and in a game with no network) as the game goes, from the events
ae_medals.c already sees: every round fired and every hit (written into the player's synced statistics, shots_fired and
shots_hit), every kill (headshots, the pair table, weapons, sprees, multikills) and every medal. At the game's end the host
turns each player's counts into a summary (ae_pgcr_rules.h), keeps it (ae_pgcr_data.h) and, in a network game, sends each to
every client (message 107), so every machine shows the same report. The rules are ae_pgcr_rules.c's.
*/

#ifndef __AE_PGCR_STATS_H
#define __AE_PGCR_STATS_H

/* a new map loads: the last game's counts and report go */
void ae_pgcr_stats_new_map(void);
/* the slot fired a round (a trigger pull, a shell, one round of a burst) */
void ae_pgcr_stats_shot(int slot);
/* damage reached the victim's unit from the attacker (slots, -1 for none) with this damage category */
void ae_pgcr_stats_damage(int attacker, int victim, int category);
/* (rc2) the damage reached the victim took this share of their whole vitality (shield and body, 1.0 the lot) from the
attacker (-1 for none); friendly: the attacker is the victim's teammate */
void ae_pgcr_stats_damage_share(int attacker, int victim, float share, int friendly);
/* (rc2) the kill's kinds (melee, power weapon, grenade, assassination, splatter, stick, kills as the flag carrier), called
as the kill record is complete, for every kill; only an enemy's is counted */
struct ae_medal_kill;
void ae_pgcr_stats_kill_kinds(struct ae_medal_kill const *kill);
/* (rc2) a tick the slot carried the flag (each tick of a CTF game) */
void ae_pgcr_stats_carrier_tick(int slot);
/* (rc2) the host's bomb events: a plant or a defuse by this player (a game's player index; AE_PGCR_BOMB_*) */
void ae_pgcr_stats_bomb(long player_index, int what);
/* a medal the host gave */
void ae_pgcr_stats_medal(int slot, int medal);
/* a player died (any death); the kill, when it counts for a killer, is the next call */
void ae_pgcr_stats_death(int victim);
void ae_pgcr_stats_kill(int killer, int victim, int headshot, long damage_tag);
/* the game ended: the summaries made, kept and sent */
void ae_pgcr_stats_game_end(void);
/* (0.9.4) the killer's kills on the victim (slots) in the game being played or just ended (the host's counts: 0 on a
   client, and for a slot out of range) */
int ae_pgcr_stats_kills(int killer, int victim);

/* message 107 (network_distributed.c): taken by a client from the host alone */
int ae_pgcr_stats_handles_message(unsigned short const *message, unsigned short size);
void ae_pgcr_stats_handle_message(long machine_index, unsigned short const *message, unsigned short size);

#endif
