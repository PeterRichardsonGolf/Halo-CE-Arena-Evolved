/*
RANDOMIZER.H

The campaign randomizer (randomizer.c). A level swaps actors and weapons
in memory; tags it does not have are copied in from the other campaign
maps' (tag_import.h), of the randomizer's tag pack. Played alone or in
split screen on one machine: a network game is never randomized.
*/

#ifndef __RANDOMIZER_H
#define __RANDOMIZER_H

/* the randomizer's tag pack (port/assets/tag_packs/ae_randomizer.toml):
the ten campaign maps' tags, their actors' and weapons' pixels and samples */
#define RANDOMIZER_PACK "ae_randomizer"
/* the pictures of the scenery Firefight places (the randomizer's pack leaves scenery out) */
#define FIREFIGHT_PACK "ae_firefight"

/* a level loading (every map's tags, the menus' too), and the level going */
void randomizer_tags_loaded(char const *scenario_name);
void randomizer_tags_unloaded(void);

/* whether this level has tags the randomizer copied in (the read hook) */
int randomizer_imported(void);

/* a number written into a saved game with the level's checkpoint checksum:
0 when this level has no copied tags, else a hash of its tag table. A saved game
of a level with another table is not taken (game_state.c) */
unsigned long randomizer_save_signature(void);

/* the actor, weapon or carried weapon a placement uses. each returns the
index it was given when the randomizer is not swapping that */
long randomizer_actor_variant(long original, long encounter_index, long squad_index, void *starting_location);
/* the last randomizer_actor_variant left its infection form out: NONE, as
one of a squad's to be swapped past the setup's share */
int randomizer_actor_left_out(void);
long randomizer_weapon_definition(long definition_index, void *scenario_object);
/* gives an actor's unit a weapon of the weapons pool in place of the actor's
own: 1 when it did, 0 when the actor's own weapon is to be given */
int randomizer_actor_weapon(long weapon_definition, long unit_index);

/* PLAYER LOADOUT (game.randomizer_loadout), for player_add_equipment: whether
it stands in for the starting profile's weapons, a level with some to give.
Gives them to a player's unit whose weapons were reset or that has none */
int randomizer_player_loadout(long unit_index, int reset_equipment);

/* the roll the level was laid out with */
unsigned long randomizer_roll(void);

/* the DEV MODE command: a line of text on the state */
void randomizer_status(char *text, unsigned int size);
/* the next level's roll: 0 for a new one each time. reroll forgets the last */
void randomizer_reroll(void);

/* the tag pack's writing, for a screen to show: queued or being written, the campaign maps read so far and the ones
this PC has, done (the pack is there and current), failed (not written), and one line why a map was left out or the
pack was not written */
struct randomizer_pack_state
{
	int writing;
	int maps_done;
	int maps_wanted;
	int done;
	int failed;
	char reason[160];
};
void randomizer_pack_state(struct randomizer_pack_state *state);
/* queues the pack's writing now, on the writer's own worker (nothing when the pack is current) */
void randomizer_pack_write(void);
/* whether a test's game (a test launch, debug.network_test) is to wait to start: game.firefight_pack is on and the pack is
queued or being written, so the game's level would load without the campaign's tags. 0 once the pack is there or failed */
int randomizer_pack_wait(void);
/* each frame: with game.randomizer on, a pack that is missing or older than its maps is queued once, and a writing
finished is taken up. The level loading meanwhile plays as the map has it */
void randomizer_frame(void);

/* what the skulls (skulls.c) and Firefight (firefight_spawner.c) ask: whether a tag's name is a class of the randomizer's */
int randomizer_weapon_is_covenant(char const *tag_name);
int randomizer_unit_is_grunt(char const *tag_name);
int randomizer_unit_is_hunter(char const *tag_name);
int randomizer_unit_can_carry(char const *tag_name);
int randomizer_show_prefix(void);
int randomizer_stock_tag_exists(long group_tag, char const *name);
long randomizer_firefight_tags(unsigned long group, long *tags, long maximum);
short randomizer_firefight_actor_tier(long variant_tag);
boolean randomizer_firefight_weapon_power(long weapon_tag);
/* how many of the level's enemy squads' actors can carry a skull */
long randomizer_level_carriers(void);

#endif /* __RANDOMIZER_H */
