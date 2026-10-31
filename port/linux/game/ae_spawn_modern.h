/*
AE_SPAWN_MODERN.H

(Arena Evolved) AE MODERN SPAWNS (ae_spawn_modern.c): score every spawn point, keep the best step of scores, reject
points in sight of a player, draw among what is left. It is on when the game's gametype says MODERN (GAME OPTIONS >
RESPAWN > SPAWNS); HALO_AE_SPAWN_RULE=modern / classic overrides that for tests. Host only: players.c's
find_best_starting_location_index asks it instead of its own loop.

The gametype keeps SPAWNS in universal_variant.ball_melee's bits 4-5 (0 CLASSIC, 1 MODERN), BALL MELEE in its low
nibble (1 LETHAL), bit 6 the weapon slots' rank 6/7 bit (ae_weapon_slots.h): every read of that byte as BALL MELEE
goes through ae_ball_melee_of. A host without this reads 0x10 as BALL MELEE STOCK, which it is. These helpers need no engine headers (unit test: ae_setup_options_test.c).
*/

#ifndef __AE_SPAWN_MODERN_H
#define __AE_SPAWN_MODERN_H

enum { AE_SPAWNS_CLASSIC = 0, AE_SPAWNS_MODERN = 1 };

/* the byte's BALL MELEE (its low nibble) */
static inline int ae_ball_melee_of(unsigned char byte)
{
	return byte & 0x0F;
}

/* the byte's SPAWNS (bits 4-5) */
static inline int ae_spawns_of(unsigned char byte)
{
	return (byte >> 4) & 3;
}

/* the byte with BALL MELEE / SPAWNS set and the other's bits kept */
static inline unsigned char ae_ball_melee_set(unsigned char byte, int value)
{
	return (unsigned char)((byte & 0xF0) | (value & 0x0F));
}

static inline unsigned char ae_spawns_set(unsigned char byte, int value)
{
	return (unsigned char)((byte & 0xCF) | ((value & 3) << 4));
}

/* the "AE spawns:" line of the game's rules (from the gametype's byte) */
void ae_spawn_modern_log_rules(unsigned char byte);
/* the game's gametype says MODERN (HALO_AE_SPAWN_RULE=modern / classic overrides) and a game engine is running */
int ae_spawn_modern_allowed(void);
/* the rule's pick, or NONE: the player waits for a free point (retried next tick) */
long ae_spawn_modern_choose(long player_index, short location_count);
/* (game_engine.c's player death) a player's death spot, kept 12 s */
void ae_spawn_modern_player_killed(long player_index);
/* (game_engine.c, HALO_AE_SPAWN_LOG=1) damage within 3 s of a spawn, a unit placed well above its point */
void ae_spawn_modern_update(void);

#endif
