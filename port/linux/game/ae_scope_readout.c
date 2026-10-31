/*
AE_SCOPE_READOUT.C

SCOPE READOUTS: the EVO RIFLE's zoom overlay (the owner's pick D8, 10-30:
Halo CE's soft scope surround on a wide window) has two frames at the window's
sides, MAG and AMMO; the game draws their values live, the zoom as the rifle's
profile has it ("2x": a change of the profile's zoom shows at once, no art to
redraw) and the rounds loaded (a network client sees them as the host does).

Placed by tools/weapon_pack/evo_scope.py's numbers, in HUD units (480 to the
view's height) from the view's centre, where the overlay is drawn: a split
view's are as big as a full one's, as the overlay's own art is.
*/

#include <stdio.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "items/weapons.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_burst_rifle.h"
#include "ae_hud_digits.h"
#include "ae_hud_text.h"
#include "ae_rifle_profiles.h"
#include "ae_scope_readout.h"

/* evo_scope.py's READOUT: the value's outer edge (the label's), its baseline and its em, HUD units from the centre
(its size: 46 px at 1080p, as the mockup) */
#define AE_SCOPE_VALUE_X 164.9f
#define AE_SCOPE_VALUE_Y 9.8f
#define AE_SCOPE_VALUE_EM 20.4f

static void draw_value(short local_player_index, real edge_x, real baseline_y, char const *text, boolean right,
	real px)
{
	union real_argb_color color;
	wchar_t line[16];
	int at;

	/* (the build's wchar_t is two bytes: no swprintf) */
	for (at = 0; text[at] && at < (int)NUMBEROF(line) - 1; at++)
		line[at] = (wchar_t)text[at];
	line[at] = 0;
	color.alpha = 0.96f;
	color.red = 214.0f / 255.0f;
	color.green = 232.0f / 255.0f;
	color.blue = 1.0f;
	ae_hud_text_draw(local_player_index, right ? edge_x - ae_hud_text_measure(line, px) : edge_x, baseline_y, line,
		px, &color, TRUE);
}

void ae_scope_readout_draw(short local_player_index, long unit_index, long weapon_index)
{
	struct ae_rifle_profile const *profile;
	struct weapon_datum *weapon;
	char text[16];
	real ppu, cx, cy;

	if (unit_index == NONE || weapon_index == NONE || unit_get_zoom_level(unit_index) == NONE)
		return;
	weapon = weapon_get(weapon_index);
	profile = ae_burst_rifle_profile_of(weapon->definition_index);
	if (!profile || !profile->scope_readouts)
		return;
	ppu = ae_hud_pixels_per_unit();
	cx = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu / 2.0f;
	cy = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu / 2.0f;
	ae_rifle_zoom_text(profile->zoom_seen, text, sizeof(text));
	draw_value(local_player_index, cx - AE_SCOPE_VALUE_X * ppu, cy + AE_SCOPE_VALUE_Y * ppu, text, FALSE,
		AE_SCOPE_VALUE_EM * ppu);
	snprintf(text, sizeof(text), "%d", (int)weapon->weapon.magazines[0].rounds_loaded);
	draw_value(local_player_index, cx + AE_SCOPE_VALUE_X * ppu, cy + AE_SCOPE_VALUE_Y * ppu, text, TRUE,
		AE_SCOPE_VALUE_EM * ppu);
}
