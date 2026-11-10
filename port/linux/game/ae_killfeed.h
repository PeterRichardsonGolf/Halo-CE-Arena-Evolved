/*
AE_KILLFEED.H

THE KILL FEED: everyone's kills as rows in the player's view (rules: ae_killfeed_rules.h; the game's side: ae_killfeed.c). Each
machine records the kills it sees, from the kill the game already tells every machine (the killer, the victim, friendly fire) and the
killing blow's damage (its weapon and a head shot), and keeps a list of rows for each local player who has KILL FEED on
(display.kill_feed, each player's, on by default). The rows replace the game's own kill lines for that player; other messages stay.

The host also knows a perfect kill (the medals' rules) and who helped; a guest knows only what the game tells it (no network message
is added), so a guest's rows have the weapon and the head shot, and an assist only where its copy of the victim's attackers has it.

A lone player's view has a panel; with more than one local player each gets a line in their message list instead, with the weapon as
a word. Nothing is sent. Include after cseries.h.
*/

#ifndef __AE_KILLFEED_H
#define __AE_KILLFEED_H

/* damage.c, with each blow that lands on an object, after the medals': a blow that depletes a player's unit is kept for the kill it
makes this tick (victim_player_index: the unit's player, NONE for none) */
void ae_killfeed_damage(long victim_player_index, long object_index, long definition_index, boolean headshot,
	unsigned long being_damaged_flags);
/* game_engine_player_killed (after the medals': the kill's perfect mark is theirs), every machine: the kill's row joins the lists */
void ae_killfeed_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire);
/* game_engine.c's multiplayer_message: whether this game message is a kill line the feed replaces for that local player */
boolean ae_killfeed_replaces_message(short local_player_index, long message);
/* hud.c's pickup text (ammo, grenade, weapon, powerup): whether this local player's KILL FEED hides it; the hold-to-pick-up prompt stays */
boolean ae_killfeed_hides_pickup_text(short local_player_index);
/* hud.c's turn, after the medals: the panel of a lone player's view */
void ae_killfeed_draw(short local_player_index);
/* a player's name colour in this local player's view, as the feed has it (the colour of names over heads; white where CLASSIC with
ENEMY NAME COLOR CLASSIC would make it the HUD's blue). person_player_index NONE: none, an enemy's */
union real_argb_color;
void ae_killfeed_name_color(short local_player_index, long viewer_player_index, long person_player_index, short person_team,
	boolean betrayal, union real_argb_color *color);
/* ELIMINATION: a revive (every machine, from the round state): "<reviver> REVIVED <player>" in the feed, by absolute player slots */
void ae_killfeed_revive(long reviver_slot, long revived_slot);
/* a new map: nothing showing */
void ae_killfeed_new_map(void);
/* one local player's view's rows gone (a split-screen player who quit: ae_split_quit.c) */
void ae_killfeed_reset_local(short local_player_index);

#endif
