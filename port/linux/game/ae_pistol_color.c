/*
AE_PISTOL_COLOR.C

PISTOL COLOR and PISTOL TRIM (VIEW > WEAPON COSMETICS, display.pistol_color,
display.pistol_trim, each player's own, DEFAULT): your pistol in first person
drawn in a multiplayer armor colour of your choice, its silver body (COLOR)
and its dark parts (TRIM). Cosmetic and your own view's only: the pistol in
others' hands, in third person and on the ground is the game's, and nothing
is sent (owner 2026-10-24).

The stock pistol's first-person model has two shaders, weapons\pistol\fp\
shaders\silver and ...\black. The model renderer gives each shader a change
colour (white for these, which take none); here it is the chosen colour while
those two shaders are drawn in a first-person pass, by the view's own player's
setting. The black one has no multipurpose map, so the renderer's default one
lets the colour reach all of it; the silver one's map masks the colour out of
its body (2026-10-24, the Xbox maps), so while a COLOR is chosen it is drawn
with the default one too. No tag data is written. A map or mod whose pistol
has other shaders is left as it is.

The idea is aLTis94's pistol_color.lua for Chimera (ideas only: it has no
licence, and no code of it is here).
*/

#include "cseries.h"
#include "math/real_math.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "tag_files/tag_groups.h"
#include "saved games/player_profile.h"

#include "ae_pistol_color.h"

#include <string.h>

/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_text(int player, const char *name, char *text, size_t size);
unsigned long ae_config_player_changes(void);
void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* (shader_definitions.h's model shaders' group) */
#define AE_SHADER_MODEL_TAG 'soso'

enum
{
	AE_PISTOL_BODY,
	AE_PISTOL_TRIM,
	AE_PISTOL_PARTS,
	AE_PISTOL_PLAYERS = 4
};

/* the multiplayer armor colors, in the profile's order (player_profile.c, shield_color.c's) */
static char const *const pistol_color_names[] =
{
	"white", "black", "red", "blue", "gray", "yellow", "green", "pink", "purple",
	"cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon",
};

static char const *const pistol_shader_names[AE_PISTOL_PARTS] =
{
	"weapons\\pistol\\fp\\shaders\\silver",
	"weapons\\pistol\\fp\\shaders\\black",
};

static struct
{
	/* the two shaders' tag data on this map (NULL: not in it), found for the scenario they were looked up in */
	long scenario_index;
	boolean found;
	void const *shaders[AE_PISTOL_PARTS];
	/* each player's choice: a profile colour's index, NONE for DEFAULT */
	unsigned long read_at;
	boolean read;
	short colors[AE_PISTOL_PLAYERS][AE_PISTOL_PARTS];
} pistol;

static short pistol_color_index(
	char const *text)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(pistol_color_names); index++)
	{
		if (!strcmp(text, pistol_color_names[index]))
			return index;
	}
	return NONE;
}

static void pistol_color_read(
	void)
{
	static char const *const settings[AE_PISTOL_PARTS] = { "display.pistol_color", "display.pistol_trim" };
	short player, part;

	if (pistol.read && pistol.read_at == ae_config_player_changes())
		return;
	pistol.read = TRUE;
	pistol.read_at = ae_config_player_changes();
	for (player = 0; player < AE_PISTOL_PLAYERS; player++)
	{
		for (part = 0; part < AE_PISTOL_PARTS; part++)
		{
			char text[32] = "";

			ae_config_player_text(player, settings[part], text, sizeof(text));
			pistol.colors[player][part] = pistol_color_index(text);
		}
	}
}

static void pistol_shaders_find(
	void)
{
	short part;

	if (pistol.found && pistol.scenario_index == global_scenario_index)
		return;
	pistol.found = TRUE;
	pistol.scenario_index = global_scenario_index;
	for (part = 0; part < AE_PISTOL_PARTS; part++)
	{
		long index = tag_loaded(AE_SHADER_MODEL_TAG, pistol_shader_names[part]);

		pistol.shaders[part] = index != NONE ? tag_get(AE_SHADER_MODEL_TAG, index) : NULL;
	}
	platform_log("pistol color: the first-person pistol's shaders %s, %s", pistol.shaders[AE_PISTOL_BODY] ?
		"silver found" : "no silver", pistol.shaders[AE_PISTOL_TRIM] ? "black found" : "no black");
}

/* the view's player's colour of a part, NONE for DEFAULT or none of these shaders */
static short pistol_color_part(
	void const *shader,
	boolean first_person)
{
	short player = render.local_player_index;
	short part;

	if (!first_person || !shader || player < 0 || player >= AE_PISTOL_PLAYERS)
		return NONE;
	pistol_color_read();
	if (pistol.colors[player][AE_PISTOL_BODY] == NONE && pistol.colors[player][AE_PISTOL_TRIM] == NONE)
		return NONE;
	pistol_shaders_find();
	for (part = 0; part < AE_PISTOL_PARTS; part++)
	{
		if (shader == pistol.shaders[part])
			return pistol.colors[player][part] != NONE ? part : NONE;
	}
	return NONE;
}

long ae_pistol_color_multipurpose(
	void const *shader,
	boolean first_person,
	long multipurpose_map_index)
{
	return pistol_color_part(shader, first_person) == AE_PISTOL_BODY ? NONE : multipurpose_map_index;
}

void ae_pistol_color_apply(
	void const *shader,
	boolean first_person,
	real_rgb_color *change_color)
{
	short part = pistol_color_part(shader, first_person);

	if (part != NONE && change_color)
		*change_color = player_profile_get_rgb_color(pistol.colors[render.local_player_index][part]);
}
