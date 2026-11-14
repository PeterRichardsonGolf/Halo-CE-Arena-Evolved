/*
AE_BOMB_SND_RULES.H

AE SEARCH AND DESTROY as a gametype rule (owner 2026-10-26: "just make it a gametype"), pure: a Bomb gametype
(game_engine_bomb.c) with this bit set plays SEARCH AND DESTROY on AE's Bomb rules (ae_bomb_assault.h: one life a
round, classic competitive rounds). The seeded AE SND (seed revision 26) has it; config game.bomb_snd stays as a
test override that turns it on for any Bomb game.

The gametype keeps it in bit 7 of the Bomb variant's flags (game_engine.h's struct bomb_variant: bits 0-1 are
Chupa's _bomb_new_round_on_score_bit and _bomb_melee_only_bit), which game_engine_variant_cleanup keeps (an AE
hook). Only the host's matters: it plays the rounds and sends SEARCH AND DESTROY in its bomb state
(ae_bomb_assault_write_state; a client follows that, ae_bomb_snd_shown). The byte travels with the variant, the
file's and the network's sizes unchanged; a Chupa or older build drops the bit and plays it as AE BOMB.
*/

#ifndef __AE_BOMB_SND_RULES_H
#define __AE_BOMB_SND_RULES_H

#define AE_BOMB_SND_FLAG 0x80u

static inline int ae_bomb_snd_flag(unsigned char flags)
{
	return (flags & AE_BOMB_SND_FLAG) != 0;
}

/* a loose bomb out of reach in SEARCH AND DESTROY (it never resets for lying untouched there): lying lower than a fall
that kills (lethal) below the map's lowest player spawn (a pit). (Asked only of a bomb on the map: one not connected
to it is in a player's inventory, game_engine_bomb.c's loose test) */
static inline int ae_bomb_snd_out_of_reach(float z, float lowest_spawn_z, float lethal)
{
	return z < lowest_spawn_z - lethal;
}

/* or lying in deep water (owner 2026-10-28: water that kills a player is out of reach, as a pit is): deeper than a
standing player's eyes (the cyborg's standing camera height, 0.62 world units). Shallower water, where a player can
still walk to it, keeps it where it lies */
#define AE_BOMB_SND_DEEP_WATER 0.62f

static inline int ae_bomb_snd_in_deep_water(float water_depth)
{
	return water_depth > AE_BOMB_SND_DEEP_WATER;
}

static inline unsigned char ae_bomb_snd_flag_set(unsigned char flags, int on)
{
	return (unsigned char)(on ? (flags | AE_BOMB_SND_FLAG) : (flags & ~AE_BOMB_SND_FLAG));
}

/* BOMB DROP (owner 2026-10-28, GAME OPTIONS' row): bit 6 of the same flags. On, the carrier drops the bomb at their
feet by holding the action button AE_BOMB_SND_DROP_HOLD_TICKS (0.5 s) with nothing in reach; off, only a death drops
it. The action button because the host sees it held every tick (a held weapon swap reaches it as one
request). Host only: the drop is the host's, every machine sees the bomb fall */
#define AE_BOMB_SND_DROP_FLAG 0x40u
#define AE_BOMB_SND_DROP_HOLD_TICKS 15

static inline int ae_bomb_snd_drop_flag(unsigned char flags)
{
	return (flags & AE_BOMB_SND_DROP_FLAG) != 0;
}

/* the carrier's hold, a tick on: held the ticks it has lasted (0: not held); holding the action button this tick;
free: nothing in reach to act on. The next count; *drop 1 the tick it reaches the hold (once a hold) */
static inline int ae_bomb_snd_drop_hold_next(int held, int holding, int free, int *drop)
{
	int next = holding && free ? held + 1 : 0;

	*drop = next == AE_BOMB_SND_DROP_HOLD_TICKS;
	return next;
}

#endif
