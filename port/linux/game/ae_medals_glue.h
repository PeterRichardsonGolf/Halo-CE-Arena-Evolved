/*
AE_MEDALS_GLUE.H

MEDALS, the game's side of the rules (ae_medals.c): what the AE hook lines in damage.c, game_engine.c,
game_engine_race.c and network_distributed.c call. The rules are ae_medal_rules.h's; the numbers of the medals and
what the display takes are ae_medals.h's. Include after cseries.h.

Only the host detects, and a local game or split screen with no network. A medal for a player on another machine goes
to it on message 102, reliably: the player's byte and the medal's number. A build that does not know the message drops
it. Bots earn none on screen, but their kills keep the counts true.
*/

#ifndef __AE_MEDALS_GLUE_H
#define __AE_MEDALS_GLUE_H

/* message 102 (network_distributed.h): the medal a player earned, from the host to that player's machine */
struct distributed_medal
{
	byte player_index;
	byte medal;
};

/* damage.c, just before a blow is dealt to an object: the shield and the body of a player's unit as they stand, and who
hurts it, to tell by the next call what the blow took */
void ae_medals_damage_before(long object_index, long owner_player_index);

/* damage.c, with each blow that lands on an object, before its aftermath counts a kill: what the blow was, kept for
the kill it may make this tick; a blow on a player's unit goes in that life's damage log */
void ae_medals_damage(long object_index, long definition_index, short category, short side_effect, boolean headshot,
	unsigned long being_damaged_flags);

/* game_engine_player_killed: a player died (the game's own credit: killer NONE for none, friendly_fire for a
teammate's) */
void ae_medals_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire);
/* the weapon definition of the killer's carried weapon whose damage this is, NONE for none */
long ae_medals_killing_weapon_definition(long killing_player_index, long damage_index);

/* (the kill feed) whether the kill of this player that the medals' rules have just seen was found perfect: the host's knowledge alone,
read after ae_medals_player_killed in the same tick */
boolean ae_medals_kill_was_perfect(long dead_player_index);

/* weapons.c, with each round a weapon fires: the player's shot ring */
void ae_medals_shot(long weapon_index, short trigger_index, long owner_object_index);

/* units.c, a weapon taken into a unit's hands; game_engine.c, a weapon the map's spawner made: the link that tells a
pickup from a spawner */
void ae_medals_weapon_taken(long unit_index, long weapon_index);
void ae_medals_item_spawned(long object_index);

/* game_engine_ctf.c, a flag about to be put back at its stand (taken back, captured or timed out): where it lies now */
void ae_medals_flag_reset(long weapon_index);

/* game_engine_update, each tick: the flags, the ball and the hill, which the game keeps in its statistics */
void ae_medals_update(void);

/* game_engine_race.c: a player touched a checkpoint */
void ae_medals_race_checkpoint(long player_index);

/* game_engine_end_game: the game ended */
void ae_medals_game_end(void);

/* game_engine_initialize_for_new_map: nothing counted, nothing showing */
void ae_medals_new_map(void);

/* network_distributed.c: message 102 */
unsigned short ae_medals_entry_size(void);
void ae_medals_handle_down(void const *entries, int from_stream);

#endif /* __AE_MEDALS_GLUE_H */
