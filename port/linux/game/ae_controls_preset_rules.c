/*
AE_CONTROLS_PRESET_RULES.C

The game's button presets and the CONTROLS screen's layouts (ae_controls_preset_rules.h).
*/

#include "ae_controls_preset_rules.h"

#include "../src/controller_layout.h"

/* the game's presets' names (player_profile.h's order; the profiles screen's: ae_screen_profiles.c) */
static char const *const game_preset_titles[AE_CONTROLS_GAME_PRESETS] =
{
	"DEFAULT", "SWAP TRIGGERS", "SWAP A, L TRIGGER", "SWAP B, L TRIGGER", "SWAP B, R STICK", "BUMPER JUMPER",
	"FISHSTICKS", "HERO", "RECON", "THE DUKE", "ANNIVERSARY", "RECLAIMER", "ZOOM & SHOOT", "BUMP & JUMP",
	"GREEN FINGERS", "CUSTOM",
};

int ae_controls_layout_of(
	int game_preset)
{
	if (game_preset == AE_CONTROLS_GAME_CUSTOM)
		return CONTROLLER_PRESET_CUSTOM;
	if (game_preset >= 0 && game_preset < CONTROLLER_PRESET_CUSTOM)
		return game_preset;
	return -1;
}

int ae_controls_game_preset_of(
	int layout)
{
	if (layout == CONTROLLER_PRESET_CUSTOM)
		return AE_CONTROLS_GAME_CUSTOM;
	if (layout >= 0 && layout < CONTROLLER_PRESET_CUSTOM)
		return layout;
	return -1;
}

int ae_controls_step(
	int game_preset,
	int direction)
{
	int layout = ae_controls_layout_of(game_preset);
	int count = CONTROLLER_PRESET_CUSTOM + 1;
	int index;

	direction = direction < 0 ? -1 : 1;
	if (layout >= 0)
		return ae_controls_game_preset_of((layout + direction + count) % count);
	/* (an unknown preset: the next known one in the game's order, wrapping) */
	if (direction > 0)
	{
		for (index = 0; index < count; index++)
		{
			if (ae_controls_game_preset_of(index) > game_preset)
				return ae_controls_game_preset_of(index);
		}
		return ae_controls_game_preset_of(0);
	}
	for (index = count - 1; index >= 0; index--)
	{
		if (ae_controls_game_preset_of(index) < game_preset)
			return ae_controls_game_preset_of(index);
	}
	return ae_controls_game_preset_of(count - 1);
}

char const *ae_controls_preset_title(
	int game_preset)
{
	return game_preset >= 0 && game_preset < AE_CONTROLS_GAME_PRESETS ? game_preset_titles[game_preset] :
		game_preset_titles[0];
}
