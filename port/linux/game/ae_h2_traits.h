/* The second-generation game's player traits (the AE block, ae_variant_ext_rules.h): every source in one place
(every player, in a scoring hill, carrying the ball, a flag or the bomb, the juggernaut), asked by the game's damage
multiplier, its camouflage, weapons' ammo and the per-tick update. Nothing applies with the default block. */
#ifndef AE_H2_TRAITS_H
#define AE_H2_TRAITS_H

enum
{
	AE_H2_TRAIT_INVISIBLE = 0x01,
	AE_H2_TRAIT_DAMAGE_RESISTANCE = 0x02,
	AE_H2_TRAIT_EXTRA_DAMAGE = 0x04,
	AE_H2_TRAIT_OVERSHIELD = 0x08,
	AE_H2_TRAIT_INFINITE_AMMO = 0x10,
	AE_H2_TRAIT_MOTION_SENSOR = 0x20,
};

/* the traits a player has now (AE_H2_TRAIT_*) */
int ae_h2_traits_of(long player_index);
/* game_engine_get_damage_multiplier: the dealer's extra damage (x2) and the victim's resistance (x0.5) */
float ae_h2_traits_damage_multiplier(long damaging_player_index, long damaged_player_index);
/* game_engine_update_player_always_invis */
int ae_h2_traits_invisible(long player_index);
/* weapons.c: a weapon its holder's traits keep full */
int ae_h2_traits_weapon_infinite_ammo(long weapon_index);
/* each tick, each player (the host): the juggernaut's overshield kept up */
void ae_h2_traits_update(long player_index);
/* whether the motion sensor shows for this player whatever the gametype says */
int ae_h2_traits_motion_sensor(long player_index);

#endif
