/* The second-generation game's player traits (see the header) */
#include "cseries.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "units/units.h"
#include "ae_variant_ext.h"
#include "ae_h2_traits.h"

boolean player_has_ball(long player_index);
boolean game_engine_player_has_flag(long player_index);
boolean ae_strip_king_player_on_hill(long player_index);
boolean ae_bomb_snd_unit_carries(long unit_index);
void player_handle_powerup_minor(long player_index, short powerup_type, short duration);

static int engine(void)
{
	struct game_variant const *variant = game_engine_get_variant();

	return variant ? (int)variant->game_engine_index : 0;
}

static int is_juggernaut(long player_index)
{
	struct game_variant const *variant = game_engine_get_variant();

	/* (the oddball engine's terminator ball: CE's Juggernaut) */
	return variant && engine() == game_engine_oddball && variant->game_engine_variant.oddball.oddball_ball_type == 2 &&
		player_has_ball(player_index);
}

int ae_h2_traits_of(long player_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct player_datum *player;
	int traits = 0;

	if (player_index == NONE || !game_engine_running() || ae_variant_ext_is_default(ext))
		return 0;
	player = player_get(player_index);
	if (ext->player_flags & AE_EXT_PLAYER_DAMAGE_RESISTANCE)
		traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
	if (ext->player_flags & AE_EXT_PLAYER_EXTRA_DAMAGE)
		traits |= AE_H2_TRAIT_EXTRA_DAMAGE;
	switch (engine())
	{
	case game_engine_king:
		if (ext->mode != AE_EXT_MODE_TERRITORIES && ae_strip_king_player_on_hill(player_index))
		{
			if (ext->king_flags & AE_EXT_KING_CAMO_ON_HILL)
				traits |= AE_H2_TRAIT_INVISIBLE;
			if (ext->king_flags & AE_EXT_KING_DAMAGE_RESISTANCE_ON_HILL)
				traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
			if (ext->king_flags & AE_EXT_KING_EXTRA_DAMAGE_ON_HILL)
				traits |= AE_H2_TRAIT_EXTRA_DAMAGE;
		}
		break;
	case game_engine_oddball:
		if (is_juggernaut(player_index))
		{
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_CAMO)
				traits |= AE_H2_TRAIT_INVISIBLE;
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_DAMAGE_RESISTANCE)
				traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_EXTRA_DAMAGE)
				traits |= AE_H2_TRAIT_EXTRA_DAMAGE;
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_INFINITE_AMMO)
				traits |= AE_H2_TRAIT_INFINITE_AMMO;
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_MOTION_SENSOR)
				traits |= AE_H2_TRAIT_MOTION_SENSOR;
			if (ext->juggernaut_flags & AE_EXT_JUGGERNAUT_OVERSHIELD)
				traits |= AE_H2_TRAIT_OVERSHIELD;
		}
		else if (player_has_ball(player_index))
		{
			if (ext->oddball_flags & AE_EXT_ODDBALL_CARRIER_CAMO)
				traits |= AE_H2_TRAIT_INVISIBLE;
			if (ext->oddball_flags & AE_EXT_ODDBALL_TOUGHNESS)
				traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
		}
		break;
	case game_engine_ctf:
		if (game_engine_player_has_flag(player_index))
		{
			if (ext->ctf_flags & AE_EXT_CTF_CARRIER_CAMO)
				traits |= AE_H2_TRAIT_INVISIBLE;
			if (ext->ctf_flags & AE_EXT_CTF_CARRIER_DAMAGE_RESISTANCE)
				traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
		}
		break;
	case game_engine_bomb:
		if (player->unit_index != NONE && ae_bomb_snd_unit_carries(player->unit_index))
		{
			if (ext->bomb_flags & AE_EXT_BOMB_CARRIER_CAMO)
				traits |= AE_H2_TRAIT_INVISIBLE;
			if (ext->bomb_flags & AE_EXT_BOMB_CARRIER_DAMAGE_RESISTANCE)
				traits |= AE_H2_TRAIT_DAMAGE_RESISTANCE;
		}
		break;
	}
	return traits;
}

float ae_h2_traits_damage_multiplier(long damaging_player_index, long damaged_player_index)
{
	float result = 1.0f;

	if (damaging_player_index != NONE && (ae_h2_traits_of(damaging_player_index) & AE_H2_TRAIT_EXTRA_DAMAGE))
		result *= ae_ext_damage_dealt_percent(1) / 100.0f;
	if (damaged_player_index != NONE && (ae_h2_traits_of(damaged_player_index) & AE_H2_TRAIT_DAMAGE_RESISTANCE))
		result *= ae_ext_damage_taken_percent(1) / 100.0f;
	return result;
}

int ae_h2_traits_invisible(long player_index)
{
	return (ae_h2_traits_of(player_index) & AE_H2_TRAIT_INVISIBLE) != 0;
}

int ae_h2_traits_motion_sensor(long player_index)
{
	return (ae_h2_traits_of(player_index) & AE_H2_TRAIT_MOTION_SENSOR) != 0;
}

int ae_h2_traits_weapon_infinite_ammo(long weapon_index)
{
	struct weapon_datum *weapon;
	long unit_index;
	long player_index;

	if (weapon_index == NONE || !game_engine_running() || ae_variant_ext_is_default(ae_variant_ext_game()))
		return 0;
	weapon = weapon_get(weapon_index);
	unit_index = weapon->object.parent_object_index;
	if (unit_index == NONE)
		return 0;
	player_index = unit_get(unit_index)->unit.player_index;
	return player_index != NONE && (ae_h2_traits_of(player_index) & AE_H2_TRAIT_INFINITE_AMMO) != 0;
}

void ae_h2_traits_update(long player_index)
{
	struct player_datum *player;
	struct object_datum *object;

	if (!(ae_h2_traits_of(player_index) & AE_H2_TRAIT_OVERSHIELD))
		return;
	player = player_get(player_index);
	if (player->unit_index == NONE)
		return;
	/* (the overshield kept charged while the trait holds: a shield above full, as the overshield powerup leaves it) */
	object = object_get(player->unit_index);
	if (object->object.shield_vitality < 2.0f)
		object->object.shield_vitality = 2.0f;
}
