#include "cseries.h"
#include <string.h>
#include "game/game_engine.h"
#include "ae_h2_presets.h"
#include "ae_variant_ext.h"

#define TICKS 30

void platform_log(char const *format, ...);

void ae_h2_preset_apply(int index, struct game_variant *variant, struct game_variant_options *options)
{
	struct ae_h2_preset const *p = ae_h2_preset_get(index);
	struct universal_variant *u;

	if (!p || p->merge)
		return;
	u = &variant->universal_variant;
	if (p->twist & AE_H2_TWIST_SCORE)
		u->score_to_win = p->score;
	if (p->twist & AE_H2_TWIST_LIVES)
		u->lives = p->lives;
	if (p->twist & AE_H2_TWIST_RESPAWN)
		u->respawn_time = p->respawn_seconds * TICKS;
	if (p->twist & AE_H2_TWIST_SUICIDE)
		u->suicide_penalty = p->suicide_seconds * TICKS;
	if (p->twist & AE_H2_TWIST_BETRAYAL)
		options->friendly_fire_penalty = p->betrayal_seconds;
	if (p->twist & AE_H2_TWIST_SHIELDS)
		SET_FLAG(u->flags, _game_variant_no_shields_bit, p->no_shields);
	if (p->twist & AE_H2_TWIST_CAMO)
		SET_FLAG(u->flags, _game_variant_always_invisible_bit, p->camo);
	if (p->twist & AE_H2_TWIST_RADAR)
	{
		SET_FLAG(u->flags, _game_variant_draw_object_in_motion_sensor_bit, p->radar);
		options->radar_players = p->radar ? _radar_players_enemies : _radar_players_none;
	}
	if (p->twist & AE_H2_TWIST_WEAPONS)
	{
		u->weapon_set = p->weapon_set;
		options->no_map_weapons = p->no_map_weapons;
	}
	if (p->twist & AE_H2_TWIST_LOADOUT)
	{
		options->loadout = _loadout_custom;
		options->primary_weapon = p->primary;
		options->secondary_weapon = p->secondary;
	}
	if ((p->twist & AE_H2_TWIST_BALL_SPEED) && p->engine == AE_H2_ENGINE_ODDBALL)
		variant->game_engine_variant.oddball.speed_with_ball = p->ball_speed;
	if ((p->twist & AE_H2_TWIST_CRAZY) && p->engine == AE_H2_ENGINE_KING)
		variant->game_engine_variant.king.moving_hill = TRUE;
	/* (territories pin their hills: the base King's moving hill is not claimed, owner 2026-11-04) */
	if (p->engine == AE_H2_ENGINE_TERRITORIES)
		variant->game_engine_variant.king.moving_hill = FALSE;
	if (p->engine == AE_H2_ENGINE_CTF)
	{
		if (p->twist & AE_H2_TWIST_AT_HOME)
			variant->game_engine_variant.ctf.flag_at_home_to_score = p->flag_at_home;
		if (p->twist & AE_H2_TWIST_SINGLE)
			variant->game_engine_variant.ctf.single_flag_time = p->single_flag_seconds;
	}
	if (p->engine == AE_H2_ENGINE_BOMB)
	{
		if (p->twist & AE_H2_TWIST_ARM)
			variant->game_engine_variant.bomb.arm_time = p->arm_seconds;
		if (p->twist & AE_H2_TWIST_RESET)
			variant->game_engine_variant.bomb.reset_time = p->reset_seconds;
	}
}

char const *ae_h2_preset_base(int index)
{
	struct ae_h2_preset const *p = ae_h2_preset_get(index);

	return p && !p->merge ? p->base : NULL;
}

int ae_h2_preset_no_vehicles(int index)
{
	struct ae_h2_preset const *p = ae_h2_preset_get(index);

	return p && p->no_vehicles;
}

void ae_h2_preset_choose_variant(struct game_variant const *variant)
{
	static wchar_t last[12];
	struct ae_variant_ext ext;
	int index = ae_h2_preset_find_wide(variant->human_readable_game_description);

	/* (the same gametype set again, as the lobby does when a game starts: GAME OPTIONS' changes to it stay) */
	if (!memcmp(last, variant->human_readable_game_description, sizeof(last)))
		return;
	memcpy(last, variant->human_readable_game_description, sizeof(last));
	ae_h2_preset_ext(index, &ext);
	ae_variant_ext_choose(&ext);
	platform_log("ae presets: %s chosen (rounds %d, round time %d s, mode %d, territories %d)",
		index >= 0 ? ae_h2_preset_get(index)->stored : "a gametype that is no preset", (int)ext.rounds,
		(int)ext.round_time_10s * 10, (int)ext.mode, (int)ext.territories);
}

void ae_h2_preset_file_block(struct game_variant const *variant, unsigned char *block)
{
	struct ae_variant_ext ext;
	int index = ae_h2_preset_find_wide(variant->human_readable_game_description);

	if (index < 0)
		return;
	ae_h2_preset_ext(index, &ext);
	ae_variant_ext_to_block(block, &ext);
}
