/*
AE_BOTS.H

Arena Evolved's side of the bots of local games (OpenCE PR #367,
port/linux/game/bots.c): what the AE hook lines in bots.c,
network_server_manager.c and game_events.c call (port/linux/game/ae_hooks.txt
lists them). Included after cseries.h.

- The pack's battle rifle in a bot's hands (ae_burst_rifle.c): bots.c knows
  the stock weapons by their tags' names; the rifle is a weapon worth having
  (kept, not swapped for an assault rifle lying near), at a rifle's distance,
  pulled once a burst as a player pulls it (ae_burst_rifle fires three
  rounds a new pull; a held trigger fires one burst).
- The lobby: bots play Slayer, CTF, Oddball (with reverse tag and
  juggernaut, AE JUGGER's too), King of the Hill and Race
  (ae_bots_objective.c); a local game of a gametype they do not play has
  none, and halo.log says so, once for each gametype the lobby is on.
- Delta Stats (network.report_events): a game with bots is not recorded nor
  sent; its scores are not people's.
*/

#ifndef __AE_BOTS_H
#define __AE_BOTS_H

/* the objective modes' play, and which gametypes bots play (ae_bots_objective.c) */
#include "ae_bots_objective.h"
/* the bots' vehicles: seats taken, ridden, left; the driving spike (ae_bots_vehicle.c) */
#include "ae_bots_vehicle.h"

/* (bots.c's bots_weapon_kind, once a map for each weapon definition) what bots.c made of the weapon by its tag's
name, changed for a weapon AE knows better: its value to a bot, the ticks between its trigger's pulls (0: held in
bursts) and the distance a bot keeps with it */
void ae_bots_weapon_kind(long definition_index, short *value, short *tap_ticks, real *range);

/* (bots.c's bots_weapon_empty) whether a weapon is spent, for an energy weapon (no rounds: its battery out): 1 or 0;
-1 for a weapon of rounds, which bots.c tests itself (ae_bots_combat_rules.h's ae_bots_combat_weapon_spent) */
int ae_bots_weapon_spent(long weapon_index);

/* (network_server_manager.c's network_game_server_idle, after the bots' lobby) a local game's lobby whose
gametype has no bots while bots.count asks for some: one halo.log line for each gametype it is on */
void ae_bots_lobby_check(boolean local_pregame, long game_engine_index, wchar_t const *gametype_name);

/* whether a player of the game now is a bot */
boolean ae_bots_in_game(void);
/* (game_events.c, before Delta Stats begins recording a game) TRUE for a game with bots, which is not recorded:
one halo.log line the first time */
boolean ae_bots_kept_from_delta(void);
/* (bots.c, each bot's tick) PLAYFIGHT (bots.playfight): the bots move and aim but never shoot, throw or strike */
boolean ae_bots_playfight(void);
/* (bots.c, each bot's tick) MOVEMENT SKILL STANDING (bots.movement_skill "standing", for tests): the bots stay where
they spawn, never moving, jumping or crouching; they still turn, aim and shoot (BOT SKILL's level for that) */
boolean ae_bots_standing(void);

/* (bots.c's bots_bot_update, via ae_bots_events.inc: debug.bot_events) the bots' fight log, read by tools/ae_test/bot_report.py */
struct bots_bot;
struct bots_tick;
void ae_bots_events_begin(struct bots_bot *bot, long bot_index, struct bots_tick *tick);
void ae_bots_events_end(struct bots_bot *bot, long bot_index, struct bots_tick *tick);
/* (damage.c's object_cause_damage) the damage a player caused to a unit: a bot's hit is measured and logged; and a
bot's unit's last damage (its effect's definition and damage flags), the cause of its death (ae_bots_falls_rules.h) */
void ae_bots_damage_note(long owner_player_index, long object_index, long definition_index, unsigned long flags);
/* (2026-10-24, ae_bots_revenge.inc) game_engine_player_killed: REVENGE's count of who killed a bot, and its kills */
void ae_bots_player_killed(long killing_player_index, long dead_player_index);

/* how the bots get around a map: teleporters, the layout file's bot lines, health packs (ae_bots_travel.c) */
#include "ae_bots_travel.h"
/* bots.count changed in a local game in progress: bots join or leave it at once (ae_bots_live.c) */
#include "ae_bots_live.h"

#endif
