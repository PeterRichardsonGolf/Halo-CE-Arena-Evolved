/*
AE_MODES.H

Arena Evolved's modes of Slayer, on OpenCE's gametypes (game_modes.c): what
its AE hook lines (and damage.c's) call (port/linux/game/ae_hooks.txt lists
them). Included after cseries.h.

A mode is a gametype's options' mode (game_engine.h's game_variant_options),
in Arena Evolved's own range of values, AE_GAME_MODE_FIRST up, so neither
OpenCE's next mode nor ours takes the other's. Every rule of these modes is
the host's (weapons, grenades, kills), so a stock OpenCE guest, which sees
"Slayer" (its sanitize knows no such mode), plays them as the host does.
*/

#ifndef __AE_MODES_H
#define __AE_MODES_H

struct game_engine;
struct game_variant;
struct game_variant_options;

enum
{
	AE_GAME_MODE_FIRST = 0x40,
	AE_GAME_MODE_ONE_IN_THE_CHAMBER = AE_GAME_MODE_FIRST,
	AE_GAME_MODE_FRAG_NATION,
	AE_GAME_MODE_MELEE_ONLY,
	AE_GAME_MODE_SNOWBALL_FIGHT,
	AE_GAME_MODE_END
};

/* ---- the built-in gametypes, after OpenCE's (game_modes_gametype's) */

short ae_modes_gametype_count(void);
wchar_t const *ae_modes_gametype_name(short index);
/* a built-in gametype of AE's by its stored name (11 characters at most: a saved gametype's), -1 for none */
short ae_modes_gametype_of_stored_name(wchar_t const *stored);
/* index (from the first of AE's) built; FALSE: not one of AE's */
boolean ae_modes_gametype(short index, struct game_variant *variant, struct game_variant_options *options);
/* debug.network_test's names ("one_in_the_chamber", "frag_nation", "melee_only", "needles",
"snowball_fight") */
boolean ae_modes_gametype_by_name(char const *name, struct game_variant *variant,
	struct game_variant_options *options);

/* ---- a gametype's options */

/* a mode value of AE's */
boolean ae_modes_known(short mode);
/* the mode a gametype plays, if it is AE's (a Slayer gametype's), else none */
short ae_modes_variant_mode(struct game_variant const *variant, struct game_variant_options const *options);
/* its kind's name, in full or abbreviated, NULL for none of AE's */
wchar_t const *ae_modes_kind_name(short mode, boolean hardcore, boolean abbreviated);

/* ---- the game being played */

/* the game engine the game plays with: AE's mode's, wrapping Slayer's, else the one given */
struct game_engine *ae_modes_engine(struct game_engine *engine);
/* a player's spawn, after its starting equipment and OpenCE's modes' */
void ae_modes_postspawn(long player_index);
/* damage.c: a hit on an object that kills it outright (One in the Chamber's: a bullet, damage.c's bullet or
sniper category) */
boolean ae_modes_force_kill(long owner_player_index, long object_index, boolean bullet);
/* game_engine_bomb.c's game_engine_bomb_hides_weapon: a weapon that is a mode's unseen fists (Chupa's
melee-only fists: none of it drawn, a bare crosshair, no shot, never dropped or picked up) */
boolean ae_modes_hides_weapon(long definition_index);
/* damage.c, around game_statistics_record_kill: the kill's damage (NONE after) */
void ae_modes_kill_damage(long damage_definition_index);

#endif // __AE_MODES_H
