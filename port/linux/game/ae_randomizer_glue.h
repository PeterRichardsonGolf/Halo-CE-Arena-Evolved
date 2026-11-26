/*
AE_RANDOMIZER_GLUE.H

What this build asks of the campaign randomizer (randomizer.c, as Content Evolved has it) that
Content Evolved's own build does not: ae_randomizer_glue.inc (compiled inside randomizer.c, for its
state) and ae_randomizer_glue.c (what the randomizer calls and this build has no copy of). The
randomizer plays in a campaign game or a Firefight game alone, never in a multiplayer game
(ae_randomizer_campaign_game).
*/

#ifndef __AE_RANDOMIZER_GLUE_H
#define __AE_RANDOMIZER_GLUE_H

/* the tag packs Firefight reads its scenery from (ae_firefight.toml holds the storage crate the
game modes' pack holds in Content Evolved's build) */
#define GAME_MODES_PACK "ae_firefight"
#define FIREFIGHT_PACK "ae_firefight"

/* the advertisement bytes of randomizer.c's advertisement functions: nothing here calls them, a
network game is never randomized */
#ifndef HALO_PORT_ADVERTISED_RANDOMIZER_FLAG
#define HALO_PORT_ADVERTISED_RANDOMIZER_FLAG 0x20
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET 3
#define HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET 6
#define HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET 10
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_SIZE 12
#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET 23
#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_SET 0x08
#endif

/* the game modes' tags (Floodfection) are not in this build */
boolean infection_tag_import_wanted(void);
void infection_tag_import(void);

/* the randomizer's tag pack's writing, for a screen to show: queued or being written, the campaign
maps read so far and the ones this PC has, done (the pack is there and current), failed (not
written), and one line why a map was left out or the pack was not written */
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
/* whether a test's game is to wait to start: the test pack is on and its pack is queued or being
written. 0 once the pack is there or failed */
int randomizer_pack_wait(void);
/* each frame: with game.randomizer on, a pack that is missing or older than its maps is queued once,
and a writing finished is taken up */
void randomizer_frame(void);
/* the next level's roll: 0 for a new one each time */
void randomizer_reroll(void);
/* the DEV MODE command: a line of text on the state */
void randomizer_status(char *text, unsigned int size);

/* the level's tag table as the randomizer left it: whether it added tags, and a number of them a saved
game keeps so that one made with other copies is not taken (0 when it added none) */
int randomizer_imported(void);
unsigned long randomizer_save_signature(void);
/* a level loading and going (ae_tag_import_spike.c): the randomizer's, with the tag table noted as
changed when it added tags */
void ae_randomizer_tags_loaded(char const *scenario_name);
void ae_randomizer_tags_unloaded(void);

/* whether the randomizer injected the firing range's profile for this level (the level's own inject leaves it) */
int ae_randomizer_range_injected(void);

/* an actor's own weapon, or the randomizer's, given to its unit: 1 when the randomizer gave one, 0
when the actor's own is to be given (a level it does not play is always 0) */
int randomizer_actor_weapon(long weapon_definition, long unit_index, long variant_index);

/* PLAYER LOADOUT's weapons of AE's (ae_randomizer_loadout.c, for randomizer.c's loadout code): 1 when `value` is an "ae:<id>" value,
with the weapon's definition (NONE when the level has not got it: logged) in *found; 1 when the game is to fall back to the level's own
loadout because the primary asked for (an AE weapon's or an item set's value) is not on the level (logged); and AE's weapons that are on the
level added to RANDOM's choices when game.campaign_ae_weapons is on */
int ae_randomizer_loadout_weapon(char const *value, long *found);
int ae_randomizer_loadout_primary_missing(char const *requested, long found_definition);
void ae_randomizer_loadout_add_ae(long *definitions, long *count, long maximum);

#endif /* __AE_RANDOMIZER_GLUE_H */
