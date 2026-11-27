/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Mode 2: 3D In-Engine Mode (Single-Player & Online Multiplayer / Co-op)
 * - 6DOF 343 Guilty Spark Monitor flight with native Mouse Look
 * - 3rd-Person Monitor View toggle (F4 or console `third_person`)
 * - Physics Beam / Raycast Object Grab, Move, Rotate (15/45/90 deg snap) & Ground Snap
 * - Halo 3-Style Left-Aligned Compact Forge Menu & Object Properties using small terminal font
 * - Interactive AI Unit Placement (Friendly vs. Enemy)
 * - "Inject Custom Tags" UI Modal & "Save Copy As..." Standalone Map Export Modal
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "camera/director.h"
#include "camera/observer.h"
#include "effects/effects.h"
#include "effects/decals.h"
#include "halo_skulls.h"
#include "objects/objects.h"
#include "units/units.h"
#include "physics/collisions.h"
#include "interface/interface.h"
#include "interface/ui_widget.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "render/render_cameras.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "items/items.h"
#include "items/item_definitions.h"

#include "objects/object_lights.h"
#include "objects/widgets/widgets.h"
#include "devices/devices.h"
#include "devices/device_light_fixtures.h"
#include "forge_engine.h"
#include "forge_map_settings.h"
#include "forge_volumes.h"
#include "forge_ai_routes.h"
#include "halo_ui_pointer.h"

extern int halo_forge_pointer_update(int wanted, struct halo_ui_pointer *pointer);
extern int ae_forge_dev_mode_toggle(int mode); /* AE hook */

#undef fopen
#undef remove
#define fopen(p, m) ((FILE *)forge_host_fopen((p), (m)))
#define remove(p) forge_host_remove((p))

extern tag tag_get_group_tag(long tag_index);
extern unsigned long system_milliseconds(void);
/* (the platform layer's: port/linux/src/xinput_sdl.c, sdl_platform.h) */
extern int halo_linux_mouse_look(short controller_index, float *yaw_delta, float *pitch_delta);
extern int halo_linux_touch_move(short controller_index, float *forward, float *strafe);
extern int halo_linux_touch_look(short gamepad_index, float *yaw, float *pitch, float *gyro_yaw, float *gyro_pitch);
extern boolean input_abstraction_port_look_inverted(short port);
extern void platform_scoreboard_scroll(int open, long *notches, long *pages);
extern boolean render_camera_world_to_screen(
	const struct render_camera *camera,
	const struct render_frustum *frustum,
	const real_point3d *world_point,
	real_point2d *screen_point);
extern boolean rasterizer_bitmap_new(struct bitmap_data *bitmap);
extern void halo_menus_art_register(void const *texture, char const *png);
extern long hud_hires_asset_count(void);
extern char const *hud_hires_asset_tag(long asset);
extern void hud_hires_register_placeholder(long asset, unsigned long const *texture);
extern long hud_hires_placeholder_asset(unsigned long data);
extern short controller_glyph_style(void);
extern boolean controller_glyph_lookup(short style, short slot, struct bitmap_data **bitmap, short bounds[4]);
extern int config_boolean(char const *name);

extern void SDL_free(void *mem);

struct forge_font_char_metric
{
	word character;
	short character_width;
	short bitmap_width;
	short bitmap_height;
	short bitmap_origin_x;
	short bitmap_origin_y;
	short hardware_character_index;
	word pad;
	long pixels_offset;
};

static int s_controls_loaded = 0;
static int s_gp_btn_down[17];
static float s_gp_lx = 0.0f, s_gp_ly = 0.0f, s_gp_rx = 0.0f, s_gp_ry = 0.0f, s_gp_lt = 0.0f, s_gp_rt = 0.0f;

void forge_controls_init_defaults(void)
{
	g_forge.bind_vk[FORGE_BIND_TOGGLE_MODE]    = 0x70; /* VK_F1 */
	g_forge.bind_vk[FORGE_BIND_PALETTE]        = 0x71; /* VK_F2 */
	g_forge.bind_vk[FORGE_BIND_THIRD_PERSON]   = 0x73; /* VK_F4 */
	g_forge.bind_vk[FORGE_BIND_QUICK_SAVE]     = 0x74; /* VK_F5 */
	g_forge.bind_vk[FORGE_BIND_ROTATE_HOLD]    = 0xA4; /* VK_LMENU (Left Alt) */
	g_forge.bind_vk[FORGE_BIND_SNAP_FLOOR]     = 'Z';
	g_forge.bind_vk[FORGE_BIND_SNAP_SURFACE]   = 'F';
	g_forge.bind_vk[FORGE_BIND_ROT_SNAP]       = 'G';
	g_forge.bind_vk[FORGE_BIND_OBJECT_OPTIONS] = 'V';
	g_forge.bind_vk[FORGE_BIND_DELETE_OBJECT]  = 0x2E; /* VK_DELETE */
	g_forge.bind_vk[FORGE_BIND_COPY_OBJECT]    = 'C';
	g_forge.bind_vk[FORGE_BIND_LOOK_SETTINGS]  = 'V';

	/* (D-pad Up is the chat's: network_chat.c, social_chat.c) */
	g_forge.bind_gp[FORGE_BIND_TOGGLE_MODE]    = FORGE_GP_DPAD_RIGHT;
	g_forge.bind_gp[FORGE_BIND_PALETTE]        = FORGE_GP_X;
	g_forge.bind_gp[FORGE_BIND_THIRD_PERSON]   = FORGE_GP_BACK;
	g_forge.bind_gp[FORGE_BIND_QUICK_SAVE]     = FORGE_GP_DPAD_DOWN;
	g_forge.bind_gp[FORGE_BIND_ROTATE_HOLD]    = FORGE_GP_RTRIGGER;
	g_forge.bind_gp[FORGE_BIND_SNAP_FLOOR]     = FORGE_GP_DPAD_LEFT;
	g_forge.bind_gp[FORGE_BIND_SNAP_SURFACE]   = FORGE_GP_RTHUMB;
	g_forge.bind_gp[FORGE_BIND_ROT_SNAP]       = FORGE_GP_LTHUMB;
	g_forge.bind_gp[FORGE_BIND_OBJECT_OPTIONS] = FORGE_GP_X;
	g_forge.bind_gp[FORGE_BIND_DELETE_OBJECT]  = FORGE_GP_Y;
	/* (with SNAP TO FLOOR: that one while an object is held, this one while
	none is) */
	g_forge.bind_gp[FORGE_BIND_COPY_OBJECT]    = FORGE_GP_DPAD_LEFT;
	g_forge.bind_gp[FORGE_BIND_LOOK_SETTINGS]  = FORGE_GP_Y;

	g_forge.controller_look_sens = 1.0f;
	g_forge.controller_fly_speed = 1.0f;
}

void forge_controls_load(void)
{
	FILE *fp;
	char line[256];
	int saw_copy_bind = 0;
	int migrated_options_vk = 0;

	forge_controls_init_defaults();
	s_controls_loaded = 1;

	fp = fopen("Faber_data/forge_controls.ini", "r");
	if (!fp)
		return;
	while (fgets(line, sizeof(line), fp))
	{
		int idx = -1, vk = 0, gp = 0;
		float fval = 1.0f;
		if (sscanf(line, "bind_%d=%d,%d", &idx, &vk, &gp) == 3)
		{
			if (idx >= 0 && idx < FORGE_BIND_COUNT)
			{
				if (idx == FORGE_BIND_ROTATE_HOLD && vk == 'R')
					vk = 0xA4; /* Upgrade legacy 'R' default to Left Alt (VK_LMENU) */
				if (idx == FORGE_BIND_OBJECT_OPTIONS && vk == 'X')
				{
					vk = 'V'; /* Upgrade legacy 'X' keyboard default to 'V' */
					migrated_options_vk = 1;
				}
				if (idx == FORGE_BIND_COPY_OBJECT)
					saw_copy_bind = 1;
				/* (D-pad Up is the chat's: Y, which only deletes what is held) */
				if (idx == FORGE_BIND_LOOK_SETTINGS && gp == FORGE_GP_DPAD_UP)
					gp = FORGE_GP_Y;
				g_forge.bind_vk[idx] = vk;
				g_forge.bind_gp[idx] = gp;
			}
		}
		else if (sscanf(line, "controller_look_sens=%f", &fval) == 1)
		{
			if (fval >= 0.2f && fval <= 4.0f)
				g_forge.controller_look_sens = fval;
		}
		else if (sscanf(line, "controller_fly_speed=%f", &fval) == 1)
		{
			if (fval >= 0.2f && fval <= 4.0f)
				g_forge.controller_fly_speed = fval;
		}
	}
	fclose(fp);

	/* Migrate pre-v2.1.2 controller default binds when loading an older 10-bind INI */
	if (!saw_copy_bind &&
		g_forge.bind_gp[FORGE_BIND_PALETTE] == FORGE_GP_B &&
		g_forge.bind_gp[FORGE_BIND_ROTATE_HOLD] == FORGE_GP_LTRIGGER &&
		g_forge.bind_gp[FORGE_BIND_DELETE_OBJECT] == FORGE_GP_BLACK)
	{
		g_forge.bind_gp[FORGE_BIND_PALETTE]        = FORGE_GP_X;
		g_forge.bind_gp[FORGE_BIND_ROTATE_HOLD]    = FORGE_GP_RTRIGGER;
		g_forge.bind_gp[FORGE_BIND_SNAP_FLOOR]     = FORGE_GP_DPAD_LEFT;
		g_forge.bind_gp[FORGE_BIND_SNAP_SURFACE]   = FORGE_GP_RTHUMB;
		g_forge.bind_gp[FORGE_BIND_ROT_SNAP]       = FORGE_GP_LTHUMB;
		g_forge.bind_gp[FORGE_BIND_OBJECT_OPTIONS] = FORGE_GP_X;
		g_forge.bind_gp[FORGE_BIND_DELETE_OBJECT]  = FORGE_GP_Y;
		g_forge.bind_gp[FORGE_BIND_COPY_OBJECT]    = FORGE_GP_DPAD_RIGHT;
		migrated_options_vk = 1;
	}
	/* Migrate the toggle off D-pad Up, which opens the chat: D-pad Right,
	and COPY, which had it, to D-pad Left (SNAP TO FLOOR's, which acts only
	while an object is held, COPY only while none is) */
	if (g_forge.bind_gp[FORGE_BIND_TOGGLE_MODE] == FORGE_GP_DPAD_UP)
	{
		g_forge.bind_gp[FORGE_BIND_TOGGLE_MODE] = FORGE_GP_DPAD_RIGHT;
		if (g_forge.bind_gp[FORGE_BIND_COPY_OBJECT] == FORGE_GP_DPAD_RIGHT)
			g_forge.bind_gp[FORGE_BIND_COPY_OBJECT] = FORGE_GP_DPAD_LEFT;
		migrated_options_vk = 1;
	}
	if (migrated_options_vk)
		forge_controls_save();
}

void forge_controls_save(void)
{
	FILE *fp = fopen("Faber_data/forge_controls.ini", "w");
	int i;
	if (!fp)
		return;
	for (i = 0; i < FORGE_BIND_COUNT; i++)
		fprintf(fp, "bind_%d=%d,%d\n", i, g_forge.bind_vk[i], g_forge.bind_gp[i]);
	fprintf(fp, "controller_look_sens=%.2f\n", g_forge.controller_look_sens);
	fprintf(fp, "controller_fly_speed=%.2f\n", g_forge.controller_fly_speed);
	fclose(fp);
}

char const *forge_vk_display_name(int vk)
{
	static char s_buf[24];
	if (vk <= 0) return "-";
	if (vk >= 'A' && vk <= 'Z') { s_buf[0] = (char)vk; s_buf[1] = 0; return s_buf; }
	if (vk >= '0' && vk <= '9') { s_buf[0] = (char)vk; s_buf[1] = 0; return s_buf; }
	if (vk >= 0x70 && vk <= 0x7B) { snprintf(s_buf, sizeof(s_buf), "F%d", vk - 0x70 + 1); return s_buf; }
	switch (vk)
	{
	case 0x01: return "LMB";
	case 0x02: return "RMB";
	case 0x04: return "MMB";
	case 0x08: return "BKSP";
	case 0x09: return "TAB";
	case 0x0D: return "ENTER";
	case 0x10: case 0xA0: case 0xA1: return "SHIFT";
	case 0x11: case 0xA2: case 0xA3: return "CTRL";
	case 0xA4: return "L-ALT";
	case 0xA5: return "R-ALT";
	case 0x12: return "ALT";
	case 0x1B: return "ESC";
	case 0x20: return "SPACE";
	case 0x21: return "PGUP";
	case 0x22: return "PGDN";
	case 0x23: return "END";
	case 0x24: return "HOME";
	case 0x25: return "LEFT";
	case 0x26: return "UP";
	case 0x27: return "RIGHT";
	case 0x28: return "DOWN";
	case 0x2D: return "INS";
	case 0x2E: return "DEL";
	case 0xBD: return "-";
	case 0xBB: return "=";
	case 0xDB: return "[";
	case 0xDD: return "]";
	default:
		snprintf(s_buf, sizeof(s_buf), "KEY %02X", vk & 0xFF);
		return s_buf;
	}
}

char const *forge_gp_display_name(int gp)
{
	switch (gp)
	{
	case FORGE_GP_A:          return "A BUTTON";
	case FORGE_GP_B:          return "B BUTTON";
	case FORGE_GP_X:          return "X BUTTON";
	case FORGE_GP_Y:          return "Y BUTTON";
	case FORGE_GP_BLACK:      return "BLACK / RB";
	case FORGE_GP_WHITE:      return "WHITE / LB";
	case FORGE_GP_LTRIGGER:   return "L TRIGGER";
	case FORGE_GP_RTRIGGER:   return "R TRIGGER";
	case FORGE_GP_DPAD_UP:    return "D-PAD UP";
	case FORGE_GP_DPAD_DOWN:  return "D-PAD DOWN";
	case FORGE_GP_DPAD_LEFT:  return "D-PAD LEFT";
	case FORGE_GP_DPAD_RIGHT: return "D-PAD RIGHT";
	case FORGE_GP_START:      return "START";
	case FORGE_GP_BACK:       return "BACK";
	case FORGE_GP_LTHUMB:     return "L THUMB";
	case FORGE_GP_RTHUMB:     return "R THUMB";
	default:                  return "-";
	}
}

char const *forge_gp_icon_token(int gp)
{
	switch (gp)
	{
	case FORGE_GP_A:          return "{A}";
	case FORGE_GP_B:          return "{B}";
	case FORGE_GP_X:          return "{X}";
	case FORGE_GP_Y:          return "{Y}";
	case FORGE_GP_BLACK:      return "{BL}";
	case FORGE_GP_WHITE:      return "{WH}";
	case FORGE_GP_LTRIGGER:   return "{LT}";
	case FORGE_GP_RTRIGGER:   return "{RT}";
	case FORGE_GP_DPAD_UP:    return "{DU}";
	case FORGE_GP_DPAD_DOWN:  return "{DD}";
	case FORGE_GP_DPAD_LEFT:  return "{DL}";
	case FORGE_GP_DPAD_RIGHT: return "{DR}";
	case FORGE_GP_START:      return "{ST}";
	case FORGE_GP_BACK:       return "{BK}";
	case FORGE_GP_LTHUMB:     return "{LS}";
	case FORGE_GP_RTHUMB:     return "{RS}";
	default:                  return "[-]";
	}
}

char const *forge_action_prompt(int action)
{
	static char s_pbuf[16][24];
	static int s_pidx = 0;
	char *dst;

	if (!s_controls_loaded)
		forge_controls_load();
	if (action < 0 || action >= FORGE_BIND_COUNT)
		return "";
	if (g_forge.using_controller && g_forge.bind_gp[action] > 0)
		return forge_gp_icon_token(g_forge.bind_gp[action]);

	dst = s_pbuf[s_pidx++ & 15];
	snprintf(dst, 24, "[%s]", forge_vk_display_name(g_forge.bind_vk[action]));
	return dst;
}

/* (the Switch's) whether Forge's menus and its monitor, whose A and B are all
Forge's, take the right face button, marked A, as A and the bottom one, marked
B, as B, as the game's menus do (controls.nintendo_menu_buttons: xinput_sdl.c's
menu_buttons_swapped); the buttons are named by where they sit everywhere else,
a Spartan's A being a jump */
static int forge_menu_buttons_swapped(void)
{
#ifdef HALO_SWITCH
	return (g_forge.active_modal != _forge_modal_none || g_forge.mode == _forge_mode_3d_monitor) &&
		config_boolean("controls.nintendo_menu_buttons");
#else
	return 0;
#endif
}

/* whether the pad's A and B are still to be swapped here: a game's menu up over
Forge (its pause menu, the controls screen) has swapped them already */
static int forge_pad_buttons_to_swap(void)
{
#ifdef HALO_SWITCH
	extern int xinput_menu_buttons_swapped(void);

	return forge_menu_buttons_swapped() && !xinput_menu_buttons_swapped();
#else
	return 0;
#endif
}

void forge_poll_controller_state(float *out_lx, float *out_ly, float *out_rx, float *out_ry, float *out_lt, float *out_rt)
{
	struct forge_pad pad;
	int any_gp_activity = 0;
	int i;

	if (!s_controls_loaded)
		forge_controls_load();

	memset(s_gp_btn_down, 0, sizeof(s_gp_btn_down));
	s_gp_lx = s_gp_ly = s_gp_rx = s_gp_ry = s_gp_lt = s_gp_rt = 0.0f;

	/* (the first controller as the game gets it, through the port's
	controls: forge_pad_read; its sticks' up made SDL's negative, its
	triggers' 0-255 SDL's 0-32767, as the rest of this reads them) */
	if (forge_pad_read(&pad))
	{
		int16_t raw_lx = pad.lx;
		int16_t raw_ly = (int16_t)(pad.ly == -32768 ? 32767 : -pad.ly);
		int16_t raw_rx = pad.rx;
		int16_t raw_ry = (int16_t)(pad.ry == -32768 ? 32767 : -pad.ry);
		int16_t raw_lt = (int16_t)(pad.analog[6] * 128);
		int16_t raw_rt = (int16_t)(pad.analog[7] * 128);
		int const deadzone = 6200;
		int const pressed = 30;

		s_gp_btn_down[FORGE_GP_A]          = pad.analog[forge_pad_buttons_to_swap() ? 1 : 0] >= pressed;
		s_gp_btn_down[FORGE_GP_B]          = pad.analog[forge_pad_buttons_to_swap() ? 0 : 1] >= pressed;
		s_gp_btn_down[FORGE_GP_X]          = pad.analog[2] >= pressed;
		s_gp_btn_down[FORGE_GP_Y]          = pad.analog[3] >= pressed;
		s_gp_btn_down[FORGE_GP_BACK]       = (pad.buttons & 0x0020) != 0;
		s_gp_btn_down[FORGE_GP_START]      = (pad.buttons & 0x0010) != 0;
		s_gp_btn_down[FORGE_GP_LTHUMB]     = (pad.buttons & 0x0040) != 0;
		s_gp_btn_down[FORGE_GP_RTHUMB]     = (pad.buttons & 0x0080) != 0;
		s_gp_btn_down[FORGE_GP_WHITE]      = pad.analog[5] >= pressed;
		s_gp_btn_down[FORGE_GP_BLACK]      = pad.analog[4] >= pressed;
		s_gp_btn_down[FORGE_GP_DPAD_UP]    = (pad.buttons & 0x0001) != 0;
		s_gp_btn_down[FORGE_GP_DPAD_DOWN]  = (pad.buttons & 0x0002) != 0;
		s_gp_btn_down[FORGE_GP_DPAD_LEFT]  = (pad.buttons & 0x0004) != 0;
		s_gp_btn_down[FORGE_GP_DPAD_RIGHT] = (pad.buttons & 0x0008) != 0;

		if (raw_lt > 8000) s_gp_btn_down[FORGE_GP_LTRIGGER] = 1;
		if (raw_rt > 8000) s_gp_btn_down[FORGE_GP_RTRIGGER] = 1;

		if (raw_lt > 2000) s_gp_lt = (float)(raw_lt - 2000) / (float)(32767 - 2000);
		if (raw_rt > 2000) s_gp_rt = (float)(raw_rt - 2000) / (float)(32767 - 2000);

		if (abs((int)raw_lx) > deadzone)
			s_gp_lx = (float)(raw_lx > 0 ? (raw_lx - deadzone) : (raw_lx + deadzone)) / (float)(32767 - deadzone);
		if (abs((int)raw_ly) > deadzone)
			s_gp_ly = -(float)(raw_ly > 0 ? (raw_ly - deadzone) : (raw_ly + deadzone)) / (float)(32767 - deadzone);
		if (abs((int)raw_rx) > deadzone)
			s_gp_rx = (float)(raw_rx > 0 ? (raw_rx - deadzone) : (raw_rx + deadzone)) / (float)(32767 - deadzone);
		if (abs((int)raw_ry) > deadzone)
			s_gp_ry = -(float)(raw_ry > 0 ? (raw_ry - deadzone) : (raw_ry + deadzone)) / (float)(32767 - deadzone);

		for (i = 1; i <= 16; i++)
		{
			if (s_gp_btn_down[i])
				any_gp_activity = 1;
		}
		if ((float)fabs((double)s_gp_lx) > 0.12f || (float)fabs((double)s_gp_ly) > 0.12f ||
			(float)fabs((double)s_gp_rx) > 0.12f || (float)fabs((double)s_gp_ry) > 0.12f ||
			s_gp_lt > 0.15f || s_gp_rt > 0.15f)
		{
			any_gp_activity = 1;
		}
	}

	/* (the touch controls' stick, which is not the pad's: touch_input_move. It
	counts with no pad connected, so outside forge_pad_read) */
	{
		float forward, strafe;

		if (halo_linux_touch_move(0, &forward, &strafe))
		{
			s_gp_lx = -strafe;
			s_gp_ly = forward;
			any_gp_activity = 1;
		}
	}

	if (any_gp_activity)
	{
		g_forge.using_controller = 1;
	}
	else if (forge_window_is_focused())
	{
		static int const kbm_watch[] = {
			'W', 'A', 'S', 'D', 'Q', 'E', 'R', 'F', 'G', 'Z', 'X', 'C', 'V',
			0x01, 0x02, 0x0D, 0x12, 0x1B, 0x20, 0x25, 0x26, 0x27, 0x28, 0x70, 0x71, 0x73, 0x74, 0xA4
		};
		for (i = 0; i < (int)(sizeof(kbm_watch) / sizeof(kbm_watch[0])); i++)
		{
			if (GetAsyncKeyState(kbm_watch[i]) & 0x8000)
			{
				g_forge.using_controller = 0;
				break;
			}
		}
	}

	if (out_lx) *out_lx = s_gp_lx;
	if (out_ly) *out_ly = s_gp_ly;
	if (out_rx) *out_rx = s_gp_rx;
	if (out_ry) *out_ry = s_gp_ry;
	if (out_lt) *out_lt = s_gp_lt;
	if (out_rt) *out_rt = s_gp_rt;
}

int forge_gp_button_down(int gp)
{
	if (gp <= 0 || gp > 16)
		return 0;
	return s_gp_btn_down[gp];
}

int forge_action_down(int action)
{
	int vk, gp;
	if (!s_controls_loaded)
		forge_controls_load();
	if (action < 0 || action >= FORGE_BIND_COUNT)
		return 0;
	vk = g_forge.bind_vk[action];
	gp = g_forge.bind_gp[action];
	if (action == FORGE_BIND_ROTATE_HOLD)
	{
		if ((GetAsyncKeyState(0xA4) & 0x8000) ||
			(GetAsyncKeyState(0x12) & 0x8000) ||
			(GetAsyncKeyState('R') & 0x8000))
		{
			return 1;
		}
	}
	if (vk > 0 && (GetAsyncKeyState(vk) & 0x8000))
		return 1;
	if (gp > 0 && forge_gp_button_down(gp))
		return 1;
	return 0;
}

int forge_get_local_player_abs_index(void)
{
	long unit_idx = player_control_get_unit_index(0);
	if (unit_idx != NONE)
	{
		struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
		if (u && u->unit.player_index != NONE)
			return (int)((uint32_t)u->unit.player_index & 0xFFFFu);
	}
	if (player_data)
	{
		long p_idx;
		for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
		{
			struct player_datum *pl = player_get(p_idx);
			if (pl && pl->local_player_index == 0)
				return (int)((uint32_t)p_idx & 0xFFFFu);
		}
	}
	return 0;
}

int forge_is_unit_in_monitor_mode(long unit_index)
{
	struct unit_datum *u;
	int p_abs;

	if (!g_forge_session_enabled || !g_forge.initialized || unit_index == NONE)
		return 0;
	u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_biped);
	if (!u || u->object.type != _object_type_biped || u->unit.player_index == NONE)
		return 0;
	if (unit_index == player_control_get_unit_index(0))
		return (g_forge.mode == _forge_mode_3d_monitor) ? 1 : 0;
	p_abs = (int)((uint32_t)u->unit.player_index & 0xFFFFu);
	if (p_abs >= 0 && p_abs < 128)
		return g_forge.player_is_monitor[p_abs] ? 1 : 0;
	return 0;
}

static void forge_hide_spartan_linked_attachments_and_children(long unit_idx, int hide_linked)
{
	struct unit_datum *u;
	struct object_definition *def;
	long child_idx;
	short a;

	u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
	if (!u)
		return;

	/* Never set u->unit.active_camouflage = 1.0f during simulation ticks!
	 * Setting active_camouflage > 0.0f during simulation causes network_distributed.c on the host
	 * to broadcast _distributed_unit_camouflaged_bit to all clients. Instead, active_camouflage
	 * is applied only around the local render pass in forge_render_begin/end_monitor_visibility(). */
	if (!hide_linked)
	{
		u->unit.active_camouflage = 0.0f;
		SET_FLAG(u->unit.flags, _unit_active_camouflaged_bit, FALSE);
	}

	if (hide_linked && u->definition_index != NONE)
	{
		def = object_definition_get(u->definition_index);
		if (def)
		{
			for (a = 0; a < def->object.attachments.count; a++)
			{
				struct object_attachment_definition *att = TAG_BLOCK_GET_ELEMENT(
					&def->object.attachments, a, struct object_attachment_definition);
				if (att && att->type.group_tag == 'ligh' && att->type.index != NONE)
				{
					/* Clear _light_definition_dont_fade_active_camouflage_bit (bit 5) so helmet lights fade out completely */
					uint32_t *lflags = (uint32_t *)tag_get('ligh', att->type.index);
					if (lflags)
						*lflags &= ~(1u << 5);
				}
			}
		}
	}

	for (child_idx = u->object.first_child_object_index; child_idx != NONE; )
	{
		struct object_datum *c_obj = (struct object_datum *)object_try_and_get_and_verify_type(child_idx, (unsigned long)-1);
		if (!c_obj)
			break;
		SET_FLAG(c_obj->object.flags, _object_invisible_bit, hide_linked ? TRUE : FALSE);
		child_idx = c_obj->object.next_object_index;
	}
}

static float s_saved_render_camo[128];
static uint8_t s_saved_render_camo_valid[128];

void forge_render_begin_monitor_visibility(void)
{
	long p_idx;
	if (!g_forge_session_enabled || !g_forge.initialized || !player_data)
		return;
	for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
	{
		struct player_datum *pl = player_get(p_idx);
		int p_abs = (int)((uint32_t)p_idx & 0xFFFFu);
		if (!pl || pl->unit_index == NONE || p_abs < 0 || p_abs >= 128)
			continue;
		if (forge_is_unit_in_monitor_mode(pl->unit_index))
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(pl->unit_index, _object_mask_unit);
			forge_hide_spartan_linked_attachments_and_children(pl->unit_index, 1);
			if (u)
			{
				s_saved_render_camo[p_abs] = u->unit.active_camouflage;
				s_saved_render_camo_valid[p_abs] = 1;
				u->unit.active_camouflage = 1.0f;
			}
		}
		else
		{
			s_saved_render_camo_valid[p_abs] = 0;
			forge_hide_spartan_linked_attachments_and_children(pl->unit_index, 0);
		}
	}
}

void forge_render_end_monitor_visibility(void)
{
	long p_idx;
	if (!g_forge_session_enabled || !g_forge.initialized || !player_data)
		return;
	for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
	{
		struct player_datum *pl = player_get(p_idx);
		int p_abs = (int)((uint32_t)p_idx & 0xFFFFu);
		if (!pl || pl->unit_index == NONE || p_abs < 0 || p_abs >= 128)
			continue;
		if (s_saved_render_camo_valid[p_abs])
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(pl->unit_index, _object_mask_unit);
			if (u)
			{
				u->unit.active_camouflage = s_saved_render_camo[p_abs];
				SET_FLAG(u->unit.flags, _unit_active_camouflaged_bit, FALSE);
			}
			s_saved_render_camo_valid[p_abs] = 0;
		}
	}
}

#define FORGE_IN_MATCH_HUD_SCALE 0.75f
#define FORGE_COUNTER_HUD_SCALE (FORGE_IN_MATCH_HUD_SCALE * 0.5f)

static float s_hud_ui_scale = 1.0f;
static float s_hud_ui_origin_x = 0.0f;
static float s_hud_ui_origin_y = 0.0f;

static void set_forge_hud_ui_scale(float scale, float origin_x, float origin_y)
{
	s_hud_ui_scale = (scale > 0.05f) ? scale : 1.0f;
	s_hud_ui_origin_x = origin_x;
	s_hud_ui_origin_y = origin_y;
}

static int s_props_row_has_value[16];
/* (a row of two buttons, its halves each its own: a route point's ADD) */
static int s_props_row_split[16];

/* ---------- a Forge menu's mouse

An open menu frees the mouse (PC and web: halo_forge_pointer_update). The
pointer picks what is under it, as the arrows do, a click is A on it (a row's
value goes back on its left and on on its right), the wheel scrolls and a right
click is B. The menus say where things are as they draw them, in the screen's
units, and the next update reads those places. */
enum
{
	_forge_hit_palette_row,
	_forge_hit_palette_previous,
	_forge_hit_palette_next,
	_forge_hit_palette_place,
	_forge_hit_palette_done,
	_forge_hit_props_row,
	_forge_hit_save_ok,
	_forge_hit_save_cancel,
	/* (MAP SETTINGS': a row, a listed tag or a key, forge_map_settings_menu.inl) */
	_forge_hit_settings_row,
};

static void forge_map_settings_menu_hover(int index);
static int forge_map_settings_menu_row_has_value(int index);
static int forge_map_settings_menu_paging(void);

struct forge_hit
{
	short x0, y0, x1, y1;
	short kind, index;
};

/* (MAP SETTINGS' keyboard has 44 keys) */
static struct forge_hit s_hits[64];
static int s_hit_count;
/* what the pointer did this update, for the keys' reading */
static struct
{
	int enter, back, left, right, up, down;
} s_pointer_input;

static void forge_hit_add(int kind, int index, short x0, short y0, short x1, short y1)
{
	struct forge_hit *hit;

	if (s_hit_count >= (int)NUMBEROF(s_hits))
		return;
	hit = &s_hits[s_hit_count++];
	hit->x0 = (short)(s_hud_ui_origin_x + ((float)x0 - s_hud_ui_origin_x) * s_hud_ui_scale);
	hit->y0 = (short)(s_hud_ui_origin_y + ((float)y0 - s_hud_ui_origin_y) * s_hud_ui_scale);
	hit->x1 = (short)(s_hud_ui_origin_x + ((float)x1 - s_hud_ui_origin_x) * s_hud_ui_scale);
	hit->y1 = (short)(s_hud_ui_origin_y + ((float)y1 - s_hud_ui_origin_y) * s_hud_ui_scale);
	hit->kind = (short)kind;
	hit->index = (short)index;
}

static struct forge_hit const *forge_hit_at(short x, short y)
{
	int i;

	for (i = 0; i < s_hit_count; i++)
	{
		if (x >= s_hits[i].x0 && x < s_hits[i].x1 && y >= s_hits[i].y0 && y < s_hits[i].y1)
			return &s_hits[i];
	}
	return NULL;
}

static void forge_pointer_poll(void)
{
	struct halo_ui_pointer pointer;
	struct forge_hit const *hit;
	int wanted = g_forge_session_enabled && g_forge.active_modal != _forge_modal_none &&
		!forge_is_main_menu_active() && !ui_widgets_active();

	memset(&s_pointer_input, 0, sizeof(s_pointer_input));
	if (!halo_forge_pointer_update(wanted, &pointer) || !wanted)
		return;
	if (pointer.moved && (hit = forge_hit_at(pointer.x, pointer.y)) != NULL)
	{
		if (hit->kind == _forge_hit_palette_row)
			g_forge.palette_cursor = hit->index;
		else if (hit->kind == _forge_hit_props_row)
		{
			g_forge.props_cursor = hit->index;
			if (s_props_row_split[hit->index & 15])
				forge_ai_route_add_side_set(pointer.x >= (hit->x0 + hit->x1) / 2);
		}
		else if (hit->kind == _forge_hit_settings_row)
			forge_map_settings_menu_hover(hit->index);
	}
	if (pointer.wheel_steps)
	{
		if (g_forge.active_modal == _forge_modal_spawn_palette)
		{
			s_pointer_input.up = pointer.wheel_steps > 0;
			s_pointer_input.down = pointer.wheel_steps < 0;
		}
		else if (g_forge.active_modal == _forge_modal_object_properties)
		{
			s_pointer_input.left = pointer.wheel_steps < 0;
			s_pointer_input.right = pointer.wheel_steps > 0;
		}
		else if (g_forge.active_modal == _forge_modal_map_settings && forge_map_settings_menu_paging())
		{
			s_pointer_input.up = pointer.wheel_steps > 0;
			s_pointer_input.down = pointer.wheel_steps < 0;
		}
	}
	if (pointer.right_clicks)
		s_pointer_input.back = 1;
	if (!pointer.left_clicks || (hit = forge_hit_at(pointer.click_x, pointer.click_y)) == NULL)
		return;
	switch (hit->kind)
	{
	case _forge_hit_palette_row:
		g_forge.palette_cursor = hit->index;
		s_pointer_input.enter = 1;
		break;
	case _forge_hit_palette_previous:
		s_pointer_input.left = 1;
		break;
	case _forge_hit_palette_next:
		s_pointer_input.right = 1;
		break;
	case _forge_hit_palette_place:
		s_pointer_input.enter = 1;
		break;
	case _forge_hit_palette_done:
	case _forge_hit_save_cancel:
		s_pointer_input.back = 1;
		break;
	case _forge_hit_props_row:
		g_forge.props_cursor = hit->index;
		/* (a row of two buttons: the half clicked, pressed) */
		if (s_props_row_split[hit->index & 15])
		{
			forge_ai_route_add_side_set(pointer.click_x >= (hit->x0 + hit->x1) / 2);
			s_pointer_input.enter = 1;
		}
		/* (on a value's left, back; anything else, as A) */
		else if (pointer.click_x < (hit->x0 + hit->x1) / 2 && s_props_row_has_value[hit->index & 15])
			s_pointer_input.left = 1;
		else
			s_pointer_input.enter = 1;
		break;
	case _forge_hit_save_ok:
		s_pointer_input.enter = 1;
		break;
	case _forge_hit_settings_row:
		forge_map_settings_menu_hover(hit->index);
		/* (on a value's left, back; anything else, as A) */
		if (pointer.click_x < (hit->x0 + hit->x1) / 2 && forge_map_settings_menu_row_has_value(hit->index))
			s_pointer_input.left = 1;
		else
			s_pointer_input.enter = 1;
		break;
	}
}

static void draw_quad(rectangle2d const *r, uint32_t argb)
{
	short x0, y0, x1, y1;
	if (!r || r->x1 <= r->x0 || r->y1 <= r->y0 || ((argb >> 24) & 0xFFu) == 0)
		return;
	if (s_hud_ui_scale != 1.0f)
	{
		x0 = (short)(s_hud_ui_origin_x + ((float)r->x0 - s_hud_ui_origin_x) * s_hud_ui_scale + 0.5f);
		y0 = (short)(s_hud_ui_origin_y + ((float)r->y0 - s_hud_ui_origin_y) * s_hud_ui_scale + 0.5f);
		x1 = (short)(s_hud_ui_origin_x + ((float)r->x1 - s_hud_ui_origin_x) * s_hud_ui_scale + 0.5f);
		y1 = (short)(s_hud_ui_origin_y + ((float)r->y1 - s_hud_ui_origin_y) * s_hud_ui_scale + 0.5f);
		if (x1 <= x0) x1 = (short)(x0 + 1);
		if (y1 <= y0) y1 = (short)(y0 + 1);
	}
	else
	{
		x0 = r->x0;
		y0 = r->y0;
		x1 = r->x1;
		y1 = r->y1;
	}
	forge_draw_screen_rect(x0, y0, x1, y1, argb);
}

static void draw_box_border(short x0, short y0, short x1, short y1, short thick, uint32_t argb)
{
	rectangle2d r;
	if (((argb >> 24) & 0xFFu) == 0 || thick <= 0)
		return;
	r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = (short)(y0 + thick); draw_quad(&r, argb);
	r.x0 = x0; r.y0 = (short)(y1 - thick); r.x1 = x1; r.y1 = y1; draw_quad(&r, argb);
	r.x0 = x0; r.y0 = y0; r.x1 = (short)(x0 + thick); r.y1 = y1; draw_quad(&r, argb);
	r.x0 = (short)(x1 - thick); r.y0 = y0; r.x1 = x1; r.y1 = y1; draw_quad(&r, argb);
}

static long get_small_forge_font(long fallback_font)
{
	long f = tag_loaded('font', "ui\\small_ui");
	if (f == NONE)
		f = tag_loaded('font', "ui\\large_ui");
	if (f == NONE)
		f = interface_get_tag_index(_interface_font_system);
	if (f == NONE)
		f = fallback_font;
	return f;
}

static long get_large_forge_font(long fallback_font)
{
	long f = tag_loaded('font', "ui\\large_ui");
	if (f == NONE)
		f = tag_loaded('font', "ui\\small_ui");
	if (f == NONE)
		f = interface_get_tag_index(_interface_font_system);
	if (f == NONE)
		f = fallback_font;
	return f;
}

/* ============================================================================
 * Editable Forge UI Theme & PNG Background System (Faber_data/ui/forge_ui.xml)
 * ============================================================================ */

enum forge_ui_area_id
{
	_forge_ui_area_spawn_menu = 0,
	_forge_ui_area_preview_panel,
	_forge_ui_area_properties_menu,
	_forge_ui_area_top_bar,
	_forge_ui_area_bottom_bar,
	_forge_ui_area_save_modal,
	_forge_ui_area_maps_modal,
	_forge_ui_area_toast_bar,
	_forge_ui_area_markers,
	NUMBER_OF_FORGE_UI_AREAS
};

struct forge_ui_area_style
{
	char font_name[64];
	short font_style; /* -1=plain, 0=bold, 1=italic, 2=condense, 3=underline */
	float font_scale;
	uint32_t title_color;
	uint32_t text_color;
	uint32_t selected_color;
	uint32_t bg_color;
	uint32_t header_color;
	uint32_t highlight_color;
	uint32_t border_color;
	uint32_t image_tint;
	char bg_image[64];
	int use_image;
	struct bitmap_data bitmap;
	uint8_t dummy_pixels[16];
	int bitmap_initialized;
	int bitmap_valid;
};

static struct
{
	int initialized;
	int reload_rev;
	char registered_map[128];
	struct forge_ui_area_style areas[NUMBER_OF_FORGE_UI_AREAS];
} s_ui_theme;

static uint32_t parse_hex_color_attr(char const *str, uint32_t fallback)
{
	unsigned long val = 0;
	char const *p = str;
	size_t len;

	if (!p || !p[0])
		return fallback;
	if (*p == '#')
		p++;
	else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;
	len = strlen(p);
	if (len != 6 && len != 8)
		return fallback;
	val = strtoul(p, NULL, 16);
	if (len == 6)
		val |= 0xFF000000ul;
	return (uint32_t)val;
}

static short parse_font_style_attr(char const *str)
{
	if (!str || !str[0])
		return NONE;
	if (_stricmp(str, "bold") == 0)
		return 0; /* _text_style_bold */
	if (_stricmp(str, "italic") == 0)
		return 1; /* _text_style_italic */
	if (_stricmp(str, "condense") == 0 || _stricmp(str, "condensed") == 0)
		return 2; /* _text_style_condense */
	if (_stricmp(str, "underline") == 0)
		return 3; /* _text_style_underline */
	return NONE; /* plain */
}

static int extract_xml_attr(char const *tag_start, char const *tag_end, char const *attr_name, char *out_val, size_t out_sz)
{
	size_t nlen = strlen(attr_name);
	char const *p = tag_start;

	while (p && p + nlen + 2 < tag_end)
	{
		if (_strnicmp(p, attr_name, nlen) == 0 &&
			(p == tag_start || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r' || p[-1] == '\n'))
		{
			char const *eq = p + nlen;
			while (eq < tag_end && (*eq == ' ' || *eq == '\t'))
				eq++;
			if (eq < tag_end && *eq == '=')
			{
				char quote;
				char const *val_start;
				char const *val_end;
				size_t copy_len;
				eq++;
				while (eq < tag_end && (*eq == ' ' || *eq == '\t'))
					eq++;
				if (eq < tag_end && (*eq == '"' || *eq == '\''))
				{
					quote = *eq++;
					val_start = eq;
					val_end = memchr(val_start, quote, (size_t)(tag_end - val_start));
					if (val_end)
					{
						copy_len = (size_t)(val_end - val_start);
						if (copy_len >= out_sz)
							copy_len = out_sz - 1;
						memcpy(out_val, val_start, copy_len);
						out_val[copy_len] = 0;
						return 1;
					}
				}
			}
		}
		p++;
	}
	return 0;
}

static void init_default_area_style(struct forge_ui_area_style *st, char const *default_png)
{
	snprintf(st->font_name, sizeof(st->font_name), "large");
	st->font_style = NONE;
	st->font_scale = 1.0f;
	st->title_color = 0xFFFFFFFFu;
	st->text_color = 0xFF2896FFu;
	st->selected_color = 0xFFFFFFFFu;
	st->bg_color = 0xC00A2649u;
	st->header_color = 0x00000000u;
	st->highlight_color = 0xD8225C9Cu;
	st->border_color = 0xFF2896FFu;
	st->image_tint = 0xFFFFFFFFu;
	snprintf(st->bg_image, sizeof(st->bg_image), "%s", default_png ? default_png : "none");
	st->use_image = (default_png && default_png[0] && _stricmp(default_png, "none") != 0) ? 1 : 0;
}

static void sync_and_register_area_png(struct forge_ui_area_style *st, int area_idx)
{
	char src_path[256];
	char rel_png[128];
	FILE *in_f;

	(void)area_idx;
	st->bitmap_valid = 0;
	if (!st->use_image || !st->bg_image[0] || _stricmp(st->bg_image, "none") == 0)
		return;

	snprintf(src_path, sizeof(src_path), "Faber_data/ui/%s", st->bg_image);
	in_f = fopen(src_path, "rb");
	if (!in_f)
		return;
	fclose(in_f);

	snprintf(rel_png, sizeof(rel_png), "ui/%s", st->bg_image);

#ifndef HALO_64BIT /* AE hook */
	if (!st->bitmap_initialized)
	{
		memset(&st->bitmap, 0, sizeof(st->bitmap));
		memset(st->dummy_pixels, 0xFF, sizeof(st->dummy_pixels));
		st->bitmap.signature = 'bitm';
		st->bitmap.width = 2;
		st->bitmap.height = 2;
		st->bitmap.depth = 1;
		st->bitmap.type = 0;   /* _bitmap_type_2d_texture */
		st->bitmap.format = 11; /* _bitmap_format_a8r8g8b8 */
		st->bitmap.flags = 1;  /* _bitmap_has_power_of_two_dimensions_bit */
		st->bitmap.base_address = st->dummy_pixels;
		if (rasterizer_bitmap_new(&st->bitmap) && st->bitmap.hardware_format)
			st->bitmap_initialized = 1;
	}

	if (st->bitmap_initialized && st->bitmap.hardware_format)
	{
		halo_menus_art_register(st->bitmap.hardware_format, rel_png);
		st->bitmap_valid = 1;
	}
#endif /* AE hook */
}

static void parse_area_xml_node(char const *xml_text, char const *node_name, struct forge_ui_area_style *st)
{
	char open_pat[80];
	char const *p;
	char const *end;
	char val[128];

	snprintf(open_pat, sizeof(open_pat), "<%s", node_name);
	p = strstr(xml_text, open_pat);
	if (!p)
		return;
	end = strchr(p, '>');
	if (!end)
		return;

	if (extract_xml_attr(p, end, "font", val, sizeof(val)))
		snprintf(st->font_name, sizeof(st->font_name), "%s", val);
	if (extract_xml_attr(p, end, "style", val, sizeof(val)))
		st->font_style = parse_font_style_attr(val);
	if (extract_xml_attr(p, end, "size", val, sizeof(val)) || extract_xml_attr(p, end, "scale", val, sizeof(val)))
	{
		float sc = (float)atof(val);
		if (sc >= 0.25f && sc <= 4.0f)
			st->font_scale = sc;
	}
	if (extract_xml_attr(p, end, "title_color", val, sizeof(val)))
		st->title_color = parse_hex_color_attr(val, st->title_color);
	if (extract_xml_attr(p, end, "text_color", val, sizeof(val)) || extract_xml_attr(p, end, "color", val, sizeof(val)))
		st->text_color = parse_hex_color_attr(val, st->text_color);
	if (extract_xml_attr(p, end, "selected_color", val, sizeof(val)))
		st->selected_color = parse_hex_color_attr(val, st->selected_color);
	if (extract_xml_attr(p, end, "bg_color", val, sizeof(val)))
		st->bg_color = parse_hex_color_attr(val, st->bg_color);
	if (extract_xml_attr(p, end, "header_color", val, sizeof(val)))
		st->header_color = parse_hex_color_attr(val, st->header_color);
	if (extract_xml_attr(p, end, "highlight_color", val, sizeof(val)))
		st->highlight_color = parse_hex_color_attr(val, st->highlight_color);
	if (extract_xml_attr(p, end, "border_color", val, sizeof(val)))
		st->border_color = parse_hex_color_attr(val, st->border_color);
	if (extract_xml_attr(p, end, "image_tint", val, sizeof(val)))
		st->image_tint = parse_hex_color_attr(val, st->image_tint);
	if (extract_xml_attr(p, end, "background", val, sizeof(val)))
		snprintf(st->bg_image, sizeof(st->bg_image), "%s", val);
	if (extract_xml_attr(p, end, "use_image", val, sizeof(val)))
		st->use_image = (_stricmp(val, "true") == 0 || strcmp(val, "1") == 0 || _stricmp(val, "yes") == 0) ? 1 : 0;
}

static void forge_ui_theme_load(int is_hot_reload)
{
	static char const *const area_nodes[NUMBER_OF_FORGE_UI_AREAS] = {
		"spawn_menu",
		"preview_panel",
		"properties_menu",
		"top_bar",
		"bottom_bar",
		"save_modal",
		"maps_modal",
		"toast_bar",
		"markers"
	};
	static char const *const area_default_pngs[NUMBER_OF_FORGE_UI_AREAS] = {
		"panel_spawn_menu.png",
		"panel_preview.png",
		"panel_modal.png",
		"panel_top_bar.png",
		"panel_bottom_bar.png",
		"escape_menu_background.png",
		"panel_modal.png",
		"panel_bottom_bar.png",
		"none"
	};
	FILE *f;
	int i;

	if (is_hot_reload)
		s_ui_theme.reload_rev++;

	for (i = 0; i < NUMBER_OF_FORGE_UI_AREAS; i++)
		init_default_area_style(&s_ui_theme.areas[i], area_default_pngs[i]);

	/* Specific default font/color tuning per area matching base game menu UI styling */
	snprintf(s_ui_theme.areas[_forge_ui_area_top_bar].font_name, sizeof(s_ui_theme.areas[_forge_ui_area_top_bar].font_name), "small");
	snprintf(s_ui_theme.areas[_forge_ui_area_bottom_bar].font_name, sizeof(s_ui_theme.areas[_forge_ui_area_bottom_bar].font_name), "small");
	snprintf(s_ui_theme.areas[_forge_ui_area_toast_bar].font_name, sizeof(s_ui_theme.areas[_forge_ui_area_toast_bar].font_name), "small");
	snprintf(s_ui_theme.areas[_forge_ui_area_markers].font_name, sizeof(s_ui_theme.areas[_forge_ui_area_markers].font_name), "small");
	s_ui_theme.areas[_forge_ui_area_toast_bar].text_color = 0xFFFFFFFFu;
	s_ui_theme.areas[_forge_ui_area_markers].title_color = 0xFFFFFFFFu;
	s_ui_theme.areas[_forge_ui_area_markers].text_color = 0xFF2896FFu;
	s_ui_theme.areas[_forge_ui_area_markers].selected_color = 0xFFFFFFFFu;

	f = fopen("Faber_data/ui/forge_ui.xml", "rb");
	if (f)
	{
		long sz;
		if (fseek(f, 0, SEEK_END) == 0 && (sz = ftell(f)) > 0 && sz < 262144 && fseek(f, 0, SEEK_SET) == 0)
		{
			char *xml_buf = (char *)malloc((size_t)sz + 1);
			if (xml_buf)
			{
				size_t rd = fread(xml_buf, 1, (size_t)sz, f);
				xml_buf[rd] = 0;
				for (i = 0; i < NUMBER_OF_FORGE_UI_AREAS; i++)
					parse_area_xml_node(xml_buf, area_nodes[i], &s_ui_theme.areas[i]);
				free(xml_buf);
			}
		}
		fclose(f);
	}

	for (i = 0; i < NUMBER_OF_FORGE_UI_AREAS; i++)
		sync_and_register_area_png(&s_ui_theme.areas[i], i);

	snprintf(s_ui_theme.registered_map, sizeof(s_ui_theme.registered_map), "%s",
		forge_get_active_map_name() ? forge_get_active_map_name() : "");
	s_ui_theme.initialized = 1;
}

static void forge_ui_theme_ensure_loaded(void)
{
	char const *cur_map = forge_get_active_map_name();
	if (!s_ui_theme.initialized)
	{
		forge_ui_theme_load(0);
	}
	else if (cur_map && strcmp(s_ui_theme.registered_map, cur_map) != 0)
	{
		/* Re-register PNG textures after map transitions because menu_tags_unloaded() clears menu art */
		int i;
		for (i = 0; i < NUMBER_OF_FORGE_UI_AREAS; i++)
			sync_and_register_area_png(&s_ui_theme.areas[i], i);
		snprintf(s_ui_theme.registered_map, sizeof(s_ui_theme.registered_map), "%s", cur_map);
	}
}

static void draw_ui_png_quad_uv(
	struct bitmap_data *bitmap,
	short x0, short y0,
	short x1, short y1,
	float u0, float v0,
	float u1, float v1,
	uint32_t tint_argb)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	int v;

	if (!bitmap || !bitmap->hardware_format || x1 <= x0 || y1 <= y0)
		return;

	for (v = 0; v < 4; v++)
	{
		real vx = (v % 3) ? (real)x1 : (real)x0;
		real vy = (v > 1) ? (real)y1 : (real)y0;
		if (s_hud_ui_scale != 1.0f)
		{
			vx = s_hud_ui_origin_x + (vx - s_hud_ui_origin_x) * s_hud_ui_scale;
			vy = s_hud_ui_origin_y + (vy - s_hud_ui_origin_y) * s_hud_ui_scale;
		}
		vertices[v].position.x = vx;
		vertices[v].position.y = vy;
		vertices[v].texture_coordinates.x = (v % 3) ? u1 : u0;
		vertices[v].texture_coordinates.y = (v > 1) ? v1 : v0;
		vertices[v].color = tint_argb;
	}

	memset(&parameters, 0, sizeof(parameters));
	parameters.meter_parameters = NULL;
	parameters.map_offset[0] = NULL;
	parameters.map_wrapped[0] = FALSE;
	parameters.map_wrapped[1] = FALSE;
	parameters.map_wrapped[2] = FALSE;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.point_sampled = FALSE;
	parameters.framebuffer_blend_function = 0;
	parameters.map[0] = bitmap;
	parameters.map[1] = NULL;
	parameters.map[2] = NULL;

	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

static void draw_ui_png_quad(struct bitmap_data *bitmap, short x0, short y0, short x1, short y1, uint32_t tint_argb)
{
	draw_ui_png_quad_uv(bitmap, x0, y0, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f, tint_argb);
}

/* ============================================================================
 * Base Game Menu Bitmap & Font Rendering Helpers
 * Draws authentic Halo CE / OpenCE menu tags (pausebox, objectives, option_bkds,
 * list_item_bkd, sel_list_item_bkd, text_button_background, arrow_sm_*, blue)
 * and ui\large_ui / ui\small_ui fonts directly in-game.
 * ============================================================================ */

static int draw_ce_menu_bitmap_uv(
	char const *tag_name,
	short frame_idx,
	short x0, short y0,
	short x1, short y1,
	float u0, float v0,
	float u1, float v1,
	uint32_t tint_argb)
{
	long tag_idx;
	struct bitmap_data *bmp;

	if (!tag_name || !tag_name[0] || x1 <= x0 || y1 <= y0)
		return 0;

	tag_idx = tag_loaded('bitm', tag_name);
	if (tag_idx == NONE)
		return 0;

	bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, frame_idx);
	if (!bmp && frame_idx != 0)
		bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, 0);
	if (!bmp || !bmp->hardware_format || bmp->width <= 0 || bmp->height <= 0)
		return 0;

	draw_ui_png_quad_uv(bmp, x0, y0, x1, y1, u0, v0, u1, v1, tint_argb);
	return 1;
}

static int draw_ce_menu_bitmap_cropped(
	char const *tag_name,
	short frame_idx,
	short x0, short y0,
	short x1, short y1,
	short crop_w, short crop_h,
	uint32_t tint_argb)
{
	long tag_idx;
	struct bitmap_data *bmp;
	float u1 = 1.0f, v1 = 1.0f;

	if (!tag_name || !tag_name[0] || x1 <= x0 || y1 <= y0)
		return 0;

	tag_idx = tag_loaded('bitm', tag_name);
	if (tag_idx == NONE)
		return 0;

	bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, frame_idx);
	if (!bmp && frame_idx != 0)
		bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, 0);
	if (!bmp || !bmp->hardware_format || bmp->width <= 0 || bmp->height <= 0)
		return 0;

	if (crop_w > 0 && crop_w < bmp->width)
		u1 = (float)crop_w / (float)bmp->width;
	if (crop_h > 0 && crop_h < bmp->height)
		v1 = (float)crop_h / (float)bmp->height;

	draw_ui_png_quad_uv(bmp, x0, y0, x1, y1, 0.0f, 0.0f, u1, v1, tint_argb);
	return 1;
}

/* Draws a 3-slice horizontally rounded Halo CE option/row highlight bar
 * using pc\bitmaps\option_bkds_sm so both left and right ends have crisp rounded caps. */
static int draw_ce_rounded_row_bar(
	short frame_idx,
	short x0, short y0,
	short x1, short y1)
{
	short w = (short)(x1 - x0);
	short cap = 16;
	float v1 = 28.0f / 32.0f;
	float u_cap = 16.0f / 256.0f;

	if (w <= 0 || y1 <= y0)
		return 0;
	if (w <= cap * 2)
		return draw_ce_menu_bitmap_cropped("pc\\bitmaps\\option_bkds_sm", frame_idx, x0, y0, x1, y1, 256, 28, 0xFFFFFFFFu);

	if (draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, x0, y0, (short)(x0 + cap), y1, 0.0f, 0.0f, u_cap, v1, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(x0 + cap), y0, (short)(x1 - cap), y1, u_cap, 0.0f, 1.0f - u_cap, v1, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(x1 - cap), y0, x1, y1, 1.0f - u_cap, 0.0f, 1.0f, v1, 0xFFFFFFFFu))
	{
		return 1;
	}
	return 0;
}

/* Draws the authentic 9-slice Halo CE pause/objectives container box with 1:1 rounded corners */
static void draw_ce_pause_box(short x0, short y0, short width, short height, int with_header)
{
	short x1 = (short)(x0 + width);
	short y1 = (short)(y0 + height);
	char const *top_left   = with_header ? "pc\\pause\\objectives_left"   : "pc\\pause\\pausebox_left";
	char const *top_center = with_header ? "pc\\pause\\objectives_center" : "pc\\pause\\pausebox_center";
	char const *top_right  = with_header ? "pc\\pause\\objectives_right"  : "pc\\pause\\pausebox_right";
	char const *obj_left   = "pc\\pause\\objectives_left";
	char const *obj_center = "pc\\pause\\objectives_center";
	char const *obj_right  = "pc\\pause\\objectives_right";
	short frame_idx = 1;
	short top_h = with_header ? 30 : 14;
	short bot_h = 14;
	float v_top_end = (float)top_h / 256.0f;
	float v_body_start = 32.0f / 256.0f;
	float v_body_end = 211.0f / 256.0f;
	float v_bot_end = 225.0f / 256.0f;

	if (width <= 32 || height <= 28)
		return;

	/* Top slice (rounded top corners; includes 29px dark header bar when with_header != 0,
	 * or clean blue rounded top cap from pausebox when with_header == 0) */
	if (draw_ce_menu_bitmap_uv(top_left, frame_idx, x0, y0, (short)(x0 + 16), (short)(y0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(top_center, frame_idx, (short)(x0 + 16), y0, (short)(x1 - 16), (short)(y0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(top_right, frame_idx, (short)(x1 - 16), y0, x1, (short)(y0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, 0xFFFFFFFFu) &&
		/* Middle body slice (clean translucent blue gradient from objectives_left/center/right) */
		draw_ce_menu_bitmap_uv(obj_left, frame_idx, x0, (short)(y0 + top_h), (short)(x0 + 16), (short)(y1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(x0 + 16), (short)(y0 + top_h), (short)(x1 - 16), (short)(y1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(x1 - 16), (short)(y0 + top_h), x1, (short)(y1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, 0xFFFFFFFFu) &&
		/* Bottom slice (1:1 unstretched rounded bottom corners from objectives_left/center/right, no footer line) */
		draw_ce_menu_bitmap_uv(obj_left, frame_idx, x0, (short)(y1 - bot_h), (short)(x0 + 16), y1, 0.0f, v_body_end, 1.0f, v_bot_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(x0 + 16), (short)(y1 - bot_h), (short)(x1 - 16), y1, 0.0f, v_body_end, 1.0f, v_bot_end, 0xFFFFFFFFu) &&
		draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(x1 - 16), (short)(y1 - bot_h), x1, y1, 0.0f, v_body_end, 1.0f, v_bot_end, 0xFFFFFFFFu))
	{
		return;
	}

	/* Fallback if menu tags are not present */
	{
		rectangle2d body, hdr;
		body.x0 = x0; body.y0 = y0; body.x1 = x1; body.y1 = y1;
		draw_quad(&body, 0xC00A2649u);
		if (with_header)
		{
			hdr.x0 = (short)(x0 + 2); hdr.y0 = (short)(y0 + 2); hdr.x1 = (short)(x1 - 2); hdr.y1 = (short)(y0 + 28);
			draw_quad(&hdr, 0xE0000000u);
			hdr.y0 = (short)(y0 + 28); hdr.y1 = (short)(y0 + 29);
			draw_quad(&hdr, 0xFF2896FFu);
		}
		draw_box_border(x0, y0, x1, y1, 2, 0xFF2896FFu);
	}
}

static void draw_ce_blue_line(short x0, short y0, short x1, short height)
{
	if (height <= 0)
		height = 1;
	if (!draw_ce_menu_bitmap_cropped("pc\\bitmaps\\blue", 0, x0, y0, x1, (short)(y0 + height), 4, 4, 0xFFFFFFFFu))
	{
		rectangle2d r;
		r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = (short)(y0 + height);
		draw_quad(&r, 0xFF2896FFu);
	}
}

static struct
{
	struct bitmap_data *bmp;
	long asset;
	int init;
} s_xbox_btn_cache[10];

static struct bitmap_data *get_xbox_hires_button_bmp(int slot, char const *asset_tag_name, short tex_size)
{
	long a, count;
	if (slot < 0 || slot >= 10)
		return NULL;
	if (!s_xbox_btn_cache[slot].init)
	{
		s_xbox_btn_cache[slot].init = 1;
		s_xbox_btn_cache[slot].asset = -1;
		count = hud_hires_asset_count();
		for (a = 0; a < count; a++)
		{
			char const *t = hud_hires_asset_tag(a);
			if (t && strcmp(t, asset_tag_name) == 0)
			{
				struct bitmap_data *b = bitmap_2d_new(tex_size, tex_size, 0, 11);
				if (b && rasterizer_bitmap_new(b))
				{
					s_xbox_btn_cache[slot].bmp = b;
					s_xbox_btn_cache[slot].asset = a;
				}
				else if (b)
				{
					bitmap_delete(b);
				}
				break;
			}
		}
	}
	if (s_xbox_btn_cache[slot].bmp && s_xbox_btn_cache[slot].asset >= 0)
	{
		/* (only if it is not registered: each registering makes every texture
		look for its placeholder again, which this did for each icon of each
		frame) */
		unsigned long const *texture = XBOX_POINTER(unsigned long const, s_xbox_btn_cache[slot].bmp->hardware_format); /* AE hook */

		if (hud_hires_placeholder_asset(texture[1]) != s_xbox_btn_cache[slot].asset)
			hud_hires_register_placeholder(s_xbox_btn_cache[slot].asset, texture);
		return s_xbox_btn_cache[slot].bmp;
	}
	return NULL;
}

static int parse_xbox_icon_token(char const *p, int *out_len, int *out_icon)
{
	static struct
	{
		char const *tok;
		int len;
		int icon;
	} const tbl[] = {
		{ "{A}",     3, FORGE_GP_A },
		{ "{B}",     3, FORGE_GP_B },
		{ "{X}",     3, FORGE_GP_X },
		{ "{Y}",     3, FORGE_GP_Y },
		{ "{BL}",    4, FORGE_GP_BLACK },
		{ "{RB}",    4, FORGE_GP_BLACK },
		{ "{WH}",    4, FORGE_GP_WHITE },
		{ "{LB}",    4, FORGE_GP_WHITE },
		{ "{LT}",    4, FORGE_GP_LTRIGGER },
		{ "{RT}",    4, FORGE_GP_RTRIGGER },
		{ "{DU}",    4, FORGE_GP_DPAD_UP },
		{ "{DD}",    4, FORGE_GP_DPAD_DOWN },
		{ "{DL}",    4, FORGE_GP_DPAD_LEFT },
		{ "{DR}",    4, FORGE_GP_DPAD_RIGHT },
		{ "{ST}",    4, FORGE_GP_START },
		{ "{START}", 7, FORGE_GP_START },
		{ "{BK}",    4, FORGE_GP_BACK },
		{ "{BACK}",  6, FORGE_GP_BACK },
		{ "{LS}",    4, FORGE_GP_LTHUMB },
		{ "{RS}",    4, FORGE_GP_RTHUMB },
		{ "{DPAD}",  6, 17 }
	};
	int i;
	if (!p || p[0] != '{')
		return 0;
	for (i = 0; i < (int)(sizeof(tbl) / sizeof(tbl[0])); i++)
	{
		if (strncmp(p, tbl[i].tok, (size_t)tbl[i].len) == 0)
		{
			if (out_len) *out_len = tbl[i].len;
			if (out_icon) *out_icon = tbl[i].icon;
			return 1;
		}
	}
	return 0;
}

static short xbox_icon_advance_px(int icon_id, int use_large_font)
{
	short base = use_large_font ? 20 : 17;
	if (icon_id == FORGE_GP_LTRIGGER || icon_id == FORGE_GP_RTRIGGER)
		return (short)(base + 5);
	if (icon_id == FORGE_GP_LTHUMB || icon_id == FORGE_GP_RTHUMB)
		return (short)(base + 2);
	return base;
}

/* the button as the player's controller shows it (Gamepads > Button
Appearance), the HUD's own artwork, fitted to the cell the Xbox icons have.
0: this style has none for it (the original Xbox styles, the D-pad, Start and
Back), and draw_xbox_button_icon draws its own */
static int draw_controller_glyph(int icon_id, short x0, short y0, short h)
{
	struct bitmap_data *bitmap;
	short bounds[4];
	short slot;
	float cell_w, cell_h, width, height, fit, x, y;

	if (icon_id >= FORGE_GP_A && icon_id <= FORGE_GP_WHITE)
	{
		slot = (short)(icon_id - FORGE_GP_A);
		cell_w = (float)h * (21.0f / 22.0f);
	}
	else if (icon_id == FORGE_GP_LTRIGGER || icon_id == FORGE_GP_RTRIGGER)
	{
		slot = (short)(icon_id == FORGE_GP_LTRIGGER ? 6 : 7);
		cell_w = (float)h * 1.25f;
	}
	else if (icon_id == FORGE_GP_LTHUMB || icon_id == FORGE_GP_RTHUMB)
	{
		slot = (short)(icon_id == FORGE_GP_LTHUMB ? 8 : 9);
		cell_w = (float)h * 1.05f;
	}
	else
	{
		return 0;
	}
	/* (the button that does it in a menu: forge_menu_buttons_swapped) */
	if (slot < 2 && forge_menu_buttons_swapped())
		slot = (short)(slot ^ 1);
	if (!controller_glyph_lookup(controller_glyph_style(), slot, &bitmap, bounds))
		return 0;
	cell_h = (float)h + 2.0f;
	width = (float)(bounds[2] - bounds[0]);
	height = (float)(bounds[3] - bounds[1]);
	if (width <= 0.0f || height <= 0.0f)
		return 0;
	fit = cell_w / width < cell_h / height ? cell_w / width : cell_h / height;
	x = (float)x0 + (cell_w - width * fit) * 0.5f - (float)bounds[0] * fit;
	y = (float)y0 - 1.0f + (cell_h - height * fit) * 0.5f - (float)bounds[1] * fit;
	draw_ui_png_quad_uv(bitmap, (short)(x + 0.5f), (short)(y + 0.5f),
		(short)(x + 512.0f * fit + 0.5f), (short)(y + 512.0f * fit + 0.5f),
		0.0f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFFu);
	return 1;
}

static void draw_xbox_button_icon(int icon_id, short x0, short y0, short height, float fit_scale)
{
	static char const *const face_assets[6] = {
		"ui\\hud\\buttons\\classic_a",
		"ui\\hud\\buttons\\classic_b",
		"ui\\hud\\buttons\\classic_x",
		"ui\\hud\\buttons\\classic_y",
		"ui\\hud\\buttons\\classic_black",
		"ui\\hud\\buttons\\classic_white"
	};
	short h = (short)((float)height * fit_scale + 0.5f);
	if (h < 10) h = 10;

	if (draw_controller_glyph(icon_id, x0, y0, h))
		return;
	if (icon_id >= FORGE_GP_A && icon_id <= FORGE_GP_WHITE)
	{
		int slot = icon_id - FORGE_GP_A;
		struct bitmap_data *bmp = get_xbox_hires_button_bmp(slot, face_assets[slot], 32);
		short w = (short)((float)h * (21.0f / 22.0f) + 0.5f);
		short iy0 = (short)(y0 - 1);
		if (bmp)
		{
			draw_ui_png_quad_uv(bmp, x0, iy0, (short)(x0 + w), (short)(iy0 + h + 2),
				0.0f, 0.0f, 21.0f / 32.0f, 26.0f / 32.0f, 0xFFFFFFFFu);
			return;
		}
	}
	else if (icon_id == FORGE_GP_LTRIGGER || icon_id == FORGE_GP_RTRIGGER)
	{
		int slot = (icon_id == FORGE_GP_LTRIGGER) ? 6 : 7;
		char const *tname = (icon_id == FORGE_GP_LTRIGGER)
			? "ui\\hud\\buttons\\trigger_left"
			: "ui\\hud\\buttons\\trigger_right";
		struct bitmap_data *bmp = get_xbox_hires_button_bmp(slot, tname, 64);
		short w = (short)((float)h * 1.25f + 0.5f);
		short iy0 = (short)(y0 - 1);
		if (bmp)
		{
			draw_ui_png_quad_uv(bmp, x0, iy0, (short)(x0 + w), (short)(iy0 + h + 2),
				0.0f, 0.0f, 50.0f / 64.0f, 35.0f / 64.0f, 0xFF66CCFFu);
			return;
		}
	}
	else if (icon_id == FORGE_GP_LTHUMB || icon_id == FORGE_GP_RTHUMB)
	{
		int slot = (icon_id == FORGE_GP_LTHUMB) ? 8 : 9;
		char const *tname = (icon_id == FORGE_GP_LTHUMB)
			? "ui\\hud\\buttons\\thumbstick_left"
			: "ui\\hud\\buttons\\thumbstick_right";
		struct bitmap_data *bmp = get_xbox_hires_button_bmp(slot, tname, 64);
		short w = (short)((float)h * 1.05f + 0.5f);
		short iy0 = (short)(y0 - 1);
		if (bmp)
		{
			draw_ui_png_quad_uv(bmp, x0, iy0, (short)(x0 + w), (short)(iy0 + h + 2),
				0.0f, 0.0f, 36.0f / 64.0f, 40.0f / 64.0f, 0xFF66CCFFu);
			return;
		}
	}
	else if ((icon_id >= FORGE_GP_DPAD_UP && icon_id <= FORGE_GP_DPAD_RIGHT) || icon_id == 17)
	{
		/* Authentic Original Xbox D-Pad cross with the active direction highlighted in bright white/cyan */
		short sz = (short)(h - 1);
		short arm = (short)(sz / 3);
		short cx, cy;
		rectangle2d r;
		if (arm < 3) arm = 3;
		sz = (short)(arm * 3);
		cx = (short)(x0 + 1);
		cy = (short)(y0 + 1);
		/* Outer cross shadow */
		r.x0 = (short)(cx + arm - 1); r.y0 = (short)(cy - 1); r.x1 = (short)(cx + arm * 2 + 1); r.y1 = (short)(cy + sz + 1);
		draw_quad(&r, 0xD0061224u);
		r.x0 = (short)(cx - 1); r.y0 = (short)(cy + arm - 1); r.x1 = (short)(cx + sz + 1); r.y1 = (short)(cy + arm * 2 + 1);
		draw_quad(&r, 0xD0061224u);
		/* Base cross in Xbox HUD cyan */
		r.x0 = (short)(cx + arm); r.y0 = cy; r.x1 = (short)(cx + arm * 2); r.y1 = (short)(cy + sz);
		draw_quad(&r, 0xFF1E6BB8u);
		r.x0 = cx; r.y0 = (short)(cy + arm); r.x1 = (short)(cx + sz); r.y1 = (short)(cy + arm * 2);
		draw_quad(&r, 0xFF1E6BB8u);
		/* Highlighted direction arm */
		if (icon_id == FORGE_GP_DPAD_UP)
		{
			r.x0 = (short)(cx + arm); r.y0 = cy; r.x1 = (short)(cx + arm * 2); r.y1 = (short)(cy + arm);
			draw_quad(&r, 0xFFFFFFFFu);
		}
		else if (icon_id == FORGE_GP_DPAD_DOWN)
		{
			r.x0 = (short)(cx + arm); r.y0 = (short)(cy + arm * 2); r.x1 = (short)(cx + arm * 2); r.y1 = (short)(cy + sz);
			draw_quad(&r, 0xFFFFFFFFu);
		}
		else if (icon_id == FORGE_GP_DPAD_LEFT)
		{
			r.x0 = cx; r.y0 = (short)(cy + arm); r.x1 = (short)(cx + arm); r.y1 = (short)(cy + arm * 2);
			draw_quad(&r, 0xFFFFFFFFu);
		}
		else if (icon_id == FORGE_GP_DPAD_RIGHT)
		{
			r.x0 = (short)(cx + arm * 2); r.y0 = (short)(cy + arm); r.x1 = (short)(cx + sz); r.y1 = (short)(cy + arm * 2);
			draw_quad(&r, 0xFFFFFFFFu);
		}
		else
		{
			r.x0 = (short)(cx + arm); r.y0 = (short)(cy + arm); r.x1 = (short)(cx + arm * 2); r.y1 = (short)(cy + arm * 2);
			draw_quad(&r, 0xFF66CCFFu);
		}
		return;
	}
	else if (icon_id == FORGE_GP_START || icon_id == FORGE_GP_BACK)
	{
		/* Original Xbox oval Start / Back pill button */
		short w = (short)(h + 2);
		short bh = (short)(h - 4);
		rectangle2d r;
		if (bh < 8) bh = 8;
		r.x0 = x0; r.y0 = (short)(y0 + 2); r.x1 = (short)(x0 + w); r.y1 = (short)(y0 + 2 + bh);
		draw_quad(&r, 0xE00A2240u);
		draw_box_border(r.x0, r.y0, r.x1, r.y1, 1, 0xFF66CCFFu);
		/* Inner arrow / pause bars */
		if (icon_id == FORGE_GP_START)
		{
			r.x0 = (short)(x0 + w / 2 - 2); r.y0 = (short)(y0 + 5); r.x1 = (short)(x0 + w / 2 + 3); r.y1 = (short)(y0 + bh);
			draw_quad(&r, 0xFFFFFFFFu);
		}
		else
		{
			r.x0 = (short)(x0 + 4); r.y0 = (short)(y0 + 5); r.x1 = (short)(x0 + w - 4); r.y1 = (short)(y0 + bh);
			draw_quad(&r, 0xFF66CCFFu);
		}
		return;
	}
}

static short measure_ce_menu_str_pixels(int use_large_font, long fallback_font, char const *ascii)
{
	long font_idx;
	struct font_header *fhdr;
	short total = 0;
	int i = 0;

	if (!ascii || !ascii[0])
		return 0;

	font_idx = use_large_font ? get_large_forge_font(fallback_font) : get_small_forge_font(fallback_font);
	fhdr = (font_idx != NONE) ? font_definition_get(font_idx) : NULL;

	while (ascii[i])
	{
		int tok_len = 0, icon_id = 0;
		if (ascii[i] == '{' && parse_xbox_icon_token(ascii + i, &tok_len, &icon_id))
		{
			total = (short)(total + xbox_icon_advance_px(icon_id, use_large_font));
			i += tok_len;
			continue;
		}
		if (fhdr)
		{
			unsigned char ch = (unsigned char)ascii[i];
			struct forge_font_char_metric *cm;
			if (ch == '|')
				ch = '/';
			cm = (struct forge_font_char_metric *)font_get_character_by_ascii_code(fhdr, (word)ch);
			if (cm)
				total = (short)(total + cm->character_width);
			else
				total = (short)(total + (use_large_font ? 8 : 6));
		}
		else
		{
			total = (short)(total + (use_large_font ? 8 : 6));
		}
		i++;
	}
	return total;
}

static int str_has_xbox_icon_tokens(char const *ascii)
{
	int i = 0;
	if (!ascii)
		return 0;
	while (ascii[i])
	{
		int tok_len = 0, icon_id = 0;
		if (ascii[i] == '{' && parse_xbox_icon_token(ascii + i, &tok_len, &icon_id))
			return 1;
		i++;
	}
	return 0;
}

static void draw_ce_menu_str_box(
	int use_large_font,
	long fallback_font,
	short x0, short y0,
	short x1, short y1,
	short justification,
	char const *ascii,
	uint32_t argb_col)
{
	real_argb_color col;
	long font_idx;
	struct font_header *fhdr;
	short box_w = (short)(x1 - x0);
	short total_w, scaled_total_w, cur_x;
	float fit_scale = 1.0f;
	short icon_h = use_large_font ? 18 : 15;
	int i = 0;

	if (!ascii || !ascii[0] || x1 <= x0 || y1 <= y0)
		return;

	total_w = measure_ce_menu_str_pixels(use_large_font, fallback_font, ascii);
	if (total_w <= 0)
		return;

	/* Keep font size 100% consistent across all buttons and UI rows; only scale down if a full-width
	 * HUD bar line exceeds the safezone width (box_w >= 480). */
	if (box_w >= 480 && total_w > box_w - 8)
	{
		fit_scale = (float)(box_w - 8) / (float)total_w;
		if (fit_scale < 0.65f)
			fit_scale = 0.65f;
	}

	col.alpha = (real)((argb_col >> 24) & 0xFFu) / 255.0f;
	col.red   = (real)((argb_col >> 16) & 0xFFu) / 255.0f;
	col.green = (real)((argb_col >> 8)  & 0xFFu) / 255.0f;
	col.blue  = (real)(argb_col         & 0xFFu) / 255.0f;

	font_idx = use_large_font ? get_large_forge_font(fallback_font) : get_small_forge_font(fallback_font);
	fhdr = (font_idx != NONE) ? font_definition_get(font_idx) : NULL;

	/* Fast path when no {ICON} tokens are present: use Halo's native text justification over wide symmetric bounds
	 * so centering, right-alignment, and left-alignment are pixel-exact and never wrap or vary in size. */
	if (!str_has_xbox_icon_tokens(ascii))
	{
		wchar_t wbuf[256];
		int wlen = 0;
		rectangle2d bounds;
		float S = s_hud_ui_scale;
		float eff_scale = S * fit_scale;
		float eff_ox = s_hud_ui_origin_x;
		float eff_oy = s_hud_ui_origin_y;
		float pivot_x = (justification == 2) ? ((float)(x0 + x1) * 0.5f) : ((justification == 1) ? (float)x1 : (float)x0);

		while (ascii[i] && wlen < 254)
		{
			unsigned char ch = (unsigned char)ascii[i++];
			if (ch == '|')
				ch = '/';
			wbuf[wlen++] = (wchar_t)ch;
		}
		wbuf[wlen] = 0;

		if (fit_scale != 1.0f && (float)fabs((double)(1.0f - eff_scale)) > 0.001f)
		{
			eff_ox = (s_hud_ui_origin_x * (1.0f - S) + pivot_x * S * (1.0f - fit_scale)) / (1.0f - eff_scale);
			eff_oy = (s_hud_ui_origin_y * (1.0f - S) + (float)y0 * S * (1.0f - fit_scale)) / (1.0f - eff_scale);
		}

		if (justification == 2 /* center */)
		{
			short mid_x = (short)((x0 + x1) / 2);
			bounds.x0 = (short)(mid_x - 1024);
			bounds.x1 = (short)(mid_x + 1024);
		}
		else if (justification == 1 /* right */)
		{
			bounds.x0 = (short)(x1 - 2048);
			bounds.x1 = x1;
		}
		else
		{
			bounds.x0 = x0;
			bounds.x1 = (short)(x0 + 2048);
		}
		bounds.y0 = y0;
		bounds.y1 = (short)(y0 + 128);

		if (eff_scale != 1.0f)
			rasterizer_text_set_scale(eff_scale, eff_ox, eff_oy);
		draw_string_set_draw_mode(font_idx, NONE, justification, 0, &col);
		rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wbuf);
		if (eff_scale != 1.0f)
			rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
		return;
	}

	scaled_total_w = (short)((float)total_w * fit_scale + 0.5f);
	if (justification == 2 /* center */)
		cur_x = (short)(x0 + (box_w - scaled_total_w) / 2);
	else if (justification == 1 /* right */)
		cur_x = (short)(x1 - scaled_total_w);
	else
		cur_x = x0;

	while (ascii[i])
	{
		int tok_len = 0, icon_id = 0;
		wchar_t wbuf[256];
		int wlen = 0;
		short run_unscaled_w = 0;

		if (ascii[i] == '{' && parse_xbox_icon_token(ascii + i, &tok_len, &icon_id))
		{
			short adv = (short)((float)xbox_icon_advance_px(icon_id, use_large_font) * fit_scale + 0.5f);
			draw_xbox_button_icon(icon_id, cur_x, (short)(y0 + 1), icon_h, fit_scale);
			cur_x = (short)(cur_x + adv);
			i += tok_len;
			continue;
		}

		while (ascii[i] && wlen < 254)
		{
			unsigned char ch;
			if (ascii[i] == '{' && parse_xbox_icon_token(ascii + i, &tok_len, &icon_id))
				break;
			ch = (unsigned char)ascii[i++];
			if (ch == '|')
				ch = '/';
			wbuf[wlen++] = (wchar_t)ch;
			if (fhdr)
			{
				struct forge_font_char_metric *cm =
					(struct forge_font_char_metric *)font_get_character_by_ascii_code(fhdr, (word)ch);
				run_unscaled_w = (short)(run_unscaled_w + (cm ? cm->character_width : (use_large_font ? 8 : 6)));
			}
			else
			{
				run_unscaled_w = (short)(run_unscaled_w + (use_large_font ? 8 : 6));
			}
		}
		wbuf[wlen] = 0;

		if (wlen > 0)
		{
			rectangle2d bounds;
			short lead = fhdr ? fhdr->leading_width : 0;
			float S = s_hud_ui_scale;
			float eff_scale = S * fit_scale;
			float eff_ox = s_hud_ui_origin_x;
			float eff_oy = s_hud_ui_origin_y;

			if (fit_scale != 1.0f && (float)fabs((double)(1.0f - eff_scale)) > 0.001f)
			{
				eff_ox = (s_hud_ui_origin_x * (1.0f - S) + (float)cur_x * S * (1.0f - fit_scale)) / (1.0f - eff_scale);
				eff_oy = (s_hud_ui_origin_y * (1.0f - S) + (float)y0 * S * (1.0f - fit_scale)) / (1.0f - eff_scale);
			}

			/* Offset x0 by -leading_width because draw_unicode_string_partial adds font->leading_width once at line start */
			bounds.x0 = (short)(cur_x - lead);
			bounds.y0 = y0;
			bounds.x1 = (short)(cur_x + run_unscaled_w + 1024);
			bounds.y1 = (short)(y0 + 128);

			if (eff_scale != 1.0f)
				rasterizer_text_set_scale(eff_scale, eff_ox, eff_oy);
			draw_string_set_draw_mode(font_idx, NONE, 0 /* left */, 0, &col);
			rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wbuf);
			if (eff_scale != 1.0f)
				rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);

			cur_x = (short)(cur_x + (short)((float)run_unscaled_w * fit_scale + 0.5f));
		}
	}
}

/* a line cut to a width, with an ellipsis: the menu's text is not clipped */
static void forge_text_fit(int use_large_font, long fallback_font, char *text, short width)
{
	size_t length = strlen(text);

	if (measure_ce_menu_str_pixels(use_large_font, fallback_font, text) <= width)
		return;
	/* (room for the dots in the text's own buffer) */
	length = length > 2 ? length - 2 : length;
	while (length > 1)
	{
		length--;
		while (length > 1 && text[length - 1] == ' ')
			length--;
		text[length] = '.';
		text[length + 1] = '.';
		text[length + 2] = 0;
		if (measure_ce_menu_str_pixels(use_large_font, fallback_font, text) <= width)
			return;
	}
}

static void draw_ce_menu_str(
	int use_large_font,
	long fallback_font,
	short x,
	short y,
	char const *ascii,
	uint32_t argb_col)
{
	draw_ce_menu_str_box(use_large_font, fallback_font, x, y, (short)(x + 2048), (short)(y + 128), 0, ascii, argb_col);
}

static void draw_themed_panel(enum forge_ui_area_id area_id, rectangle2d const *r, short header_height);

static void draw_sized_bottom_pill_bar(
	long fallback_font,
	short safe_x0,
	short safe_x1,
	short y_bottom,
	char const *line1,
	char const *line2,
	short frame_idx,
	enum forge_ui_area_id fallback_area,
	uint32_t text_col,
	short *out_top_y)
{
	short safe_w = (short)(safe_x1 - safe_x0);
	short w1 = measure_ce_menu_str_pixels(0, fallback_font, line1);
	short w2 = (line2 && line2[0]) ? measure_ce_menu_str_pixels(0, fallback_font, line2) : 0;
	short content_w = (w1 > w2) ? w1 : w2;
	short bar_w = (short)(content_w + 34);
	short bx0, bx1, by0, by1;

	if (bar_w < 140)
		bar_w = 140;
	if (bar_w > safe_w)
		bar_w = safe_w;

	bx0 = (short)(safe_x0 + (safe_w - bar_w) / 2);
	bx1 = (short)(bx0 + bar_w);

	if (line2 && line2[0])
	{
		short row1_y0 = (short)(y_bottom - 44);
		short row1_y1 = (short)(y_bottom - 22);
		short row2_y0 = (short)(y_bottom - 22);
		short row2_y1 = y_bottom;

		if (out_top_y)
			*out_top_y = row1_y0;
		set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)(safe_x0 + safe_x1) * 0.5f, (float)y_bottom);
		if (!draw_ce_rounded_row_bar(frame_idx, bx0, row1_y0, bx1, row1_y1))
		{
			rectangle2d r1 = { row1_y0, bx0, row1_y1, bx1 };
			draw_themed_panel(fallback_area, &r1, 0);
		}
		if (!draw_ce_rounded_row_bar(frame_idx, bx0, row2_y0, bx1, row2_y1))
		{
			rectangle2d r2 = { row2_y0, bx0, row2_y1, bx1 };
			draw_themed_panel(fallback_area, &r2, 0);
		}
		draw_ce_menu_str_box(0, fallback_font, bx0, (short)(row1_y0 + 2), bx1, row1_y1, 2 /* center */, line1, text_col);
		draw_ce_menu_str_box(0, fallback_font, bx0, (short)(row2_y0 + 2), bx1, row2_y1, 2 /* center */, line2, text_col);
	}
	else
	{
		by0 = (short)(y_bottom - 24);
		by1 = y_bottom;
		if (out_top_y)
			*out_top_y = by0;
		set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)(safe_x0 + safe_x1) * 0.5f, (float)y_bottom);
		if (!draw_ce_rounded_row_bar(frame_idx, bx0, by0, bx1, by1))
		{
			rectangle2d bbar = { by0, bx0, by1, bx1 };
			draw_themed_panel(fallback_area, &bbar, 0);
		}
		draw_ce_menu_str_box(0, fallback_font, bx0, (short)(by0 + 2), bx1, by1, 2 /* center */, line1, text_col);
	}
}

static void draw_ce_text_button(
	long fallback_font,
	short x0, short y0,
	short width, short height,
	int focused,
	char const *label)
{
	short x1 = (short)(x0 + width);
	short y1 = (short)(y0 + height);
	short text_y = (short)(y0 + (height - 18) / 2);
	uint32_t text_col = focused ? 0xFFFFFFFFu : 0xFF2896FFu;

	if (!draw_ce_menu_bitmap_cropped("pc\\bitmaps\\text_button_background", (short)(focused ? 1 : 0), x0, y0, x1, y1, 128, 21, 0xFFFFFFFFu))
	{
		rectangle2d r;
		r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
		draw_quad(&r, focused ? 0xD8143C70u : 0xB007162Cu);
		draw_box_border(x0, y0, x1, y1, 1, 0xFF2896FFu);
	}
	draw_ce_menu_str_box(0, fallback_font, x0, text_y, x1, (short)(y1 + 4), 2 /* center */, label, text_col);
}

static void draw_themed_panel(enum forge_ui_area_id area_id, rectangle2d const *r, short header_height)
{
	struct forge_ui_area_style *st = &s_ui_theme.areas[area_id];
	if (!r || r->x1 <= r->x0 || r->y1 <= r->y0)
		return;

	if (st->use_image && st->bitmap_valid && st->bitmap.hardware_format)
	{
		draw_ui_png_quad(&st->bitmap, r->x0, r->y0, r->x1, r->y1, st->image_tint);
	}
	else
	{
		draw_quad(r, st->bg_color);
	}

	if (header_height > 0 && ((st->header_color >> 24) & 0xFFu) != 0)
	{
		rectangle2d hdr;
		hdr.x0 = r->x0;
		hdr.y0 = r->y0;
		hdr.x1 = r->x1;
		hdr.y1 = (short)(r->y0 + header_height);
		draw_quad(&hdr, st->header_color);
	}

	if (((st->border_color >> 24) & 0xFFu) != 0)
	{
		draw_box_border(r->x0, r->y0, r->x1, r->y1, 1, st->border_color);
	}
}

static float snap_angle_rad(float angle_rad, enum forge_snap_angle_mode mode)
{
	float step_deg = 0.0f;
	float step_rad;
	if (mode == _forge_snap_15_deg) step_deg = 15.0f;
	else if (mode == _forge_snap_45_deg) step_deg = 45.0f;
	else if (mode == _forge_snap_90_deg) step_deg = 90.0f;
	else return angle_rad;

	step_rad = step_deg * (3.1415926535f / 180.0f);
	return (float)floor((double)(angle_rad / step_rad) + 0.5) * step_rad;
}

static struct object_datum *forge_valid_object_get(long object_index)
{
	if (object_index == NONE)
		return NULL;
	return (struct object_datum *)object_try_and_get_and_verify_type(object_index, (unsigned long)-1);
}

static void euler_to_forward_up(float yaw, float pitch, float roll, real_vector3d *forward, real_vector3d *up)
{
	float cy = (float)cos((double)yaw);
	float sy = (float)sin((double)yaw);
	float cp = (float)cos((double)pitch);
	float sp = (float)sin((double)pitch);
	float cr = (float)cos((double)roll);
	float sr = (float)sin((double)roll);

	forward->i = cy * cp;
	forward->j = sy * cp;
	forward->k = sp;

	up->i = -cy * sp * cr + sy * sr;
	up->j = -sy * sp * cr - cy * sr;
	up->k = cp * cr;
}

static void euler_to_basis_vectors(
	float yaw,
	float pitch,
	float roll,
	real_vector3d *forward,
	real_vector3d *left,
	real_vector3d *up)
{
	euler_to_forward_up(yaw, pitch, roll, forward, up);
	/* Right-handed basis: left = up x forward */
	left->i = up->j * forward->k - up->k * forward->j;
	left->j = up->k * forward->i - up->i * forward->k;
	left->k = up->i * forward->j - up->j * forward->i;
}

static void forge_eval_magnetic_snap_for_held_instance(
	int32_t held_live_idx,
	float *io_x,
	float *io_y,
	float *io_z,
	float *io_yaw,
	float pitch,
	float roll)
{
	struct forge_live_instance const *held;
	struct forge_tag_snap_config const *held_cfg;
	real_vector3d hfwd, hleft, hup;
	float best_dist = 1.25f; /* magnetic capture radius in world units */
	float best_tw_x = 0.0f, best_tw_y = 0.0f, best_tw_z = 0.0f;
	float best_tn_x = 0.0f, best_tn_y = 0.0f, best_tn_z = 0.0f;
	int best_hp_idx = -1;
	int best_tp_idx = -1;
	int i, h, t;

	g_forge.snap_point_locked = 0;
	g_forge.snap_lock_label[0] = '\0';

	if (held_live_idx < 0 || held_live_idx >= g_forge.live_instance_count || !io_x || !io_y || !io_z || !io_yaw)
		return;

	/* Holding Alt or Ctrl temporarily suppresses magnetic snapping for free placement */
	if ((GetAsyncKeyState(0x12) & 0x8000) ||
		(GetAsyncKeyState(0xA4) & 0x8000) ||
		(GetAsyncKeyState(0x11) & 0x8000))
	{
		return;
	}

	held = &g_forge.live_instances[held_live_idx];
	if (!held->active || !held->tag_path[0])
		return;

	held_cfg = forge_snap_points_get_for_tag(held->group_tag, held->tag_path, 0);
	if (!held_cfg || held_cfg->point_count <= 0)
		return;

	euler_to_basis_vectors(*io_yaw, pitch, roll, &hfwd, &hleft, &hup);

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *target = &g_forge.live_instances[i];
		struct forge_tag_snap_config const *target_cfg;
		real_vector3d tfwd, tleft, tup;
		float dx, dy, dz;

		if (i == held_live_idx || !target->active || !target->tag_path[0])
			continue;

		dx = target->x - *io_x;
		dy = target->y - *io_y;
		dz = target->z - *io_z;
		if (dx * dx + dy * dy + dz * dz > 35.0f * 35.0f)
			continue;

		target_cfg = forge_snap_points_get_for_tag(target->group_tag, target->tag_path, 0);
		if (!target_cfg || target_cfg->point_count <= 0)
			continue;

		euler_to_basis_vectors(target->yaw, target->pitch, target->roll, &tfwd, &tleft, &tup);

		for (t = 0; t < target_cfg->point_count; t++)
		{
			struct forge_snap_point const *tp = &target_cfg->points[t];
			float tw_x, tw_y, tw_z;
			float tn_x, tn_y, tn_z;

			tw_x = target->x + tp->x * tfwd.i + tp->y * tleft.i + tp->z * tup.i;
			tw_y = target->y + tp->x * tfwd.j + tp->y * tleft.j + tp->z * tup.j;
			tw_z = target->z + tp->x * tfwd.k + tp->y * tleft.k + tp->z * tup.k;

			tn_x = tp->nx * tfwd.i + tp->ny * tleft.i + tp->nz * tup.i;
			tn_y = tp->nx * tfwd.j + tp->ny * tleft.j + tp->nz * tup.j;
			tn_z = tp->nx * tfwd.k + tp->ny * tleft.k + tp->nz * tup.k;

			for (h = 0; h < held_cfg->point_count; h++)
			{
				struct forge_snap_point const *hp = &held_cfg->points[h];
				float hw_x, hw_y, hw_z;
				float hn_x, hn_y, hn_z;
				float pdx, pdy, pdz, dist, ndot;

				hw_x = *io_x + hp->x * hfwd.i + hp->y * hleft.i + hp->z * hup.i;
				hw_y = *io_y + hp->x * hfwd.j + hp->y * hleft.j + hp->z * hup.j;
				hw_z = *io_z + hp->x * hfwd.k + hp->y * hleft.k + hp->z * hup.k;

				pdx = hw_x - tw_x;
				pdy = hw_y - tw_y;
				pdz = hw_z - tw_z;
				dist = (float)sqrt((double)(pdx * pdx + pdy * pdy + pdz * pdz));
				if (dist >= best_dist)
					continue;

				hn_x = hp->nx * hfwd.i + hp->ny * hleft.i + hp->nz * hup.i;
				hn_y = hp->nx * hfwd.j + hp->ny * hleft.j + hp->nz * hup.j;
				hn_z = hp->nx * hfwd.k + hp->ny * hleft.k + hp->nz * hup.k;

				ndot = hn_x * tn_x + hn_y * tn_y + hn_z * tn_z;
				/* Require opposing or perpendicular normals (reject two faces pointing the same way) */
				if (ndot > 0.35f)
					continue;

				best_dist = dist;
				best_tw_x = tw_x;
				best_tw_y = tw_y;
				best_tw_z = tw_z;
				best_tn_x = tn_x;
				best_tn_y = tn_y;
				best_tn_z = tn_z;
				best_hp_idx = h;
				best_tp_idx = t;
			}
		}
	}

	if (best_hp_idx >= 0)
	{
		struct forge_snap_point const *hp = &held_cfg->points[best_hp_idx];
		float hn_x = hp->nx * hfwd.i + hp->ny * hleft.i + hp->nz * hup.i;
		float hn_y = hp->nx * hfwd.j + hp->ny * hleft.j + hp->nz * hup.j;
		float hn_z = hp->nx * hfwd.k + hp->ny * hleft.k + hp->nz * hup.k;
		float ho_x, ho_y, ho_z;

		/* If both normals are mostly horizontal and within ~38 deg of opposing, align yaw flush */
		if ((float)fabs((double)hn_z) < 0.5f && (float)fabs((double)best_tn_z) < 0.5f)
		{
			float h_len = (float)sqrt((double)(hn_x * hn_x + hn_y * hn_y));
			float t_len = (float)sqrt((double)(best_tn_x * best_tn_x + best_tn_y * best_tn_y));
			if (h_len > 0.4f && t_len > 0.4f)
			{
				float hnx = hn_x / h_len, hny = hn_y / h_len;
				float tnx = best_tn_x / t_len, tny = best_tn_y / t_len;
				float hdot = hnx * tnx + hny * tny;
				if (hdot <= -0.78f)
				{
					float cur_ang = (float)atan2((double)hny, (double)hnx);
					float des_ang = (float)atan2((double)(-tny), (double)(-tnx));
					*io_yaw += (des_ang - cur_ang);
					euler_to_basis_vectors(*io_yaw, pitch, roll, &hfwd, &hleft, &hup);
				}
			}
		}

		ho_x = hp->x * hfwd.i + hp->y * hleft.i + hp->z * hup.i;
		ho_y = hp->x * hfwd.j + hp->y * hleft.j + hp->z * hup.j;
		ho_z = hp->x * hfwd.k + hp->y * hleft.k + hp->z * hup.k;

		*io_x = best_tw_x - ho_x;
		*io_y = best_tw_y - ho_y;
		*io_z = best_tw_z - ho_z;

		g_forge.snap_point_locked = 1;
		g_forge.snap_lock_world_x = best_tw_x;
		g_forge.snap_lock_world_y = best_tw_y;
		g_forge.snap_lock_world_z = best_tw_z;
		snprintf(
			g_forge.snap_lock_label,
			sizeof(g_forge.snap_lock_label),
			"SNAP LOCKED [PT #%d <-> PT #%d]",
			best_hp_idx + 1,
			best_tp_idx + 1);
	}
}

static void forward_up_to_euler(
	real_vector3d const *forward,
	real_vector3d const *up,
	float *out_yaw,
	float *out_pitch,
	float *out_roll)
{
	float fk = forward->k;
	float yaw, pitch, roll, cy, sy, cp, sp;

	if (fk > 1.0f) fk = 1.0f;
	if (fk < -1.0f) fk = -1.0f;
	pitch = (float)asin((double)fk);
	if (fk > 0.9995f)
	{
		yaw = (float)atan2((double)(-up->j), (double)(-up->i));
		roll = 0.0f;
	}
	else if (fk < -0.9995f)
	{
		yaw = (float)atan2((double)up->j, (double)up->i);
		roll = 0.0f;
	}
	else
	{
		yaw = (float)atan2((double)forward->j, (double)forward->i);
		cy = (float)cos((double)yaw);
		sy = (float)sin((double)yaw);
		cp = (float)cos((double)pitch);
		sp = (float)sin((double)pitch);
		roll = (float)atan2(
			(double)(sy * up->i - cy * up->j),
			(double)(-cy * sp * up->i - sy * sp * up->j + cp * up->k));
	}

	*out_yaw = yaw;
	*out_pitch = pitch;
	*out_roll = roll;
}

static int count_active_live_instances(void)
{
	int i, active_count = 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (g_forge.live_instances[i].active)
			active_count++;
	}
	return active_count;
}

static void forge_ensure_default_object_edit_entry(struct forge_live_instance *inst)
{
	int32_t i;

	if (!inst || !inst->is_default_scenario_object)
		return;
	if (inst->edit_entry_index >= 0 && inst->edit_entry_index < (int32_t)g_forge.edit_state.default_edit_count)
		return;

	for (i = 0; i < (int32_t)g_forge.edit_state.default_edit_count; i++)
	{
		struct forge_default_object_edit *de = &g_forge.edit_state.default_edits[i];
		float dx, dy, dz, odx, ody, odz;
		if (!de->active || de->deleted || de->group_tag != inst->group_tag)
			continue;
		dx = de->new_x - inst->x;
		dy = de->new_y - inst->y;
		dz = de->new_z - inst->z;
		odx = de->orig_x - inst->orig_x;
		ody = de->orig_y - inst->orig_y;
		odz = de->orig_z - inst->orig_z;
		if (((dx * dx + dy * dy < 9.0f && (float)fabs((double)dz) < 5.0f) ||
			 (odx * odx + ody * ody < 9.0f && (float)fabs((double)odz) < 5.0f)) &&
			_stricmp(de->tag_path, inst->tag_path) == 0)
		{
			inst->edit_entry_index = i;
			inst->orig_x = de->orig_x;
			inst->orig_y = de->orig_y;
			inst->orig_z = de->orig_z;
			return;
		}
	}

	if (g_forge.edit_state.default_edit_count < FORGE_MAX_DEFAULT_OBJECT_EDITS)
	{
		struct forge_default_object_edit *de =
			&g_forge.edit_state.default_edits[g_forge.edit_state.default_edit_count];
		inst->edit_entry_index = g_forge.edit_state.default_edit_count++;
		memset(de, 0, sizeof(*de));
		de->active = 1;
		de->deleted = 0;
		de->group_tag = inst->group_tag;
		snprintf(de->tag_path, sizeof(de->tag_path), "%s", inst->tag_path);
		de->orig_x = inst->orig_x;
		de->orig_y = inst->orig_y;
		de->orig_z = inst->orig_z;
		de->new_x = inst->x;
		de->new_y = inst->y;
		de->new_z = inst->z;
		de->new_yaw = inst->yaw;
		de->new_pitch = inst->pitch;
		de->new_roll = inst->roll;
		de->gametype_flags = inst->gametype_flags;
	}
	else
		forge_set_status("Map budget reached: changes past %d of the map's own objects won't be saved",
			FORGE_MAX_DEFAULT_OBJECT_EDITS);
}

static int32_t adopt_default_scenario_object(long object_index)
{
	struct object_datum *obj;
	struct unit_datum *unit;
	long def_idx;
	uint32_t grp;
	char const *tname;
	int32_t i, live_idx;
	struct forge_live_instance *inst;

	obj = forge_valid_object_get(object_index);
	if (!obj)
		return -1;

	/* Never adopt a player's controlled biped */
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	if (unit && unit->unit.player_index != NONE)
		return -1;

	/* Check if already in live_instances */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (g_forge.live_instances[i].active &&
			g_forge.live_instances[i].object_index == object_index)
		{
			/* (an AI spawn's model is its marker, with its own entry) */
			if (g_forge.live_instances[i].group_tag != FORGE_GROUP_AI_SPAWN)
				forge_ensure_default_object_edit_entry(&g_forge.live_instances[i]);
			return i;
		}
	}
	/* Nor an AI's: where it starts is its spawn marker's, and it would be back
	there next game. (A client's units have no actors, so any biped) */
	if (unit && forge_ai_map_editable())
		return -1;

	if (g_forge.live_instance_count >= FORGE_MAX_LIVE_INSTANCES)
		return -1;

	def_idx = obj->definition_index;
	grp = (def_idx != NONE) ? (uint32_t)tag_get_group_tag(def_idx) : 'scen';
	tname = (def_idx != NONE) ? tag_get_name(def_idx) : "scenario_object";
	if (!tname || !tname[0])
		tname = "scenario_object";

	if (obj->object.parent_object_index != NONE)
		object_detach(object_index);
	SET_FLAG(obj->object.flags, _object_cannot_be_garbage_bit, TRUE);
	SET_FLAG(obj->object.flags, _object_deleted_when_deactivated_bit, FALSE);
	object_set_automatic_deactivation(object_index, FALSE);

	live_idx = g_forge.live_instance_count++;
	inst = &g_forge.live_instances[live_idx];
	memset(inst, 0, sizeof(*inst));
	inst->active = 1;
	inst->object_index = object_index;
	inst->actor_index = NONE;
	inst->edit_entry_index = -1;
	inst->group_tag = grp;
	inst->source_cache_id = 0;
	inst->is_default_scenario_object = 1;
	inst->orig_x = obj->object.position.x;
	inst->orig_y = obj->object.position.y;
	inst->orig_z = obj->object.position.z;
	inst->x = obj->object.position.x;
	inst->y = obj->object.position.y;
	inst->z = obj->object.position.z;
	forward_up_to_euler(&obj->object.forward, &obj->object.up, &inst->yaw, &inst->pitch, &inst->roll);
	snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tname);

	forge_ensure_default_object_edit_entry(inst);

	return live_idx;
}

int32_t forge_3d_adopt_or_find_object_instance(long object_index)
{
	return adopt_default_scenario_object(object_index);
}

static uint8_t *s_live_sp_buf = NULL;
static struct scenario *s_orig_sp_scen = NULL;
static void *s_orig_sp_addr = NULL;
static long s_orig_sp_count = 0;

static uint8_t *s_live_nf_buf = NULL;
static struct scenario *s_orig_nf_scen = NULL;
static uint8_t *s_orig_nf_addr = NULL;
static int s_orig_nf_count = 0;

/* a saved map's spawns and flags were put in the level loading (forge_3d_saved_markers_apply) and the Forge session has not adopted them yet:
its adoption keeps them (forge_init_map_scenario_entities gives the map's own back only when they are not this level's) */
static int s_saved_markers_fresh = 0;

void forge_3d_saved_markers_forget(void)
{
	s_saved_markers_fresh = 0;
}

/* Syncs all active 'spwn' live_instances into the in-memory scenario->players
 * AND into g_forge.edit_state.player_spawns so Red/Blue/FFA player spawns work live and save cleanly. */
static void sync_live_player_spawns_to_scenario(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	int i, out_count = 0;

	g_forge.edit_state.replace_player_spawns = 1;
	for (i = 0; i < g_forge.live_instance_count && out_count < FORGE_MAX_PLAYER_SPAWNS; i++)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[i];
		struct forge_player_spawn_entry *sp;
		int16_t mode_type = 12; /* _game_engine_all */

		if (!inst->active || inst->group_tag != 'spwn')
			continue;

		if (inst->gametype_flags == 0x0001u) mode_type = 1;      /* CTF Only */
		else if (inst->gametype_flags == 0x0002u) mode_type = 2; /* Slayer Only */
		else if (inst->gametype_flags == 0x0004u) mode_type = 3; /* Oddball Only */
		else if (inst->gametype_flags == 0x0008u) mode_type = 4; /* KOTH Only */
		else if (inst->gametype_flags == 0x0010u) mode_type = 5; /* Race Only */

		sp = &g_forge.edit_state.player_spawns[out_count];
		memset(sp, 0, sizeof(*sp));
		sp->x = inst->x;
		sp->y = inst->y;
		sp->z = inst->z;
		sp->yaw = inst->yaw;
		sp->bsp_index = (int16_t)(inst->pad_inst & 0x7Fu);

		if (inst->team_designator == 2 || inst->team_designator < 0 || inst->team_designator > 1)
		{
			/* FFA Spawn (Yellow) */
			inst->team_designator = 2;
			sp->team_index = 0;
			sp->type0 = 13; /* _game_engine_all_non_team (FFA Slayer/Oddball/KOTH/Race) */
			sp->type1 = mode_type;
			snprintf(inst->tag_path, sizeof(inst->tag_path), "spawn\\player_ffa");
		}
		else if (inst->team_designator == 1)
		{
			/* Blue Team Spawn (Blue) */
			sp->team_index = 1;
			sp->type0 = (mode_type != 12) ? mode_type :
				((inst->netgame_flag_type > 0 && inst->netgame_flag_type != 13) ? inst->netgame_flag_type : 12);
			sp->type1 = 12;
			snprintf(inst->tag_path, sizeof(inst->tag_path), "spawn\\player_blue");
		}
		else
		{
			/* Red Team Spawn (Red) */
			inst->team_designator = 0;
			sp->team_index = 0;
			sp->type0 = (mode_type != 12) ? mode_type :
				((inst->netgame_flag_type > 0 && inst->netgame_flag_type != 13) ? inst->netgame_flag_type : 12);
			sp->type1 = 12;
			snprintf(inst->tag_path, sizeof(inst->tag_path), "spawn\\player_red");
		}

		inst->edit_entry_index = out_count;
		out_count++;
	}
	g_forge.edit_state.player_spawn_count = out_count;

	if (scen && out_count <= FORGE_MAX_PLAYER_SPAWNS)
	{
		if (!s_live_sp_buf)
		{
			s_live_sp_buf = (uint8_t *)calloc(FORGE_MAX_PLAYER_SPAWNS, 52u);
		}
		if (scen->players.address && scen->players.address != XBOX_ADDRESS(s_live_sp_buf)) /* AE hook */
		{
			s_orig_sp_scen = scen;
			s_orig_sp_addr = XBOX_POINTER(void, scen->players.address); /* AE hook */
			s_orig_sp_count = scen->players.count;
		}
		if (s_live_sp_buf)
		{
			memset(s_live_sp_buf, 0, (size_t)out_count * 52u);
			for (i = 0; i < out_count; i++)
			{
				struct forge_player_spawn_entry const *sp = &g_forge.edit_state.player_spawns[i];
				uint8_t *dst = s_live_sp_buf + (size_t)i * 52u;
				*(float *)(dst + 0x00) = sp->x;
				*(float *)(dst + 0x04) = sp->y;
				*(float *)(dst + 0x08) = sp->z;
				*(float *)(dst + 0x0C) = sp->yaw;
				*(int16_t *)(dst + 0x10) = sp->team_index;
				*(int16_t *)(dst + 0x12) = sp->bsp_index;
				*(int16_t *)(dst + 0x14) = sp->type0;
				*(int16_t *)(dst + 0x16) = sp->type1;
				*(int16_t *)(dst + 0x18) = sp->type2;
				*(int16_t *)(dst + 0x1A) = sp->type3;
			}
			scen->players.address = XBOX_ADDRESS(s_live_sp_buf); /* AE hook */
			scen->players.count = out_count;
		}
	}
}

/* a saved map's spawns and flags in the level loading's scenario, as it is
playing them, for every machine and any game: the map's own go. Forge's
own scan of the scenario then finds these, and its sync keeps them */
void forge_3d_saved_markers_apply(
	struct scenario *scen,
	struct forge_player_spawn_entry const *spawns,
	int spawn_count,
	int replace_spawns,
	struct forge_netgame_flag_entry const *flags,
	int flag_count,
	int replace_flags)
{
	int i;

	if (!scen)
		return;
	if (replace_spawns && spawn_count <= FORGE_MAX_PLAYER_SPAWNS)
	{
		if (!s_live_sp_buf)
			s_live_sp_buf = (uint8_t *)calloc(FORGE_MAX_PLAYER_SPAWNS, 52u);
		if (s_live_sp_buf)
		{
			s_orig_sp_scen = scen;
			s_orig_sp_addr = XBOX_POINTER(void, scen->players.address); /* AE hook */
			s_orig_sp_count = scen->players.count;
			memset(s_live_sp_buf, 0, (size_t)FORGE_MAX_PLAYER_SPAWNS * 52u);
			for (i = 0; i < spawn_count; i++)
			{
				uint8_t *dst = s_live_sp_buf + (size_t)i * 52u;

				*(float *)(dst + 0x00) = spawns[i].x;
				*(float *)(dst + 0x04) = spawns[i].y;
				*(float *)(dst + 0x08) = spawns[i].z;
				*(float *)(dst + 0x0C) = spawns[i].yaw;
				*(int16_t *)(dst + 0x10) = spawns[i].team_index;
				*(int16_t *)(dst + 0x12) = spawns[i].bsp_index;
				*(int16_t *)(dst + 0x14) = spawns[i].type0;
				*(int16_t *)(dst + 0x16) = spawns[i].type1;
				*(int16_t *)(dst + 0x18) = spawns[i].type2;
				*(int16_t *)(dst + 0x1A) = spawns[i].type3;
			}
			scen->players.address = XBOX_ADDRESS(s_live_sp_buf); /* AE hook */
			scen->players.count = spawn_count;
		}
	}
	if (replace_flags && flag_count <= FORGE_MAX_NETGAME_FLAGS)
	{
		if (!s_live_nf_buf)
			s_live_nf_buf = (uint8_t *)calloc(FORGE_MAX_NETGAME_FLAGS, 148u);
		if (s_live_nf_buf)
		{
			s_orig_nf_scen = scen;
			s_orig_nf_addr = XBOX_POINTER(uint8_t, scen->netgame_flags.address); /* AE hook */
			s_orig_nf_count = scen->netgame_flags.count;
			memset(s_live_nf_buf, 0, (size_t)FORGE_MAX_NETGAME_FLAGS * 148u);
			for (i = 0; i < flag_count; i++)
			{
				uint8_t *dst = s_live_nf_buf + (size_t)i * 148u;

				*(float *)(dst + 0x00) = flags[i].x;
				*(float *)(dst + 0x04) = flags[i].y;
				*(float *)(dst + 0x08) = flags[i].z;
				*(float *)(dst + 0x0C) = flags[i].yaw;
				*(int16_t *)(dst + 0x10) = flags[i].type;
				*(int16_t *)(dst + 0x12) = flags[i].usage_id;
				*(int32_t *)(dst + 0x1C) = NONE; /* weapon_group.index = NONE */
			}
			scen->netgame_flags.address = XBOX_ADDRESS(s_live_nf_buf); /* AE hook */
			scen->netgame_flags.count = flag_count;
		}
	}
	s_saved_markers_fresh = replace_spawns || replace_flags;
}

/* a scenario object a saved map moved: its rotation, as its position is set */
void forge_3d_saved_object_place(long object_index, float x, float y, float z, float yaw, float pitch, float roll)
{
	real_point3d pos;
	real_vector3d fwd, up;

	pos.x = x;
	pos.y = y;
	pos.z = z;
	euler_to_forward_up(yaw, pitch, roll, &fwd, &up);
	object_set_position(object_index, &pos, &fwd, &up);
	object_attached_lights_moved(object_index);
}

static char const *forge_ff_spawn_type_label(int type)
{
	switch (type)
	{
	case _forge_ff_spawn_ai_infantry_only:   return "INFANTRY AI";
	case _forge_ff_spawn_ai_and_vehicles:    return "INFANTRY + VEHICLES";
	case _forge_ff_spawn_enemy_vehicle_only: return "VEHICLE AI: ANY";
	case _forge_ff_spawn_enemy_ghost_only:   return "VEHICLE AI: LIGHT";
	case _forge_ff_spawn_enemy_heavy_veh:    return "VEHICLE AI: HEAVY";
	case _forge_ff_spawn_player_spawn:       return "PLAYER SPAWN";
	case FF_DROP_SPAWN_TYPE_ANY:
	case FF_DROP_SPAWN_TYPE_COVENANT:
	case FF_DROP_SPAWN_TYPE_HUMAN:
	case FF_DROP_SPAWN_TYPE_FLOOD:
	case FF_DROP_SPAWN_TYPE_COVENANT_HUMAN:
	case FF_DROP_SPAWN_TYPE_COVENANT_FLOOD:
	case FF_DROP_SPAWN_TYPE_HUMAN_FLOOD:     return "ENEMY DROP POINT";
	case _forge_ff_spawn_player_vehicle:     return "PLAYER VEHICLE";
	case FF_ROUTE_SPAWN_TYPE:                return "DROPSHIP ROUTE";
	default:                                 return "AI INFANTRY";
	}
}

/* the kinds of Firefight marker FIREFIGHT SPAWNS places, each a row of its
own: a marker's type is one of its kind's (vehicle AI's light or heavy) */
enum forge_ff_spawn_kind
{
	_forge_ff_kind_player_spawn = 0,
	_forge_ff_kind_player_vehicle,
	_forge_ff_kind_infantry,
	_forge_ff_kind_vehicle,
	_forge_ff_kind_enemy_drop,
	_forge_ff_kind_route
};

static int forge_ff_spawn_kind(int type)
{
	switch (type)
	{
	case _forge_ff_spawn_player_spawn:       return _forge_ff_kind_player_spawn;
	case _forge_ff_spawn_player_vehicle:     return _forge_ff_kind_player_vehicle;
	case _forge_ff_spawn_enemy_vehicle_only:
	case _forge_ff_spawn_enemy_ghost_only:
	case _forge_ff_spawn_enemy_heavy_veh:    return _forge_ff_kind_vehicle;
	case FF_ROUTE_SPAWN_TYPE:                return _forge_ff_kind_route;
	default:
		return ff_drop_point_kinds(type) != 0 ? _forge_ff_kind_enemy_drop : _forge_ff_kind_infantry;
	}
}

/* the panel's title for a marker: its kind's */
static char const *forge_ff_spawn_kind_title(int type)
{
	switch (forge_ff_spawn_kind(type))
	{
	case _forge_ff_kind_player_spawn:   return "FIREFIGHT PLAYER SPAWN";
	case _forge_ff_kind_player_vehicle: return "FIREFIGHT PLAYER VEHICLE";
	case _forge_ff_kind_vehicle:        return "FIREFIGHT VEHICLE AI";
	case _forge_ff_kind_enemy_drop:     return "ENEMY DROP POINT";
	case _forge_ff_kind_route:          return "DROPSHIP ROUTE";
	default:                            return "FIREFIGHT INFANTRY AI";
	}
}

static char const *forge_ff_round_filter_label(int rnd)
{
	switch (rnd)
	{
	case _forge_ff_round_all:       return "ALL ROUNDS";
	case _forge_ff_round_1_only:    return "ROUND 1";
	case _forge_ff_round_2_only:    return "ROUND 2";
	case _forge_ff_round_3_only:    return "ROUND 3";
	case _forge_ff_round_4_only:    return "ROUND 4";
	case _forge_ff_round_5_only:    return "ROUND 5";
	case _forge_ff_round_early_1_3: return "ROUNDS 1-3";
	case _forge_ff_round_late_4_5:  return "ROUNDS 4-5";
	case _forge_ff_round_bonus:     return "BONUS ROUND";
	default:                        return "ALL ROUNDS";
	}
}

static char const *forge_ff_wave_filter_label(int wav)
{
	switch (wav)
	{
	case _forge_ff_wave_all:    return "ALL WAVES";
	case _forge_ff_wave_1_only: return "WAVE 1";
	case _forge_ff_wave_2_only: return "WAVE 2";
	case _forge_ff_wave_3_boss: return "WAVE 3 (BOSS)";
	default:                    return "ALL WAVES";
	}
}

/* ---------- Teleporters: a 'tele' node (a marker) or a teleporter's scenery
(forge_teleporter_scenery_tag), which carries its netgame flag with it. Each
sends (a teleporter_source flag) or receives (a teleporter_target), or, a
scenery only, is OFF: just a model. A sender takes a player to the first
receiver of its channel (game_engine.c) */

static void sync_live_netgame_flags_to_scenario(void);

/* how far above a teleporter scenery its flag stands: a map's own pair's
(forge_tele_absorb_node), else about its pad's top */
static float s_tele_default_dz = 0.1f;

static int forge_tele_is(struct forge_live_instance const *inst)
{
	return inst->active &&
		(inst->group_tag == 'tele' || forge_teleporter_scenery_tag(inst->group_tag, inst->tag_path));
}

/* 1 sender, 2 receiver, 0 off */
static int forge_tele_role(struct forge_live_instance const *inst)
{
	if (inst->group_tag == 'tele')
		return inst->netgame_flag_type == _netgame_flag_teleporter_target ? 2 : 1;
	return inst->netgame_flag_type == _netgame_flag_teleporter_source ? 1 :
		(inst->netgame_flag_type == _netgame_flag_teleporter_target ? 2 : 0);
}

static int forge_tele_channel(struct forge_live_instance const *inst)
{
	return (inst->team_designator >= 0 && inst->team_designator < FORGE_TELE_CHANNELS) ?
		inst->team_designator : 0;
}

static char const *forge_tele_role_name(int role)
{
	return role == 1 ? "SENDER" : (role == 2 ? "RECEIVER" : "OFF");
}

/* the teleporters of `role` on `channel`, but `except` */
static int forge_tele_count(int role, int channel, int32_t except)
{
	int i, count = 0;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];

		if (i != except && forge_tele_is(inst) && forge_tele_role(inst) == role && forge_tele_channel(inst) == channel)
			count++;
	}
	return count;
}

/* its other ends: the receivers of a sender's channel, or the senders of a receiver's */
static int forge_tele_other_ends(int32_t live_idx, int32_t *out, int max_out)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[live_idx];
	int role = forge_tele_role(inst);
	int i, count = 0;

	if (!forge_tele_is(inst) || role == 0)
		return 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *other = &g_forge.live_instances[i];

		if (i == live_idx || !forge_tele_is(other) || forge_tele_role(other) != 3 - role ||
			forge_tele_channel(other) != forge_tele_channel(inst))
		{
			continue;
		}
		if (out && count < max_out)
			out[count] = i;
		count++;
	}
	return count;
}

/* a new one's channel and role (`want_role` 0: either): the other end of a
channel that has one end only, else the first channel no one uses */
static int forge_tele_auto_pick(int32_t except, int want_role, int *out_role)
{
	int c;

	for (c = 0; c < FORGE_TELE_CHANNELS; c++)
	{
		int senders = forge_tele_count(1, c, except);
		int receivers = forge_tele_count(2, c, except);

		if (want_role != 1 && senders > 0 && receivers == 0)
		{
			*out_role = 2;
			return c;
		}
		if (want_role == 1 && receivers > 0 && senders == 0)
		{
			*out_role = 1;
			return c;
		}
	}
	*out_role = want_role ? want_role : 1;
	for (c = 0; c < FORGE_TELE_CHANNELS; c++)
	{
		if (forge_tele_count(1, c, except) == 0 && forge_tele_count(2, c, except) == 0)
			return c;
	}
	return 0;
}

/* (no network: forge_tele_set) */
static void forge_tele_apply(int32_t live_idx, int role, int channel)
{
	struct forge_live_instance *inst = &g_forge.live_instances[live_idx];

	if (channel < 0 || channel >= FORGE_TELE_CHANNELS)
		channel = 0;
	if (inst->group_tag == 'tele')
	{
		/* (a node is always one or the other, its marker the role's) */
		if (role != 2)
			role = 1;
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s",
			role == 2 ? "teleporter\\destination" : "teleporter\\origin");
	}
	inst->netgame_flag_type = (int16_t)(role == 1 ? _netgame_flag_teleporter_source :
		(role == 2 ? _netgame_flag_teleporter_target : -1));
	inst->team_designator = (int16_t)channel;
	/* (a placed scenery's saved team is its usage: an unbaked one comes back as it was) */
	if (inst->group_tag != 'tele' && !inst->is_default_scenario_object &&
		inst->edit_entry_index >= 0 && inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
	{
		g_forge.edit_state.placed_objects[inst->edit_entry_index].team_index = FORGE_TELE_USAGE(role, channel);
	}
	sync_live_netgame_flags_to_scenario();
}

int forge_3d_teleporter_net_usage(int32_t live_idx)
{
	struct forge_live_instance const *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return -1;
	inst = &g_forge.live_instances[live_idx];
	if (!forge_tele_is(inst))
		return -1;
	return FORGE_TELE_USAGE(forge_tele_role(inst), forge_tele_channel(inst));
}

static void forge_tele_set(int32_t live_idx, int role, int channel)
{
	forge_tele_apply(live_idx, role, channel);
	forge_network_send_marker_usage(live_idx, (int16_t)forge_3d_teleporter_net_usage(live_idx));
}

/* a teleporter scenery standing on a node takes the node's place: its role,
channel and where its flag stands. A map's own pairs (Blood Gulch's) are
nodes on their scenery, so moving the scenery moves the teleporter. Every
machine does this alike, as the map loads and as a scenery is placed */
static int forge_tele_absorb_node(int32_t scenery_idx)
{
	struct forge_live_instance *sc = &g_forge.live_instances[scenery_idx];
	float best_d2 = 0.6f * 0.6f;
	int32_t best = -1;
	int i;

	if (!sc->active || sc->group_tag == 'tele' || !forge_teleporter_scenery_tag(sc->group_tag, sc->tag_path))
		return 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *node = &g_forge.live_instances[i];
		float dx = node->x - sc->x;
		float dy = node->y - sc->y;
		float dz = node->z - sc->z;

		if (!node->active || node->group_tag != 'tele' || dz < -0.5f || dz > 1.5f)
			continue;
		if (dx * dx + dy * dy < best_d2)
		{
			best_d2 = dx * dx + dy * dy;
			best = i;
		}
	}
	if (best < 0)
		return 0;
	{
		struct forge_live_instance *node = &g_forge.live_instances[best];

		sc->tele_dz = node->z - sc->z;
		sc->tele_dyaw = node->yaw - sc->yaw;
		s_tele_default_dz = sc->tele_dz;
		node->active = 0;
		forge_tele_apply(scenery_idx, forge_tele_role(node), forge_tele_channel(node));
	}
	return 1;
}

/* a placed teleporter scenery: what it was if another machine or the map's
saved state says (`usage`, -1 for none: a palette's or a copy's), else a
node under it, else paired */
static void forge_tele_scenery_placed(int32_t live_idx, int usage)
{
	struct forge_live_instance *inst = &g_forge.live_instances[live_idx];

	inst->tele_dz = s_tele_default_dz;
	inst->tele_dyaw = 0.0f;
	if (usage >= 0)
		forge_tele_apply(live_idx, (usage >> 8) & 3, usage & 0xFF);
	if (forge_tele_absorb_node(live_idx))
		return;
	if (usage < 0)
	{
		int role = 1;
		int channel = forge_tele_auto_pick(live_idx, 0, &role);

		forge_tele_apply(live_idx, role, channel);
	}
}

/* what a teleporter does, for the status line */
static void forge_tele_describe(int32_t live_idx, char *out, size_t out_size)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[live_idx];
	int role = forge_tele_role(inst);
	int channel = forge_tele_channel(inst);
	int ends = forge_tele_other_ends(live_idx, NULL, 0);

	if (role == 0)
		snprintf(out, out_size, "Teleporter OFF: set it to SENDER or RECEIVER");
	else if (ends == 0)
		snprintf(out, out_size, "Teleporter %s #%d: no %s on channel #%d yet",
			forge_tele_role_name(role), channel, role == 1 ? "receiver" : "sender", channel);
	else
		snprintf(out, out_size, "Teleporter %s #%d: linked to %d %s%s",
			forge_tele_role_name(role), channel, ends, role == 1 ? "receiver" : "sender", ends == 1 ? "" : "s");
}

/* the teleporter scenery that is this object, or -1 */
static int32_t forge_tele_of_object(long object_index)
{
	int i;

	if (object_index == NONE)
		return -1;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];

		if (inst->object_index == object_index)
			return (inst->group_tag != 'tele' && forge_tele_is(inst)) ? i : -1;
	}
	return -1;
}

/* Syncs all active 'tele', 'flag', and 'ffsp' live_instances into the in-memory scenario->netgame_flags
 * AND into g_forge.edit_state.netgame_flags so teleporters, objectives, and Firefight spawns work live and save cleanly. */
static void sync_live_netgame_flags_to_scenario(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	int i, out_count = 0;

	g_forge.edit_state.replace_netgame_flags = 1;
	for (i = 0; i < g_forge.live_instance_count && out_count < FORGE_MAX_NETGAME_FLAGS; i++)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[i];
		struct forge_netgame_flag_entry *nf;
		int is_tele_scenery;
		if (!inst->active)
			continue;
		/* (a teleporter's scenery, its flag with it: forge_tele_*) */
		is_tele_scenery = inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp' &&
			forge_teleporter_scenery_tag(inst->group_tag, inst->tag_path);
		if (is_tele_scenery)
		{
			if (forge_tele_role(inst) == 0)
				continue;
			nf = &g_forge.edit_state.netgame_flags[out_count];
			memset(nf, 0, sizeof(*nf));
			nf->x = inst->x;
			nf->y = inst->y;
			nf->z = inst->z + inst->tele_dz;
			nf->yaw = inst->yaw + inst->tele_dyaw;
			nf->type = inst->netgame_flag_type;
			nf->usage_id = (int16_t)forge_tele_channel(inst);
			out_count++;
			continue;
		}
		if (inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp')
			continue;

		if (inst->group_tag == 'ffsp')
			inst->netgame_flag_type = FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN;

		if (inst->group_tag == 'flag' && inst->netgame_flag_type == _netgame_flag_ctf_flag)
		{
			inst->team_designator = (inst->team_designator == 1) ? 1 : 0;
			snprintf(inst->tag_path, sizeof(inst->tag_path), "%s",
				inst->team_designator == 1 ? "objective\\ctf_flag_blue" : "objective\\ctf_flag_red");
		}

		nf = &g_forge.edit_state.netgame_flags[out_count];
		memset(nf, 0, sizeof(*nf));
		nf->x = inst->x;
		nf->y = inst->y;
		nf->z = inst->z;
		nf->yaw = inst->yaw;
		nf->type = inst->netgame_flag_type;
		nf->usage_id = inst->team_designator;
		inst->edit_entry_index = out_count;
		out_count++;
	}
	g_forge.edit_state.netgame_flag_count = out_count;

	if (scen && out_count <= FORGE_MAX_NETGAME_FLAGS)
	{
		/* Reallocate scenario->netgame_flags in memory if needed so newly added teleporters/hills work live */
		if (!s_live_nf_buf)
		{
			s_live_nf_buf = (uint8_t *)calloc(FORGE_MAX_NETGAME_FLAGS, 148u);
		}
		if (scen->netgame_flags.address && scen->netgame_flags.address != XBOX_ADDRESS(s_live_nf_buf)) /* AE hook */
		{
			s_orig_nf_scen = scen;
			s_orig_nf_addr = XBOX_POINTER(uint8_t, scen->netgame_flags.address); /* AE hook */
			s_orig_nf_count = scen->netgame_flags.count;
		}
		if (s_live_nf_buf)
		{
			memset(s_live_nf_buf, 0, (size_t)out_count * 148u);
			for (i = 0; i < out_count; i++)
			{
				uint8_t *dst = s_live_nf_buf + (size_t)i * 148u;
				*(float *)(dst + 0x00) = g_forge.edit_state.netgame_flags[i].x;
				*(float *)(dst + 0x04) = g_forge.edit_state.netgame_flags[i].y;
				*(float *)(dst + 0x08) = g_forge.edit_state.netgame_flags[i].z;
				*(float *)(dst + 0x0C) = g_forge.edit_state.netgame_flags[i].yaw;
				*(int16_t *)(dst + 0x10) = g_forge.edit_state.netgame_flags[i].type;
				*(int16_t *)(dst + 0x12) = g_forge.edit_state.netgame_flags[i].usage_id;
				*(int32_t *)(dst + 0x1C) = NONE; /* weapon_group.index = NONE */
			}
			/* Also update CTF flag base positions in s_orig_nf_addr if ctf_globals cached a pointer to it */
			if (s_orig_nf_addr && s_orig_nf_count > 0)
			{
				int j;
				for (j = 0; j < s_orig_nf_count; j++)
				{
					uint8_t *orig_f = s_orig_nf_addr + (size_t)j * 148u;
					int16_t otype = *(int16_t const *)(orig_f + 0x10);
					int16_t oteam = *(int16_t const *)(orig_f + 0x12);
					if (otype == _netgame_flag_ctf_flag)
					{
						for (i = 0; i < out_count; i++)
						{
							if (g_forge.edit_state.netgame_flags[i].type == _netgame_flag_ctf_flag &&
								g_forge.edit_state.netgame_flags[i].usage_id == oteam)
							{
								*(float *)(orig_f + 0x00) = g_forge.edit_state.netgame_flags[i].x;
								*(float *)(orig_f + 0x04) = g_forge.edit_state.netgame_flags[i].y;
								*(float *)(orig_f + 0x08) = g_forge.edit_state.netgame_flags[i].z;
								*(float *)(orig_f + 0x0C) = g_forge.edit_state.netgame_flags[i].yaw;
								break;
							}
						}
					}
				}
			}
			scen->netgame_flags.address = XBOX_ADDRESS(s_live_nf_buf); /* AE hook */
			scen->netgame_flags.count = out_count;
		}
		if (player_data)
		{
			long p_idx;
			for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
			{
				struct player_datum *pl = player_get(p_idx);
				if (pl && (pl->teleporter_index < 0 || pl->teleporter_index >= out_count))
					pl->teleporter_index = NONE;
			}
		}
	}
}

static void forge_ensure_object_connected_to_bsp(long object_index, float x, float y, float z)
{
	struct object_datum *obj = forge_valid_object_get(object_index);
	if (!obj)
		return;

	SET_FLAG(obj->object.flags, _object_cannot_be_garbage_bit, TRUE);
	SET_FLAG(obj->object.flags, _object_deleted_when_deactivated_bit, FALSE);
	SET_FLAG(obj->object.flags, _object_outside_of_map_bit, FALSE);
	if (TEST_FLAG(obj->object.flags, _object_garbage_bit))
		object_set_garbage(object_index, FALSE);

	if (obj->object.type == _object_type_weapon ||
		obj->object.type == _object_type_equipment ||
		obj->object.type == _object_type_garbage)
	{
		struct item_datum *it = (struct item_datum *)obj;
		it->item.detonation_ticks = 0;
		it->item.last_owned_time = game_time_get();
	}

	if (obj->object.parent_object_index == NONE &&
		(obj->object.first_cluster_reference_index == NONE ||
		 obj->object.location.cluster_index == NONE ||
		 TEST_FLAG(obj->object.flags, _object_outside_of_map_bit)))
	{
		static float const s_Offsets[10][3] = {
			{  0.00f,  0.00f,  0.08f },
			{  0.00f,  0.00f,  0.25f },
			{  0.00f,  0.00f,  0.60f },
			{  0.00f,  0.00f,  1.10f },
			{  0.00f,  0.00f, -0.20f },
			{  0.25f,  0.00f,  0.15f },
			{ -0.25f,  0.00f,  0.15f },
			{  0.00f,  0.25f,  0.15f },
			{  0.00f, -0.25f,  0.15f },
			{  0.00f,  0.00f,  2.00f }
		};
		struct location loc;
		real_point3d probe_pt;
		int k;
		int found = 0;

		for (k = 0; k < 10; k++)
		{
			probe_pt.x = x + s_Offsets[k][0];
			probe_pt.y = y + s_Offsets[k][1];
			probe_pt.z = z + s_Offsets[k][2];
			memset(&loc, 0, sizeof(loc));
			loc.cluster_index = NONE;
			scenario_location_from_point(&loc, &probe_pt);
			if (loc.cluster_index != NONE)
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
			probe_pt.x = g_forge.cam_x;
			probe_pt.y = g_forge.cam_y;
			probe_pt.z = g_forge.cam_z;
			memset(&loc, 0, sizeof(loc));
			loc.cluster_index = NONE;
			scenario_location_from_point(&loc, &probe_pt);
			if (loc.cluster_index != NONE)
				found = 1;
		}
		if (found)
		{
			object_disconnect_from_map(object_index);
			object_reconnect_to_map(object_index, &loc);
			SET_FLAG(obj->object.flags, _object_outside_of_map_bit, FALSE);
			object_set_automatic_deactivation(object_index, FALSE);
			object_activate(object_index);
		}
	}
}

/* a scenery whose own animation puts its model far from where it stands
(Keyes' Flood form: d20's idle has its root where he was in that level, 130
units off) stands in its model's pose instead, as Firefight's Flood drop does */
void forge_scenery_hold_still_if_strayed(long object_index)
{
	struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_scenery);

	if (obj && obj->object.animation.state.index != NONE &&
		distance3d(&obj->object.bounding_sphere_center, &obj->object.position) >
			20.0f + obj->object.bounding_sphere_radius)
	{
		union real_point3d position = obj->object.position;
		union real_vector3d forward = obj->object.forward;
		union real_vector3d up = obj->object.up;

		obj->object.animation.state.index = NONE;
		SET_FLAG(obj->object.flags, _object_animates_automatically_bit, FALSE);
		object_set_position(object_index, &position, &forward, &up);
	}
}

static long forge_create_configured_engine_object(
	long def_idx,
	uint32_t group_tag,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int always_spawn,
	float scale)
{
	struct object_placement_data placement;
	long obj_idx;
	struct object_datum *obj;

	if (def_idx == NONE)
		return NONE;

	object_placement_data_new(&placement, def_idx, NONE);
	SET_FLAG(placement.flags, _new_object_never_automatically_delete_bit, TRUE);
	placement.position.x = x;
	placement.position.y = y;
	placement.position.z = z;
	euler_to_forward_up(yaw, pitch, roll, &placement.forward, &placement.up);

	/* (a vehicle set to ALWAYS SPAWN is made whatever the gametype's
	vehicles, which object_new otherwise asks: game_engine_remap_vehicle) */
	game_engine_vehicle_always_spawn = always_spawn && group_tag == 'vehi';
	obj_idx = object_new(&placement);
	game_engine_vehicle_always_spawn = FALSE;
	obj = forge_valid_object_get(obj_idx);
	if (!obj)
		return NONE;
	forge_scenery_hold_still_if_strayed(obj_idx);
	/* (SCALE: set, and moved where it is, which works out its nodes at its size) */
	if (scale > 0.0f && fabs((double)scale - 1.0) > 0.001)
	{
		obj->object.scale = scale;
		object_set_position(obj_idx, &placement.position, &placement.forward, &placement.up);
	}

	SET_FLAG(obj->object.flags, _object_cannot_be_garbage_bit, TRUE);
	SET_FLAG(obj->object.flags, _object_deleted_when_deactivated_bit, FALSE);
	SET_FLAG(obj->object.flags, _object_outside_of_map_bit, FALSE);
	if (group_tag == 'vehi')
	{
		SET_FLAG(obj->object.flags, _object_at_rest_bit, FALSE);
		unit_set_enterable_by_player(obj_idx, TRUE);
		unit_set_controllable(obj_idx, TRUE);
	}
	else if (obj->object.type == _object_type_machine ||
			 obj->object.type == _object_type_light_fixture ||
			 obj->object.type == _object_type_control)
	{
		struct scenario_device_datum scen_dev;
		memset(&scen_dev, 0, sizeof(scen_dev));
		scen_dev.power_group_index = NONE;
		scen_dev.position_group_index = NONE;
		scen_dev.flags = 0;
		device_add_scenario_information(obj_idx, &scen_dev);
		if (obj->object.type == _object_type_light_fixture)
		{
			struct light_fixture_datum *lf = (struct light_fixture_datum *)obj;
			lf->light_fixture.color.red = 1.0f;
			lf->light_fixture.color.green = 1.0f;
			lf->light_fixture.color.blue = 1.0f;
			lf->light_fixture.intensity = 1.0f;
			lf->light_fixture.falloff_angle = 1.0f;
			lf->light_fixture.cutoff_angle = 1.2f;
		}
	}
	object_set_automatic_deactivation(obj_idx, FALSE);
	object_activate(obj_idx);
	forge_ensure_object_connected_to_bsp(obj_idx, x, y, z);
	return obj_idx;
}

/* the next object spawned is a vehicle set to ALWAYS SPAWN: a saved map's,
its OBJECT_ALWAYS_SPAWN line read just before its OBJECT line
(forge_runtime_tags.c), or a network peer's !FV: */
int g_forge_next_spawn_always = 0;
float g_forge_next_spawn_scale = 0.0f;

/* the undo history (UNDO/REDO, ctrl+z/ctrl+y: forge_history_*, after the
properties' UNDO) */
static void forge_history_forget(int32_t live_idx);
static void forge_history_begin(int32_t live_idx);
static void forge_history_end(int32_t live_idx);
static void forge_history_note_placed(int32_t live_idx, int new_group);

int32_t forge_3d_spawn_object_at_slot(
	int32_t forced_live_idx,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net)
{
	int32_t live_idx = -1;
	struct forge_live_instance *inst;
	int always_spawn = g_forge_next_spawn_always && group_tag == 'vehi';
	float spawn_scale = group_tag == 'scen' || group_tag == 'deca' ? g_forge_next_spawn_scale : 0.0f;

	g_forge_next_spawn_always = 0;
	g_forge_next_spawn_scale = 0.0f;
	if (!tag_path || !tag_path[0])
		return -1;

	if (forced_live_idx >= 0 && forced_live_idx < FORGE_MAX_LIVE_INSTANCES)
	{
		if (forced_live_idx >= g_forge.live_instance_count)
		{
			memset(
				&g_forge.live_instances[g_forge.live_instance_count],
				0,
				(size_t)(forced_live_idx - g_forge.live_instance_count + 1) * sizeof(struct forge_live_instance));
			g_forge.live_instance_count = forced_live_idx + 1;
		}
		else if (g_forge.live_instances[forced_live_idx].active &&
			g_forge.live_instances[forced_live_idx].object_index != NONE)
		{
			if (forge_valid_object_get(g_forge.live_instances[forced_live_idx].object_index))
				object_delete(g_forge.live_instances[forced_live_idx].object_index);
			g_forge.live_instances[forced_live_idx].object_index = NONE;
		}
		live_idx = forced_live_idx;
	}
	else if (g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES)
	{
		live_idx = g_forge.live_instance_count++;
	}
	else
	{
		int32_t s;
		for (s = 0; s < g_forge.live_instance_count; s++)
		{
			if (!g_forge.live_instances[s].active)
			{
				live_idx = s;
				break;
			}
		}
		if (live_idx < 0)
		{
			forge_set_status("Forge Object Budget Reached (%d/%d)!", count_active_live_instances(), FORGE_MAX_LIVE_INSTANCES);
			return -1;
		}
	}

	/* (a slot taken by something else: the history's steps of what was there go) */
	if (!g_forge.live_instances[live_idx].active || g_forge.live_instances[live_idx].group_tag != group_tag)
		forge_history_forget(live_idx);
	forge_network_slot_made(live_idx); /* AE hook */
	inst = &g_forge.live_instances[live_idx];
	memset(inst, 0, sizeof(*inst));
	inst->active = 1;
	inst->object_index = NONE;
	inst->actor_index = NONE;
	inst->edit_entry_index = -1;
	inst->group_tag = group_tag;
	inst->source_cache_id = source_cache_id;
	inst->gametype_flags = 0; /* 0 = All Modes */
	inst->netgame_flag_type = -1;
	inst->team_designator = team_designator;
	inst->always_spawn = (uint8_t)always_spawn;
	inst->scale = spawn_scale;
	inst->x = x;
	inst->y = y;
	inst->z = z;
	inst->yaw = yaw;
	inst->pitch = pitch;
	inst->roll = roll;
	snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tag_path);
	if (game_connection() == _game_connection_network_client && broadcast_net != 0)
		inst->pad_inst |= 0x80u;

	if (group_tag == 'spwn')
	{
		inst->team_designator = strstr(tag_path, "blue") ? 1 : strstr(tag_path, "ffa") ? 2 : 0;
		inst->netgame_flag_type = (inst->team_designator == 2) ? 13 : 12;
		sync_live_player_spawns_to_scenario();
		forge_set_status("Placed %s Player Spawn at (%.1f, %.1f, %.1f)",
			inst->team_designator == 1 ? "Blue Team" : (inst->team_designator == 2 ? "FFA (Yellow)" : "Red Team"),
			x, y, z);
	}
	else if (group_tag == 'ffsp')
	{
		inst->netgame_flag_type = FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN;
		inst->team_designator = (team_designator >= 0) ? team_designator : 0;
		sync_live_netgame_flags_to_scenario();
		forge_set_status("Placed Firefight Spawn [%s | %s | %s]",
			forge_ff_spawn_type_label(FORGE_FF_UNPACK_TYPE(inst->team_designator)),
			forge_ff_round_filter_label(FORGE_FF_UNPACK_ROUND(inst->team_designator)),
			forge_ff_wave_filter_label(FORGE_FF_UNPACK_WAVE(inst->team_designator)));
	}
	else if (group_tag == FORGE_GROUP_AI_SPAWN)
	{
		inst->edit_entry_index = forge_ai_spawn_entry_new(live_idx, tag_path, x, y, z, yaw, team_designator);
		if (inst->edit_entry_index < 0)
		{
			inst->active = 0;
			forge_set_status("AI Spawn Limit Reached (%d)!", FORGE_MAX_AI_SPAWNS);
			return -1;
		}
		/* (RANDOM's path is the palette's: a rank's tag stands for it) */
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s",
			g_forge.edit_state.ai_spawns[inst->edit_entry_index].actor_variant_path);
		forge_ai_preview_update(live_idx);
		forge_set_status("Placed AI Spawn");
	}
	else if (group_tag == FORGE_GROUP_VOLUME)
	{
		char what[96];

		/* (a volume is no object: forge_volumes.c keeps what it is) */
		inst->object_index = NONE;
		inst->edit_entry_index = -1;
		forge_volume_spawned(live_idx, tag_path);
		forge_volume_describe(live_idx, what, sizeof(what));
		if (broadcast_net)
			forge_set_status("Placed %s", what);
	}
	else if (group_tag == FORGE_GROUP_AI_ROUTE)
	{
		/* (a route point is no object: its route, place, behavior and wait are
		its team number, forge_ai_routes.h) */
		inst->object_index = NONE;
		inst->edit_entry_index = -1;
		forge_ai_route_spawned(live_idx, broadcast_net);
		if (!inst->active)
			return -1;
	}
	else if (group_tag == 'tele')
	{
		/* _netgame_flag_teleport_from = 6, _netgame_flag_teleport_to = 7 */
		int role = strstr(tag_path, "destination") ? 2 : 1;
		int channel = (team_designator >= 0 && team_designator < FORGE_TELE_CHANNELS) ? team_designator : 0;
		char what[96];

		/* (placed here from the palette: paired with a lone end, or on a channel of its own) */
		if (broadcast_net && team_designator == 0)
			channel = forge_tele_auto_pick(live_idx, role, &role);
		forge_tele_apply(live_idx, role, channel);
		forge_tele_describe(live_idx, what, sizeof(what));
		forge_set_status("Placed %s", what);
	}
	else if (group_tag == 'flag')
	{
		/* _netgame_flag_ctf_flag = 0, _netgame_flag_oddball_ball_spawn = 2, _netgame_flag_race_track = 3, _netgame_flag_hill_flag = 8 */
		inst->netgame_flag_type =
			strstr(tag_path, "firefight_generator") ? FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR :
			strstr(tag_path, "oddball") ? 2 :
			strstr(tag_path, "koth") ? 8 :
			strstr(tag_path, "race") ? 3 : 0;
		if (inst->netgame_flag_type == 0)
		{
			inst->team_designator = strstr(tag_path, "blue") ? 1 : 0;
		}
		else if (inst->netgame_flag_type == 2 && team_designator == 0)
		{
			/* Auto-assign next unused Oddball spawn ID (0..15) so multiple ball spawns each have a valid ID */
			int cand, j, used;
			int16_t chosen_id = 0;
			for (cand = 0; cand < 16; cand++)
			{
				used = 0;
				for (j = 0; j < live_idx; j++)
				{
					if (g_forge.live_instances[j].active &&
						g_forge.live_instances[j].group_tag == 'flag' &&
						g_forge.live_instances[j].netgame_flag_type == 2 &&
						g_forge.live_instances[j].team_designator == cand)
					{
						used = 1;
						break;
					}
				}
				if (!used)
				{
					chosen_id = (int16_t)cand;
					break;
				}
			}
			inst->team_designator = chosen_id;
		}
		else
		{
			inst->team_designator = (team_designator >= 0) ? team_designator : 0;
		}
		sync_live_netgame_flags_to_scenario();
		forge_set_status("Placed Objective '%s' [Ref/Team #%d] at (%.1f, %.1f, %.1f)",
			tag_path, (int)inst->team_designator, x, y, z);
	}
	else if (group_tag == 'actv')
	{
		int32_t spawned_unit = NONE;
		inst->is_ai_unit = 1;
		if (game_connection() != _game_connection_network_client)
		{
			inst->actor_index = forge_ai_spawn_unit_live(tag_path, x, y, z, yaw, team_designator, &spawned_unit);
			inst->object_index = spawned_unit;
		}
		forge_set_status("Placed AI '%s' at (%.1f, %.1f, %.1f)", tag_path, x, y, z);
		if (broadcast_net && forge_is_multiplayer_session())
		{
			forge_network_send_spawn_ext(
				live_idx,
				group_tag,
				tag_path,
				source_cache_id,
				x, y, z,
				yaw, pitch, roll,
				team_designator,
				inst->object_index,
				NONE);
		}
		return live_idx;
	}
	else
	{
		uint32_t actual_grp = group_tag;
		long def_idx = forge_resolve_engine_tag(group_tag, tag_path, &actual_grp);
		int is_net_obj_type = (group_tag == 'vehi' || group_tag == 'weap' ||
							   group_tag == 'eqip' || group_tag == 'bipd' ||
							   actual_grp == 'vehi' || actual_grp == 'weap' ||
							   actual_grp == 'eqip' || actual_grp == 'bipd');
		int skip_local_obj_new_on_client = (game_connection() == _game_connection_network_client && is_net_obj_type);

		/* (an effect spot is no object: forge_effect_spots_update plays it there) */
		if (def_idx != NONE && !skip_local_obj_new_on_client && group_tag != 'effe' && group_tag != 'deca')
		{
			inst->object_index = forge_create_configured_engine_object(
				def_idx, group_tag, x, y, z, yaw, pitch, roll, inst->always_spawn, inst->scale);
		}

		if (g_forge.edit_state.placed_object_count < FORGE_MAX_PLACED_OBJECTS)
		{
			struct forge_placed_object_entry *po = &g_forge.edit_state.placed_objects[g_forge.edit_state.placed_object_count];
			inst->edit_entry_index = g_forge.edit_state.placed_object_count++;
			memset(po, 0, sizeof(*po));
			po->active = 1;
			po->source_cache_id = source_cache_id;
			po->group_tag = group_tag;
			po->gametype_flags = inst->gametype_flags;
			po->always_spawn = inst->always_spawn;
			po->no_damage = inst->no_damage;
			po->no_collision = inst->no_collision;
			po->scale = inst->scale;
			po->category = (group_tag == 'vehi') ? _forge_object_vehicle :
				(group_tag == 'weap') ? _forge_object_weapon :
				(group_tag == 'bipd') ? _forge_object_biped :
				(group_tag == 'eqip') ? _forge_object_equipment : _forge_object_scenery;
			snprintf(po->tag_path, sizeof(po->tag_path), "%s", tag_path);
			po->x = x;
			po->y = y;
			po->z = z;
			po->yaw = yaw;
			po->pitch = pitch;
			po->roll = roll;
		}
		/* (an old save's tag no one has now: the object stays in the map's
		record, and comes back if the tag does) */
		if (inst->edit_entry_index < 0)
			forge_set_status("Save budget reached (%d placed): '%s' won't be saved with the map",
				FORGE_MAX_PLACED_OBJECTS, tag_path);
		else if (def_idx == NONE)
			forge_set_status("Missing tag '%s': it stays in the map, but cannot be shown", tag_path);
		else
			forge_set_status("Placed '%s' at (%.1f, %.1f, %.1f)", tag_path, x, y, z);

		/* (a teleporter's scenery is one: paired here, as it was elsewhere; the other
		machines hear its role and channel as its team) */
		if (forge_teleporter_scenery_tag(group_tag, tag_path))
		{
			char what[96];

			forge_tele_scenery_placed(live_idx,
				(team_designator & FORGE_TELE_USAGE_SET) ? (int)(uint16_t)team_designator : -1);
			team_designator = (int16_t)forge_3d_teleporter_net_usage(live_idx);
			forge_tele_describe(live_idx, what, sizeof(what));
			forge_set_status("Placed %s", what);
		}

		if (broadcast_net && forge_is_multiplayer_session())
		{
			forge_network_send_spawn_ext(
				live_idx,
				group_tag,
				tag_path,
				source_cache_id,
				x, y, z,
				yaw, pitch, roll,
				team_designator,
				inst->object_index,
				def_idx);
		}
		return live_idx;
	}

	if (broadcast_net && forge_is_multiplayer_session())
	{
		forge_network_send_spawn_ext(
			live_idx,
			group_tag,
			tag_path,
			source_cache_id,
			x, y, z,
			yaw, pitch, roll,
			inst->team_designator,
			inst->object_index,
			NONE);
	}

	return live_idx;
}

int32_t forge_3d_spawn_object_at(
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net)
{
	return forge_3d_spawn_object_at_slot(
		-1,
		group_tag,
		tag_path,
		source_cache_id,
		x, y, z,
		yaw, pitch, roll,
		team_designator,
		broadcast_net);
}

static void forge_eject_all_passengers_from_vehicle(long vehicle_index)
{
	struct object_iterator u_iter;
	struct unit_datum *u;

	if (vehicle_index == NONE)
		return;

	object_iterator_new(&u_iter, _object_mask_unit, 0);
	while ((u = (struct unit_datum *)object_iterator_next(&u_iter)) != NULL)
	{
		long u_idx = u_iter.index;
		if (u_idx == vehicle_index)
			continue;
		if (u->object.parent_object_index == vehicle_index && u->unit.parent_seat_index != NONE)
		{
			unit_exit_seat_end(u_idx);
			SET_FLAG(u->object.flags, _object_at_rest_bit, FALSE);
			object_activate(u_idx);
		}
	}
}

void forge_3d_move_instance_ext(
	int32_t live_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int is_held,
	int broadcast_net)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active)
		return;

	inst->x = x;
	inst->y = y;
	inst->z = z;
	inst->yaw = yaw;
	inst->pitch = pitch;
	inst->roll = roll;

	if (inst->object_index != NONE)
	{
		struct object_datum *obj = forge_valid_object_get(inst->object_index);
		if (obj)
		{
			real_point3d pos;
			real_vector3d fwd, up;
			pos.x = x;
			pos.y = y;
			pos.z = z;
			euler_to_forward_up(yaw, pitch, roll, &fwd, &up);
			if ((obj->object.type == _object_type_weapon ||
				 obj->object.type == _object_type_equipment ||
				 obj->object.type == _object_type_garbage) &&
				obj->definition_index != NONE)
			{
				struct item_definition *idef = item_definition_get(obj->definition_index);
				if (idef)
					idef->item.flags &= ~1UL; /* Clear _item_definition_always_maintains_z_up_bit so pitched/rolled items stay oriented */
			}
			if (obj->object.type == _object_type_vehicle)
			{
				unit_set_enterable_by_player(inst->object_index, TRUE);
				unit_set_controllable(inst->object_index, TRUE);
				if (is_held)
					forge_eject_all_passengers_from_vehicle(inst->object_index);
			}
			SET_FLAG(obj->object.flags, _object_outside_of_map_bit, FALSE);
			SET_FLAG(obj->object.flags, _object_no_collisions_bit, is_held ? TRUE : FALSE);
			if (inst->group_tag == FORGE_GROUP_AI_SPAWN)
				forge_ai_preview_protect(inst->object_index);
			if (!is_held && obj->object.type == _object_type_vehicle)
			{
				/* Placed/released vehicle: wake physics on BOTH Host and Clients so it falls to ground and is immediately drivable! */
				SET_FLAG(obj->object.flags, _object_at_rest_bit, FALSE);
				object_set_position(inst->object_index, &pos, &fwd, &up);
				object_set_automatic_deactivation(inst->object_index, FALSE);
				object_activate(inst->object_index);
				forge_ensure_object_connected_to_bsp(inst->object_index, x, y, z);
				obj->object.translational_velocity.i = 0.0f;
				obj->object.translational_velocity.j = 0.0f;
				obj->object.translational_velocity.k = -0.02f;
				obj->object.angular_velocity.i = 0.0f;
				obj->object.angular_velocity.j = 0.0f;
				obj->object.angular_velocity.k = 0.0f;
			}
			else
			{
				SET_FLAG(obj->object.flags, _object_at_rest_bit, TRUE);
				object_set_position(inst->object_index, &pos, &fwd, &up);
				object_set_automatic_deactivation(inst->object_index, FALSE);
				object_activate(inst->object_index);
				forge_ensure_object_connected_to_bsp(inst->object_index, x, y, z);
				obj->object.translational_velocity.i = 0.0f;
				obj->object.translational_velocity.j = 0.0f;
				obj->object.translational_velocity.k = 0.0f;
				obj->object.angular_velocity.i = 0.0f;
				obj->object.angular_velocity.j = 0.0f;
				obj->object.angular_velocity.k = 0.0f;
			}
			/* (its lights with it: nothing moves a device's or scenery's in the game) */
			object_attached_lights_moved(inst->object_index);
		}
		else
		{
			inst->object_index = NONE;
		}
	}

	/* (a teleporter's scenery is saved as an object, and its flag goes with it) */
	if (forge_teleporter_scenery_tag(inst->group_tag, inst->tag_path))
		sync_live_netgame_flags_to_scenario();
	if (inst->group_tag == 'tele' || inst->group_tag == 'flag' || inst->group_tag == 'ffsp')
	{
		sync_live_netgame_flags_to_scenario();
	}
	else if (inst->group_tag == 'spwn')
	{
		sync_live_player_spawns_to_scenario();
	}
	else if (inst->group_tag == FORGE_GROUP_AI_SPAWN)
	{
		forge_ai_spawn_moved(inst);
	}
	else if (inst->group_tag == FORGE_GROUP_VOLUME)
	{
		forge_volume_moved(live_idx);
	}
	else if (inst->is_default_scenario_object)
	{
		forge_ensure_default_object_edit_entry(inst);
		if (inst->edit_entry_index >= 0 && inst->edit_entry_index < g_forge.edit_state.default_edit_count)
		{
			struct forge_default_object_edit *de = &g_forge.edit_state.default_edits[inst->edit_entry_index];
			de->new_x = x;
			de->new_y = y;
			de->new_z = z;
			de->new_yaw = yaw;
			de->new_pitch = pitch;
			de->new_roll = roll;
			de->gametype_flags = inst->gametype_flags;
		}
	}
	else if (!inst->is_ai_unit && inst->edit_entry_index >= 0 &&
		inst->edit_entry_index < g_forge.edit_state.placed_object_count)
	{
		struct forge_placed_object_entry *po = &g_forge.edit_state.placed_objects[inst->edit_entry_index];
		po->x = x;
		po->y = y;
		po->z = z;
		po->yaw = yaw;
		po->pitch = pitch;
		po->roll = roll;
		po->gametype_flags = inst->gametype_flags;
		po->always_spawn = inst->always_spawn;
	}

	if (broadcast_net && forge_is_multiplayer_session())
	{
		forge_network_send_transform_ext(live_idx, inst->object_index, x, y, z, yaw, pitch, roll, is_held ? 1 : 0, 0);
	}
}

void forge_3d_move_instance(
	int32_t live_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int broadcast_net)
{
	int is_held = (live_idx >= 0 && live_idx == g_forge.grabbed_live_index) ? 1 : 0;
	forge_3d_move_instance_ext(live_idx, x, y, z, yaw, pitch, roll, is_held, broadcast_net);
}

/* (firefight_spawner.c: the flight Firefight flies, with no one aboard) */
extern boolean firefight_dropship_preview_start(real_point3d const *zone, real facing, long dropship_tag);
extern short firefight_dropship_preview_update(real seconds);
extern void firefight_dropship_preview_stop(void);
extern boolean firefight_dropship_preview_departing(void);
extern boolean firefight_flood_drop_preview_start(
	real_point3d const *zone, real facing, long flood_form_tag, long burst_effect_tag, long puff_effect_tag);
extern boolean firefight_flood_drop_preview_update(real seconds);
extern void firefight_flood_drop_preview_stop(void);
extern short firefight_ordnance_preview_start(real_point3d const *area, long area_object, short count);
extern short firefight_ordnance_preview_update(void);
extern void firefight_ordnance_preview_stop(void);
extern void firefight_ordnance_preview_refusals(char *text, long size);

/* the FIREFIGHT category's drop area (the game's props: tools/prop_assets.py),
where Firefight's ordnance pods come down; placed standing, as they land */
#define FORGE_DROP_AREA_TAG "firefight\\props\\drop_area_pod"
#define FORGE_DROP_AREA_PITCH (-1.5707963f)
#define FORGE_TEST_DROP_PODS 8

static int forge_is_drop_area(struct forge_live_instance const *inst)
{
	return inst->group_tag == 'scen' && _stricmp(inst->tag_path, FORGE_DROP_AREA_TAG) == 0;
}

static int forge_ff_is_dropship_zone(struct forge_live_instance const *inst);
static void forge_ff_route_remove_points(int route_id);

void forge_3d_delete_instance(int32_t live_idx, int broadcast_net)
{
	struct forge_live_instance *inst;
	long old_obj_idx;
	int route_id;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active)
		return;
	/* (a map's own volume stays the map's: turned off, not deleted) */
	if (inst->group_tag == FORGE_GROUP_VOLUME && forge_volume_get(live_idx) &&
		forge_volume_get(live_idx)->map_index >= 0)
	{
		struct forge_volume off = *forge_volume_get(live_idx);

		off.effect = _forge_volume_off;
		forge_volume_set(live_idx, &off, broadcast_net);
		forge_set_status("A map's volume can't be deleted: it is turned OFF");
		return;
	}

	/* (deleted: not undone, so the history's steps of it go) */
	forge_history_forget(live_idx);

	/* A dropship zone takes its route with it (other machines hear of each point) */
	route_id = (broadcast_net && forge_ff_is_dropship_zone(inst)) ? FF_ROUTE_ID(inst->team_designator) : 0;

	old_obj_idx = inst->object_index;
	if (inst->object_index != NONE)
	{
		if (forge_valid_object_get(inst->object_index))
			object_delete(inst->object_index);
		inst->object_index = NONE;
	}
	inst->active = 0;

	/* (a teleporter's scenery is saved as an object, and its flag goes with it) */
	if (forge_teleporter_scenery_tag(inst->group_tag, inst->tag_path))
		sync_live_netgame_flags_to_scenario();
	if (inst->group_tag == 'tele' || inst->group_tag == 'flag' || inst->group_tag == 'ffsp')
	{
		sync_live_netgame_flags_to_scenario();
	}
	else if (inst->group_tag == 'spwn')
	{
		sync_live_player_spawns_to_scenario();
	}
	else if (inst->group_tag == FORGE_GROUP_AI_SPAWN)
	{
		forge_ai_spawn_deleted(inst);
	}
	else if (inst->group_tag == FORGE_GROUP_AI_ROUTE)
	{
		/* (the route's others numbered on again, here; the others hear of each) */
		if (broadcast_net)
			forge_ai_route_point_deleted(FORGE_AI_ROUTE_NUMBER(inst->team_designator));
	}
	else if (inst->is_default_scenario_object)
	{
		forge_ensure_default_object_edit_entry(inst);
		if (inst->edit_entry_index >= 0 && inst->edit_entry_index < g_forge.edit_state.default_edit_count)
		{
			g_forge.edit_state.default_edits[inst->edit_entry_index].deleted = 1;
		}
	}
	else if (!inst->is_ai_unit && inst->edit_entry_index >= 0 &&
		inst->edit_entry_index < g_forge.edit_state.placed_object_count)
	{
		g_forge.edit_state.placed_objects[inst->edit_entry_index].active = 0;
		g_forge.edit_state.placed_objects[inst->edit_entry_index].x = -99999.0f;
	}

	forge_set_status("Deleted '%s'", inst->tag_path);

	if (broadcast_net && forge_is_multiplayer_session())
	{
		forge_network_send_delete_ext(live_idx, old_obj_idx);
	}
	if (route_id != 0)
		forge_ff_route_remove_points(route_id);
}

/* ---------- Dropship routes: a zone's flight path, as linked Firefight markers (halo_firefight_route.h) */

/* the factions an enemy drop point (once the dropship zone) spawns
(FF_DROP_KINDS_*), 0 for anything else */
static unsigned forge_ff_drop_kinds(struct forge_live_instance const *inst)
{
	return inst->active && inst->group_tag == 'ffsp' ?
		ff_drop_point_kinds(FORGE_FF_UNPACK_TYPE(inst->team_designator)) : 0;
}

static int forge_ff_is_dropship_zone(struct forge_live_instance const *inst)
{
	return forge_ff_drop_kinds(inst) != 0;
}

/* the SPAWN TYPE row's next type, within the marker's kind (FIREFIGHT SPAWNS
chose it): vehicle AI's LIGHT or HEAVY (one of before's ANY goes to them),
infantry with or without vehicles; the other kinds have one type each (an
enemy drop point's factions are its FACTIONS TO SPAWN page's) */
static int forge_ff_spawn_type_step(int type, int dir)
{
	switch (forge_ff_spawn_kind(type))
	{
	case _forge_ff_kind_vehicle:
		if (type == _forge_ff_spawn_enemy_vehicle_only)
			return dir > 0 ? _forge_ff_spawn_enemy_ghost_only : _forge_ff_spawn_enemy_heavy_veh;
		return type == _forge_ff_spawn_enemy_ghost_only ? _forge_ff_spawn_enemy_heavy_veh : _forge_ff_spawn_enemy_ghost_only;
	case _forge_ff_kind_infantry:
		return type == _forge_ff_spawn_ai_infantry_only ? _forge_ff_spawn_ai_and_vehicles : _forge_ff_spawn_ai_infantry_only;
	default:
		return type;
	}
}

/* ---------- A drop point's own rows, after the six every Firefight marker has

FLIGHT ROUTE (and its legs' styles once it has one) and TEST DROP while it
spawns a ship's faction, FACTIONS TO SPAWN (a page of its own), and TEST
FLOOD while it spawns the Flood */

enum forge_ff_zone_row
{
	_forge_ff_zone_row_route,
	_forge_ff_zone_row_arrival,
	_forge_ff_zone_row_departure,
	_forge_ff_zone_row_factions,
	_forge_ff_zone_row_test_drop,
	_forge_ff_zone_row_test_flood
};

#define FORGE_FF_ZONE_FIRST_ROW 6
#define FORGE_FF_ZONE_MAX_ROWS 6

static int forge_ff_zone_rows(struct forge_live_instance const *inst, int *rows)
{
	unsigned kinds = forge_ff_drop_kinds(inst);
	int count = 0;

	if (kinds & FF_DROP_KINDS_SHIPS)
	{
		rows[count++] = _forge_ff_zone_row_route;
		if (FF_ROUTE_ID(inst->team_designator) != 0)
		{
			rows[count++] = _forge_ff_zone_row_arrival;
			rows[count++] = _forge_ff_zone_row_departure;
		}
	}
	rows[count++] = _forge_ff_zone_row_factions;
	if (kinds & FF_DROP_KINDS_SHIPS)
		rows[count++] = _forge_ff_zone_row_test_drop;
	if (kinds & FF_DROP_KINDS_FLOOD)
		rows[count++] = _forge_ff_zone_row_test_flood;
	return count;
}

/* the panel's row a zone's row is at, or -1 when it has none */
static int forge_ff_zone_row_at(struct forge_live_instance const *inst, int cursor)
{
	int rows[FORGE_FF_ZONE_MAX_ROWS];
	int count = forge_ff_zone_rows(inst, rows);

	return cursor >= FORGE_FF_ZONE_FIRST_ROW && cursor < FORGE_FF_ZONE_FIRST_ROW + count ?
		rows[cursor - FORGE_FF_ZONE_FIRST_ROW] : -1;
}

/* the FACTIONS TO SPAWN page: HUMAN, COVENANT, FLOOD, then BACK */
static int s_props_factions_open = 0;
static int const s_props_faction_kinds[3] = { FF_DROP_KIND_HUMAN, FF_DROP_KIND_COVENANT, FF_DROP_KIND_FLOOD };

static void forge_ff_factions_summary(unsigned kinds, char *out, size_t size)
{
	if ((kinds & FF_DROP_KINDS_ALL) == FF_DROP_KINDS_ALL)
	{
		snprintf(out, size, "ALL");
		return;
	}
	snprintf(out, size, "%s%s%s%s%s",
		(kinds & FF_DROP_KINDS_HUMAN) ? "HUMAN" : "",
		(kinds & FF_DROP_KINDS_HUMAN) && (kinds & (FF_DROP_KINDS_COVENANT | FF_DROP_KINDS_FLOOD)) ? " + " : "",
		(kinds & FF_DROP_KINDS_COVENANT) ? "COV" : "",
		(kinds & FF_DROP_KINDS_COVENANT) && (kinds & FF_DROP_KINDS_FLOOD) ? " + " : "",
		(kinds & FF_DROP_KINDS_FLOOD) ? "FLOOD" : "");
}

static int forge_ff_is_route_point(struct forge_live_instance const *inst)
{
	return inst->active && inst->group_tag == 'ffsp' &&
		FORGE_FF_UNPACK_TYPE(inst->team_designator) == FF_ROUTE_SPAWN_TYPE;
}

/* a usage the other machines haven't seen: one they can't work out from a move */
static void forge_ff_marker_set_usage(int32_t live_idx, int16_t usage)
{
	if (g_forge.live_instances[live_idx].team_designator == usage)
		return;
	g_forge.live_instances[live_idx].team_designator = usage;
	sync_live_netgame_flags_to_scenario();
	forge_network_send_marker_usage(live_idx, usage);
}

void forge_3d_marker_usage_apply(int32_t live_idx, int16_t usage)
{
	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	/* (a teleporter's: its role and channel, FORGE_TELE_USAGE) */
	if (forge_tele_is(&g_forge.live_instances[live_idx]))
	{
		forge_tele_apply(live_idx, ((uint16_t)usage >> 8) & 3, usage & 0xFF);
		return;
	}
	/* (an AI route's point: forge_ai_routes.h) */
	if (forge_ai_route_is(&g_forge.live_instances[live_idx]))
	{
		forge_ai_route_usage_apply(live_idx, usage);
		return;
	}
	/* (an effect spot's REPEAT: its seconds) */
	if (g_forge.live_instances[live_idx].active && g_forge.live_instances[live_idx].group_tag == 'effe')
	{
		struct forge_live_instance *inst = &g_forge.live_instances[live_idx];

		inst->team_designator = usage;
		if (inst->edit_entry_index >= 0 && inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
			g_forge.edit_state.placed_objects[inst->edit_entry_index].team_index = usage;
		return;
	}
	if (!g_forge.live_instances[live_idx].active || g_forge.live_instances[live_idx].group_tag != 'ffsp')
		return;
	g_forge.live_instances[live_idx].team_designator = usage;
	sync_live_netgame_flags_to_scenario();
}

/* ---------- Effect spots: an effect placed as itself, played there again and
again (the blood and the Flood's gore: forge_runtime_tags.c's
is_forge_effect_spot_tag). Every machine plays its own, in Forge and in a game
of a saved map, harmless; REPEAT (the spot's team number) is how many seconds
apart */

static int const s_effect_spot_repeats[] = { 3, 1, 2, 5, 8, 12, 20, 30 };

static int forge_effect_spot_seconds(struct forge_live_instance const *inst)
{
	int k;

	for (k = 0; k < (int)(sizeof(s_effect_spot_repeats) / sizeof(s_effect_spot_repeats[0])); k++)
	{
		if (inst->team_designator == s_effect_spot_repeats[k])
			return s_effect_spot_repeats[k];
	}
	return s_effect_spot_repeats[0];
}

/* REPEAT's next, in order of seconds */
static int forge_effect_spot_seconds_step(int seconds, int dir)
{
	static int const s_ordered[] = { 1, 2, 3, 5, 8, 12, 20, 30 };
	int count = (int)(sizeof(s_ordered) / sizeof(s_ordered[0]));
	int k, at = 2;

	for (k = 0; k < count; k++)
	{
		if (s_ordered[k] == seconds)
			at = k;
	}
	return s_ordered[(at + count + dir) % count];
}

static long s_effect_spot_next_tick[FORGE_MAX_LIVE_INSTANCES];

static void forge_effect_spots_update(void)
{
	long now = game_time_get();
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		long period;
		long effect_tag;
		char const *marker_names[1] = { "" };
		real_point3d marker_points[1];
		real_vector3d marker_forwards[1];
		real_vector3d forward;

		if (!inst->active || inst->group_tag != 'effe')
		{
			s_effect_spot_next_tick[i] = 0;
			continue;
		}
		period = (long)forge_effect_spot_seconds(inst) * TICKS_PER_SECOND;
		/* (a new spot, or the game's time gone back: now) */
		if (s_effect_spot_next_tick[i] <= 0 || s_effect_spot_next_tick[i] > now + period)
			s_effect_spot_next_tick[i] = now;
		if (now < s_effect_spot_next_tick[i])
			continue;
		s_effect_spot_next_tick[i] = now + period;
		effect_tag = forge_resolve_engine_tag('effe', inst->tag_path, NULL);
		if (effect_tag == NONE)
			continue;
		marker_points[0].x = inst->x;
		marker_points[0].y = inst->y;
		marker_points[0].z = inst->z + 0.1f;
		/* (out of the spot's up, as a surface's blood comes off it) */
		euler_to_forward_up(inst->yaw, inst->pitch, inst->roll, &forward, &marker_forwards[0]);
		/* (harmless, but with its objects: the Flood's bursts are mostly the
		bits they throw, garbage the game clears away) */
		skulls_effect_harmless = TRUE;
		effect_new_unattached_from_markers(effect_tag, NONE, NULL, 1, marker_names, marker_points, marker_forwards,
			0.0f, 0.0f, NULL, NULL, TRUE);
		skulls_effect_harmless = FALSE;
	}
}

/* ---------- Placed decals: a decal put on the surface under where it's placed
(its up's other way), at its SCALE, on every machine. A decal can't move, so
one moved, scaled or made again by the level is taken away and made anew;
the game's own made are kept to take away (decals.c's capture) */

#define FORGE_DECAL_PIECES 8

static float forge_inst_scale(struct forge_live_instance const *inst);
static int get_filtered_catalog_indices(int *out_indices, int max_out);

static struct
{
	long pieces[FORGE_DECAL_PIECES];
	short piece_count;
	/* (made, or tried: a decal with no surface under it isn't tried each frame) */
	uint8_t tried;
	long made_time; /* the game's time it was made at (a game's start is its 0) */
	float x, y, z, yaw, pitch, roll, scale;
	uint32_t path_hash;
} s_forge_decals[FORGE_MAX_LIVE_INSTANCES];

static uint32_t forge_text_hash(char const *text)
{
	uint32_t h = 2166136261u;

	while (text && *text)
		h = (h ^ (uint32_t)(unsigned char)*text++) * 16777619u;
	return h;
}

static void forge_decal_take_away(int i, long now)
{
	short k;

	/* (none of a game before this one's: those went with its level) */
	if (s_forge_decals[i].made_time <= now)
	{
		for (k = 0; k < s_forge_decals[i].piece_count; k++)
			decal_delete_made(s_forge_decals[i].pieces[k]);
	}
	s_forge_decals[i].piece_count = 0;
}

/* a decal at a point, down along its up, pointing along its forward (the
player turns it with the spot), the preview's too: its pieces */
static short forge_decal_make(long decal_tag, float x, float y, float z,
	real_vector3d const *up, real_vector3d const *forward, float scale, long *pieces, short maximum)
{
	short count;

	real_point3d origin;
	real_vector3d into;

	origin.x = x + up->i * 0.5f;
	origin.y = y + up->j * 0.5f;
	origin.z = z + up->k * 0.5f;
	into.i = -up->i * 1.5f;
	into.j = -up->j * 1.5f;
	into.k = -up->k * 1.5f;
	decal_capture_begin(pieces, maximum);
	decal_forced_forward = forward;
	decal_new(decal_tag, &origin, &into, scale > 0.0f ? scale : 1.0f, TRUE, NONE, NULL);
	decal_forced_forward = NULL;
	count = decal_capture_end();
	return count;
}

static void forge_decals_update(void)
{
	long now = game_time_get();
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		uint32_t hash;
		int stale;
		short k;

		if (!inst->active || inst->group_tag != 'deca')
		{
			if (s_forge_decals[i].piece_count > 0)
				forge_decal_take_away(i, now);
			s_forge_decals[i].tried = 0;
			continue;
		}
		hash = forge_text_hash(inst->tag_path);
		stale = !s_forge_decals[i].tried || s_forge_decals[i].made_time > now ||
			s_forge_decals[i].path_hash != hash || s_forge_decals[i].x != inst->x || s_forge_decals[i].y != inst->y ||
			s_forge_decals[i].z != inst->z || s_forge_decals[i].yaw != inst->yaw ||
			s_forge_decals[i].pitch != inst->pitch || s_forge_decals[i].roll != inst->roll ||
			s_forge_decals[i].scale != inst->scale;
		for (k = 0; !stale && k < s_forge_decals[i].piece_count; k++)
			stale = !decal_exists(s_forge_decals[i].pieces[k]);
		if (!stale)
			continue;
		/* (one held is made again as it settles, not every frame it moves) */
		if (i == g_forge.grabbed_live_index && s_forge_decals[i].piece_count > 0 && (now & 7) != 0)
			continue;
		forge_decal_take_away(i, now);
		{
			long decal_tag = forge_resolve_engine_tag('deca', inst->tag_path, NULL);
			real_vector3d forward, up;

			s_forge_decals[i].made_time = now;
			s_forge_decals[i].tried = 1;
			s_forge_decals[i].path_hash = hash;
			s_forge_decals[i].x = inst->x;
			s_forge_decals[i].y = inst->y;
			s_forge_decals[i].z = inst->z;
			s_forge_decals[i].yaw = inst->yaw;
			s_forge_decals[i].pitch = inst->pitch;
			s_forge_decals[i].roll = inst->roll;
			s_forge_decals[i].scale = inst->scale;
			if (decal_tag == NONE)
			{
				/* (none to make: a piece count that says it was tried) */
				s_forge_decals[i].piece_count = 0;
				continue;
			}
			euler_to_forward_up(inst->yaw, inst->pitch, inst->roll, &forward, &up);
			s_forge_decals[i].piece_count = forge_decal_make(decal_tag, inst->x, inst->y, inst->z, &up, &forward,
				forge_inst_scale(inst), s_forge_decals[i].pieces, FORGE_DECAL_PIECES);
		}
	}
}

/* ---------- The palette's live preview: EFFECTS' and DECALS' rows shown as
themselves in the world, this machine's alone, while the row is chosen (the
model preview, forge_runtime_tags.c's, shows none of them): an effect played
in front of the camera again and again, an emitter there with its particles,
a decal on the surface looked at */

static struct
{
	int catalog_idx;
	unsigned long since_ms;
	long object_index;
	long next_effect_tick;
	long pieces[FORGE_DECAL_PIECES];
	short piece_count;
	long made_time;
	real_point3d decal_point;
} s_live_preview = { -1, 0, NONE, 0 };

static void forge_palette_live_preview_clear(void)
{
	long now = game_time_get();
	short k;

	if (s_live_preview.object_index != NONE &&
		object_try_and_get_and_verify_type(s_live_preview.object_index, _object_mask_scenery))
	{
		object_delete(s_live_preview.object_index);
	}
	s_live_preview.object_index = NONE;
	if (s_live_preview.made_time <= now)
	{
		for (k = 0; k < s_live_preview.piece_count; k++)
			decal_delete_made(s_live_preview.pieces[k]);
	}
	s_live_preview.piece_count = 0;
	s_live_preview.next_effect_tick = 0;
}

static void forge_palette_live_preview_update(void)
{
	int idx = -1;
	unsigned long now_ms = (unsigned long)system_milliseconds();
	long now = game_time_get();
	struct forge_catalog_entry const *entry;
	real_vector3d forward, up, right;
	real_point3d point;

	if (g_forge_session_enabled && g_forge.active_modal == _forge_modal_spawn_palette &&
		(g_forge.current_category == _forge_cat_effects || g_forge.current_category == _forge_cat_decals))
	{
		int filtered[512];
		int count = get_filtered_catalog_indices(filtered, 512);

		if (g_forge.palette_cursor >= 0 && g_forge.palette_cursor < count &&
			g_forge.catalog[filtered[g_forge.palette_cursor]].group_tag != FORGE_GROUP_FOLDER)
		{
			idx = filtered[g_forge.palette_cursor];
		}
	}
	if (idx != s_live_preview.catalog_idx)
	{
		forge_palette_live_preview_clear();
		s_live_preview.catalog_idx = idx;
		s_live_preview.since_ms = now_ms;
	}
	/* (a row passed over while scrolling reads no tag) */
	if (idx < 0 || now_ms - s_live_preview.since_ms < 250u)
		return;
	entry = &g_forge.catalog[idx];

	euler_to_forward_up(g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll, &forward, &up);
	cross_product3d(&forward, &up, &right);
	/* (ahead, and to the side of the palette) */
	point.x = g_forge.cam_x + forward.i * 3.2f + right.i * 1.0f - up.i * 0.3f;
	point.y = g_forge.cam_y + forward.j * 3.2f + right.j * 1.0f - up.j * 0.3f;
	point.z = g_forge.cam_z + forward.k * 3.2f + right.k * 1.0f - up.k * 0.3f;

	if (entry->group_tag == 'effe')
	{
		if (now >= s_live_preview.next_effect_tick || s_live_preview.next_effect_tick > now + 2 * TICKS_PER_SECOND)
		{
			long effect_tag = forge_resolve_engine_tag('effe', entry->tag_path, NULL);
			char const *marker_names[1] = { "" };
			real_vector3d marker_forwards[1];

			s_live_preview.next_effect_tick = now + (long)(TICKS_PER_SECOND * 3 / 2);
			if (effect_tag != NONE)
			{
				marker_forwards[0] = *global_up3d;
				skulls_effect_harmless = TRUE;
				effect_new_unattached_from_markers(effect_tag, NONE, NULL, 1, marker_names, &point, marker_forwards,
					0.0f, 0.0f, NULL, NULL, TRUE);
				skulls_effect_harmless = FALSE;
			}
		}
	}
	else if (entry->group_tag == 'scen')
	{
		real_vector3d level_forward = forward;

		level_forward.k = 0.0f;
		if (normalize3d(&level_forward) <= 0.0f)
			level_forward = *global_forward3d;
		if (s_live_preview.object_index == NONE)
		{
			uint32_t actual = 'scen';
			long tag = forge_resolve_engine_tag('scen', entry->tag_path, &actual);

			if (tag != NONE)
			{
				struct object_placement_data placement;

				object_placement_data_new(&placement, tag, NONE);
				placement.position = point;
				placement.forward = level_forward;
				placement.up = *global_up3d;
				s_live_preview.object_index = object_new(&placement);
				if (s_live_preview.object_index != NONE)
				{
					SET_FLAG(object_get(s_live_preview.object_index)->object.flags, _object_cannot_be_garbage_bit, TRUE);
					object_set_automatic_deactivation(s_live_preview.object_index, FALSE);
				}
			}
			if (s_live_preview.object_index == NONE)
				s_live_preview.catalog_idx = -2; /* (tried: none) */
		}
		else if (object_try_and_get_and_verify_type(s_live_preview.object_index, _object_mask_scenery))
		{
			/* (it stays in front of the camera) */
			object_set_position(s_live_preview.object_index, &point, &level_forward, global_up3d);
		}
	}
	else if (entry->group_tag == 'deca')
	{
		struct collision_result coll;
		real_point3d from;
		real_vector3d along;

		from.x = g_forge.cam_x;
		from.y = g_forge.cam_y;
		from.z = g_forge.cam_z;
		along.i = forward.i * 20.0f;
		along.j = forward.j * 20.0f;
		along.k = forward.k * 20.0f;
		if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
				&from, &along, NONE, &coll) &&
			(s_live_preview.piece_count == 0 || distance3d(&coll.point, &s_live_preview.decal_point) > 0.75f))
		{
			long decal_tag = forge_resolve_engine_tag('deca', entry->tag_path, NULL);
			real_vector3d normal;
			short k;

			if (s_live_preview.made_time <= now)
			{
				for (k = 0; k < s_live_preview.piece_count; k++)
					decal_delete_made(s_live_preview.pieces[k]);
			}
			s_live_preview.piece_count = 0;
			s_live_preview.decal_point = coll.point;
			s_live_preview.made_time = now;
			normal.i = coll.plane.n.i;
			normal.j = coll.plane.n.j;
			normal.k = coll.plane.n.k;
			if (decal_tag != NONE)
			{
				s_live_preview.piece_count = forge_decal_make(decal_tag, coll.point.x, coll.point.y, coll.point.z,
					&normal, &forward, 1.0f, s_live_preview.pieces, FORGE_DECAL_PIECES);
			}
		}
	}
}

static int forge_ff_route_free_id(void)
{
	int id, i;

	for (id = 1; id <= FF_ROUTE_MAX_ID; id++)
	{
		int used = 0;

		for (i = 0; i < g_forge.live_instance_count && !used; i++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[i];

			used = (forge_ff_is_dropship_zone(inst) || forge_ff_is_route_point(inst)) &&
				FF_ROUTE_ID(inst->team_designator) == id;
		}
		if (!used)
			return id;
	}
	return 0;
}

/* a leg's points as live indices, in flying order */
static int forge_ff_route_leg_points(int route_id, int leg, int32_t *out_points)
{
	int count = 0;
	int i, slot;

	for (i = 0; i < g_forge.live_instance_count && count < FF_ROUTE_MAX_LEG_POINTS; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int order;

		if (!forge_ff_is_route_point(inst) || FF_ROUTE_ID(inst->team_designator) != route_id ||
			FF_ROUTE_POINT_LEG(inst->team_designator) != leg)
		{
			continue;
		}
		order = FF_ROUTE_POINT_ORDER(inst->team_designator);
		for (slot = count; slot > 0 &&
			FF_ROUTE_POINT_ORDER(g_forge.live_instances[out_points[slot - 1]].team_designator) > order; slot--)
		{
			out_points[slot] = out_points[slot - 1];
		}
		out_points[slot] = i;
		count++;
	}
	return count;
}

static int forge_ff_route_point_count(int route_id)
{
	int32_t points[FF_ROUTE_MAX_LEG_POINTS];

	return forge_ff_route_leg_points(route_id, FF_ROUTE_APPROACH, points) +
		forge_ff_route_leg_points(route_id, FF_ROUTE_DEPARTURE, points);
}

static int32_t forge_ff_route_zone(int route_id)
{
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (forge_ff_is_dropship_zone(&g_forge.live_instances[i]) &&
			FF_ROUTE_ID(g_forge.live_instances[i].team_designator) == route_id)
		{
			return i;
		}
	}
	return -1;
}

/* every point of a leg has its style: 0 for a leg without any */
static int forge_ff_route_leg_style(int route_id, int leg)
{
	int32_t points[FF_ROUTE_MAX_LEG_POINTS];

	return forge_ff_route_leg_points(route_id, leg, points) > 0 ?
		FF_ROUTE_POINT_STYLE(g_forge.live_instances[points[0]].team_designator) : 0;
}

static void forge_ff_route_set_leg_style(int route_id, int leg, int style)
{
	int32_t points[FF_ROUTE_MAX_LEG_POINTS];
	int count = forge_ff_route_leg_points(route_id, leg, points);
	int k;

	for (k = 0; k < count; k++)
	{
		int order = FF_ROUTE_POINT_ORDER(g_forge.live_instances[points[k]].team_designator);

		forge_ff_marker_set_usage(points[k], FF_ROUTE_POINT_USAGE(route_id, leg, order, style));
	}
}

/* the route as it will be flown: what the spawner builds from the same markers,
with its styles' tails when it's for showing, not for laying out points */
static void forge_ff_route_legs(struct forge_live_instance const *zone, struct ff_route_leg *legs, int with_tails)
{
	int route_id = FF_ROUTE_ID(zone->team_designator);
	int styles[FF_ROUTE_LEG_COUNT];
	int leg;

	memset(legs, 0, sizeof(*legs) * FF_ROUTE_LEG_COUNT);
	for (leg = 0; leg < FF_ROUTE_LEG_COUNT; leg++)
	{
		int32_t points[FF_ROUTE_MAX_LEG_POINTS];
		int count = forge_ff_route_leg_points(route_id, leg, points);
		int k;

		for (k = 0; k < count; k++)
		{
			struct forge_live_instance const *point = &g_forge.live_instances[points[k]];

			ff_route_leg_add(&legs[leg], k, point->x, point->y, point->z);
		}
		styles[leg] = forge_ff_route_leg_style(route_id, leg);
	}
	ff_route_legs_finish(legs, zone->x, zone->y, zone->z, zone->yaw, with_tails ? styles : NULL);
}

/* renumbers a leg from 0 and leaves the rank free for a new point: 0 when it's full */
static int forge_ff_route_make_room(int route_id, int leg, int rank)
{
	int32_t points[FF_ROUTE_MAX_LEG_POINTS];
	int count = forge_ff_route_leg_points(route_id, leg, points);
	int style = forge_ff_route_leg_style(route_id, leg);
	int k;

	if (count >= FF_ROUTE_MAX_LEG_POINTS)
		return 0;
	for (k = 0; k < count; k++)
		forge_ff_marker_set_usage(points[k], FF_ROUTE_POINT_USAGE(route_id, leg, k < rank ? k : k + 1, style));
	return 1;
}

static int32_t forge_ff_route_spawn_point(
	struct forge_live_instance const *zone,
	int route_id,
	int leg,
	int rank,
	struct ff_route_point const *where)
{
	return forge_3d_spawn_object_at(
		'ffsp',
		"spawn\\firefight_spawn",
		zone->source_cache_id,
		where->x, where->y, where->z,
		zone->yaw, 0.0f, 0.0f,
		FF_ROUTE_POINT_USAGE(route_id, leg, rank, forge_ff_route_leg_style(route_id, leg)),
		1);
}

/* gives a zone a route if it has none, and each empty leg a point to drag from there */
static int forge_ff_route_fill(int32_t zone_idx)
{
	struct forge_live_instance *zone = &g_forge.live_instances[zone_idx];
	int route_id = FF_ROUTE_ID(zone->team_designator);
	int added = 0;
	int leg;

	if (route_id == 0)
	{
		route_id = forge_ff_route_free_id();
		if (route_id == 0)
		{
			forge_set_status("No Free Dropship Routes (%d in this map)", FF_ROUTE_MAX_ID);
			return 0;
		}
		forge_ff_marker_set_usage(zone_idx, FF_ROUTE_WITH_ID(zone->team_designator, route_id));
	}
	for (leg = 0; leg < FF_ROUTE_LEG_COUNT; leg++)
	{
		int32_t points[FF_ROUTE_MAX_LEG_POINTS];

		if (forge_ff_route_leg_points(route_id, leg, points) == 0)
		{
			struct ff_route_point where = ff_route_default_point(zone->x, zone->y, zone->z, zone->yaw, leg);

			if (forge_ff_route_spawn_point(zone, route_id, leg, 0, &where) >= 0)
				added++;
		}
	}
	return added;
}

static void forge_ff_route_remove_points(int route_id)
{
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (forge_ff_is_route_point(&g_forge.live_instances[i]) &&
			FF_ROUTE_ID(g_forge.live_instances[i].team_designator) == route_id)
		{
			forge_3d_delete_instance(i, 1);
		}
	}
}

/* the points a zone's FLIGHT ROUTE row last took away: a press of left or right
shouldn't cost a shaped route, so the panel can put them back till it's next opened */
static struct
{
	int32_t zone_idx;
	int count;
	struct
	{
		short usage;
		struct ff_route_point where;
	} points[FF_ROUTE_LEG_COUNT * FF_ROUTE_MAX_LEG_POINTS];
} s_route_taken = { -1 };

/* the zone flies the automatic drop and climb */
static void forge_ff_route_set_automatic(int32_t zone_idx)
{
	struct forge_live_instance *zone = &g_forge.live_instances[zone_idx];
	int route_id = FF_ROUTE_ID(zone->team_designator);
	int leg;

	if (route_id == 0)
		return;
	s_route_taken.zone_idx = zone_idx;
	s_route_taken.count = 0;
	for (leg = 0; leg < FF_ROUTE_LEG_COUNT; leg++)
	{
		int32_t points[FF_ROUTE_MAX_LEG_POINTS];
		int count = forge_ff_route_leg_points(route_id, leg, points);
		int k;

		for (k = 0; k < count; k++)
		{
			struct forge_live_instance const *point = &g_forge.live_instances[points[k]];

			s_route_taken.points[s_route_taken.count].usage = point->team_designator;
			s_route_taken.points[s_route_taken.count].where.x = point->x;
			s_route_taken.points[s_route_taken.count].where.y = point->y;
			s_route_taken.points[s_route_taken.count].where.z = point->z;
			s_route_taken.count++;
		}
	}
	forge_ff_marker_set_usage(zone_idx, FF_ROUTE_WITH_ID(zone->team_designator, 0));
	forge_ff_route_remove_points(route_id);
	forge_set_status("Automatic Flight: Straight Down and Back Up");
}

/* the zone flies a route of its own: what it had, or a new one */
static void forge_ff_route_set_custom(int32_t zone_idx)
{
	struct forge_live_instance *zone = &g_forge.live_instances[zone_idx];
	int k;

	if (FF_ROUTE_ID(zone->team_designator) != 0)
		return;
	if (s_route_taken.zone_idx == zone_idx && s_route_taken.count > 0)
	{
		int route_id = forge_ff_route_free_id();

		if (route_id != 0)
		{
			forge_ff_marker_set_usage(zone_idx, FF_ROUTE_WITH_ID(zone->team_designator, route_id));
			for (k = 0; k < s_route_taken.count; k++)
			{
				int usage = s_route_taken.points[k].usage;

				forge_3d_spawn_object_at(
					'ffsp',
					"spawn\\firefight_spawn",
					zone->source_cache_id,
					s_route_taken.points[k].where.x,
					s_route_taken.points[k].where.y,
					s_route_taken.points[k].where.z,
					zone->yaw, 0.0f, 0.0f,
					FF_ROUTE_POINT_USAGE(
						route_id, FF_ROUTE_POINT_LEG(usage), FF_ROUTE_POINT_ORDER(usage), FF_ROUTE_POINT_STYLE(usage)),
					1);
			}
			s_route_taken.count = 0;
			forge_set_status("Dropship Route Restored");
			return;
		}
	}
	if (forge_ff_route_fill(zone_idx) > 0)
	{
		g_forge.route_follow_live_index = zone_idx;
		forge_set_status("Dropship Route Added: Move Its Points to Shape the Flight");
	}
}

static void forge_ff_route_shift(int route_id, float dx, float dy, float dz)
{
	int i;

	if (dx == 0.0f && dy == 0.0f && dz == 0.0f)
		return;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];

		if (forge_ff_is_route_point(inst) && FF_ROUTE_ID(inst->team_designator) == route_id)
		{
			forge_3d_move_instance_ext(
				i,
				inst->x + dx, inst->y + dy, inst->z + dz,
				inst->yaw, inst->pitch, inst->roll,
				0,
				0);
		}
	}
}

/* a new point next to this one, on the curve when there's route on that side */
static void forge_ff_route_insert_point(int32_t point_idx, int after)
{
	struct forge_live_instance const *point = &g_forge.live_instances[point_idx];
	int route_id = FF_ROUTE_ID(point->team_designator);
	int leg_index = FF_ROUTE_POINT_LEG(point->team_designator);
	int32_t zone_idx = forge_ff_route_zone(route_id);
	int32_t points[FF_ROUTE_MAX_LEG_POINTS];
	int count = forge_ff_route_leg_points(route_id, leg_index, points);
	struct ff_route_leg legs[FF_ROUTE_LEG_COUNT];
	struct ff_route_leg const *leg = &legs[leg_index];
	struct ff_route_point where;
	int rank = 0;
	int at, segment;

	if (zone_idx < 0)
	{
		forge_set_status("This Route Has No Enemy Drop Point");
		return;
	}
	while (rank < count && points[rank] != point_idx)
		rank++;

	forge_ff_route_legs(&g_forge.live_instances[zone_idx], legs, 0);
	at = (leg_index == FF_ROUTE_DEPARTURE) ? rank + 1 : rank; /* (the departure starts on the hover) */
	segment = after ? at : at - 1;
	if (segment >= 0 && segment + 1 < leg->count)
	{
		where = ff_route_curve(leg, segment, 0.5f);
	}
	else
	{
		/* Past either end of the route: carries on the way the line was going */
		struct ff_route_point const *from = &leg->points[at];
		struct ff_route_point const *inner = &leg->points[after ? at - 1 : at + 1];

		where.x = from->x + (from->x - inner->x) * 0.5f;
		where.y = from->y + (from->y - inner->y) * 0.5f;
		where.z = from->z + (from->z - inner->z) * 0.5f;
	}

	rank += after ? 1 : 0;
	if (!forge_ff_route_make_room(route_id, leg_index, rank))
	{
		forge_set_status("This Leg of the Route Is Full (%d Points)", FF_ROUTE_MAX_LEG_POINTS);
		return;
	}
	if (forge_ff_route_spawn_point(&g_forge.live_instances[zone_idx], route_id, leg_index, rank, &where) >= 0)
		forge_set_status("Added Route Point");
}

/* TEST DROP: this zone's flight flown now, as Firefight would, by the
Covenant's Spirit and then the humans' Pelican (those of them the point
spawns), the Pelican setting off as the Spirit leaves. The host's: a ship is
a network object, so everyone sees it. TEST FLOOD: Keyes' Flood form grown and
burst there, on this machine alone (scenery isn't sent). Pressed again,
either is called off */
static int s_route_test_flying = 0; /* 1: TEST DROP's ships; 2: TEST FLOOD */
static struct
{
	long next_ship_tag; /* the Pelican, once the Spirit leaves (NONE: none) */
	real_point3d where;
	real facing;
	char const *flying_name;
	char const *next_name;
} s_test_drop = { NONE };

static long forge_ff_test_ship_tag(char const *path)
{
	uint32_t actual_group = 'vehi';

	return forge_resolve_engine_tag('vehi', path, &actual_group);
}

static void forge_ff_test_stop(void)
{
	firefight_dropship_preview_stop();
	firefight_flood_drop_preview_stop();
	s_route_test_flying = 0;
	s_test_drop.next_ship_tag = NONE;
}

static void forge_ff_route_test_flight(int32_t zone_idx)
{
	struct forge_live_instance const *zone = &g_forge.live_instances[zone_idx];
	unsigned kinds = forge_ff_drop_kinds(zone);
	long spirit_tag;
	long pelican_tag;

	if (s_route_test_flying)
	{
		forge_set_status(s_route_test_flying == 2 ? "Test Flood Stopped" : "Test Drop Stopped");
		forge_ff_test_stop();
		return;
	}
	if (game_connection() == _game_connection_network_client)
	{
		forge_set_status("Only the Host Can Fly a Test Drop");
		return;
	}
	spirit_tag = (kinds & FF_DROP_KINDS_COVENANT) ? forge_ff_test_ship_tag("vehicles\\c_dropship\\c_dropship") : NONE;
	pelican_tag = (kinds & FF_DROP_KINDS_HUMAN) ? forge_ff_test_ship_tag("vehicles\\pelican\\pelican") : NONE;
	if (spirit_tag == NONE && pelican_tag == NONE)
	{
		forge_set_status("No Spirit or Pelican Tag Is Loaded in This Map");
		return;
	}

	s_test_drop.where.x = zone->x;
	s_test_drop.where.y = zone->y;
	s_test_drop.where.z = zone->z;
	s_test_drop.facing = zone->yaw;
	s_test_drop.flying_name = spirit_tag != NONE ? "Spirit" : "Pelican";
	s_test_drop.next_name = "Pelican";
	forge_ff_test_stop();
	s_test_drop.next_ship_tag = spirit_tag != NONE ? pelican_tag : NONE;
	if (firefight_dropship_preview_start(&s_test_drop.where, s_test_drop.facing,
		spirit_tag != NONE ? spirit_tag : pelican_tag))
	{
		s_route_test_flying = 1;
		g_forge.active_modal = _forge_modal_none;
		s_props_factions_open = 0;
	}
	else
	{
		s_test_drop.next_ship_tag = NONE;
		forge_set_status("The Test Ship Could Not Be Made");
	}
}

static void forge_ff_zone_test_flood(int32_t zone_idx)
{
	struct forge_live_instance const *zone = &g_forge.live_instances[zone_idx];
	uint32_t actual_group = 'scen';
	real_point3d where;
	long form_tag;
	long burst_tag;
	long puff_tag;

	if (s_route_test_flying)
	{
		forge_set_status(s_route_test_flying == 2 ? "Test Flood Stopped" : "Test Drop Stopped");
		forge_ff_test_stop();
		return;
	}
	form_tag = forge_resolve_engine_tag('scen', FF_FLOOD_DROP_TAG, &actual_group);
	/* (and its burst and the gore it throws, which Firefight's levels have
	copied in and Forge's reads from its pack now, the first of each found) */
	{
		static char const *const bursts[] = {
			"characters\\floodcarrier\\effects\\body destroyed",
			"effects\\flood bit death",
		};
		static char const *const puffs[] = {
			"characters\\flood_captain\\punchspray",
			"effects\\flood bit death",
		};
		int k;

		burst_tag = NONE;
		puff_tag = NONE;
		for (k = 0; k < 2 && burst_tag == NONE; k++)
		{
			uint32_t effect_group = 'effe';

			burst_tag = forge_resolve_engine_tag('effe', bursts[k], &effect_group);
		}
		for (k = 0; k < 2 && puff_tag == NONE; k++)
		{
			uint32_t effect_group = 'effe';

			puff_tag = forge_resolve_engine_tag('effe', puffs[k], &effect_group);
		}
	}
	if (form_tag == NONE)
	{
		forge_set_status("Keyes' Flood Form Is Not Loaded in This Map");
		return;
	}
	where.x = zone->x;
	where.y = zone->y;
	where.z = zone->z;
	forge_ff_test_stop();
	if (firefight_flood_drop_preview_start(&where, zone->yaw, form_tag, burst_tag, puff_tag))
	{
		s_route_test_flying = 2;
		g_forge.active_modal = _forge_modal_none;
		s_props_factions_open = 0;
	}
	else
	{
		forge_set_status("The Flood Form Could Not Be Made");
	}
}

/* lets go FORGE_TEST_DROP_PODS pods round a drop area, as Firefight's drop
would (this machine's alone); pressed again, they're taken away */
static int s_test_dropping = 0;

static void forge_drop_area_test_drop(int32_t live_idx)
{
	struct forge_live_instance const *area = &g_forge.live_instances[live_idx];
	real_point3d where;
	short count;

	if (s_test_dropping)
	{
		firefight_ordnance_preview_stop();
		s_test_dropping = 0;
		forge_set_status("Test Drop Stopped");
		return;
	}
	where.x = area->x;
	where.y = area->y;
	where.z = area->z;
	count = firefight_ordnance_preview_start(&where, area->object_index, FORGE_TEST_DROP_PODS);
	if (count <= 0)
	{
		char reasons[256];

		firefight_ordnance_preview_refusals(reasons, sizeof(reasons));
		forge_set_status("No Pods Could Land Here: %s", reasons);
		return;
	}
	s_test_dropping = 1;
	g_forge.active_modal = _forge_modal_none;
	if (count < FORGE_TEST_DROP_PODS)
		forge_set_status("Test Drop: %d of %d Pods Found Room", (int)count, FORGE_TEST_DROP_PODS);
}

static void forge_drop_area_test_drop_update(void)
{
	short phase;

	if (!s_test_dropping)
		return;
	phase = firefight_ordnance_preview_update();
	if (phase == 0)
	{
		s_test_dropping = 0;
		forge_set_status("Test Drop Finished");
	}
	else if (phase == 1)
	{
		forge_set_status("Test Drop: Falling");
	}
}

static void forge_ff_route_test_flight_update(float dt)
{
	if (s_route_test_flying == 2)
	{
		/* (TEST FLOOD's: Keyes' form, this machine's alone) */
		if (!firefight_flood_drop_preview_update(dt))
		{
			s_route_test_flying = 0;
			forge_set_status("Test Flood Finished");
		}
		else
		{
			forge_set_status("Test Flood: The Flood Is Emerging");
		}
	}
	else if (s_route_test_flying)
	{
		static char const *const phases[] = { "", "Approach", "Hover", "Departure" };
		short phase = firefight_dropship_preview_update(dt);

		/* (the Pelican sets off on the same flight as the Spirit leaves) */
		if (s_test_drop.next_ship_tag != NONE && (phase == 0 || firefight_dropship_preview_departing()))
		{
			long next = s_test_drop.next_ship_tag;

			s_test_drop.next_ship_tag = NONE;
			if (firefight_dropship_preview_start(&s_test_drop.where, s_test_drop.facing, next))
			{
				s_test_drop.flying_name = s_test_drop.next_name;
				phase = FF_ROUTE_PHASE_APPROACH;
			}
		}
		if (phase == 0)
		{
			s_route_test_flying = 0;
			forge_set_status("Test Drop Finished");
		}
		else
		{
			forge_set_status("Test Drop: %s %s", s_test_drop.flying_name, phases[phase]);
		}
	}
}

/* once a frame: the properties panel closing settles what its spawn type asked
for, and a held zone's new route stops following once it's let go */
static int32_t s_route_props_live_idx = -1;

static void forge_ff_routes_update(float dt)
{
	int panel_open = g_forge.active_modal == _forge_modal_object_properties && g_forge.grabbed_live_index >= 0;

	if (!panel_open && s_route_props_live_idx >= 0 && s_route_props_live_idx < g_forge.live_instance_count)
	{
		int32_t live_idx = s_route_props_live_idx;
		struct forge_live_instance *inst = &g_forge.live_instances[live_idx];

		if (inst->active && inst->group_tag == 'ffsp' && !forge_ff_is_route_point(inst) &&
			!forge_ff_is_dropship_zone(inst) && FF_ROUTE_ID(inst->team_designator) != 0)
		{
			int route_id = FF_ROUTE_ID(inst->team_designator);

			forge_ff_marker_set_usage(live_idx, FF_ROUTE_WITH_ID(inst->team_designator, 0));
			forge_ff_route_remove_points(route_id);
		}
	}
	s_route_props_live_idx = panel_open ? g_forge.grabbed_live_index : -1;
	forge_ff_route_test_flight_update(dt);
	forge_drop_area_test_drop_update();

	if (g_forge.route_follow_live_index >= 0 && g_forge.grabbed_live_index != g_forge.route_follow_live_index)
	{
		int32_t zone_idx = g_forge.route_follow_live_index;

		g_forge.route_follow_live_index = -1;
		if (zone_idx < g_forge.live_instance_count && forge_ff_is_dropship_zone(&g_forge.live_instances[zone_idx]))
		{
			int route_id = FF_ROUTE_ID(g_forge.live_instances[zone_idx].team_designator);
			int i;

			for (i = 0; i < g_forge.live_instance_count; i++)
			{
				struct forge_live_instance const *inst = &g_forge.live_instances[i];

				if (forge_ff_is_route_point(inst) && FF_ROUTE_ID(inst->team_designator) == route_id)
				{
					forge_network_send_transform_ext(
						i, inst->object_index, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 1);
				}
			}
		}
	}
}

static int get_filtered_catalog_indices_for_cat(enum forge_palette_category cat, int *out_indices, int max_out)
{
	int i, count = 0;
	for (i = 0; i < g_forge.catalog_count && count < max_out; i++)
	{
		if (g_forge.catalog[i].category == (uint8_t)cat &&
			forge_catalog_entry_allowed(&g_forge.catalog[i]))
		{
			if (out_indices)
				out_indices[count] = i;
			count++;
		}
	}
	return count;
}

/* the Actors tab's level: its groups (NONE), or a group's ranks and RANDOM */
static int s_palette_ai_group = NONE;

/* a split tab's level (forge_catalog_category_split): its origins' folders
(origin NONE), an origin's keyword folders and the tags in none, or a keyword
folder's tags */
static int s_palette_origin = NONE;
static int s_palette_keyword = 0;
/* FIREFIGHT's level: its rows, or FIREFIGHT SPAWNS' kinds of marker */
static int s_palette_ff_menu = 0;

static int forge_palette_is_ff_preset(struct forge_catalog_entry const *row)
{
	return row->group_tag == 'ffsp' && _strnicmp(row->tag_path, FORGE_FF_PRESET_PATH, strlen(FORGE_FF_PRESET_PATH)) == 0;
}

static void forge_palette_folders_reset(void)
{
	s_palette_ff_menu = 0;
	s_palette_ai_group = NONE;
	s_palette_origin = NONE;
	s_palette_keyword = 0;
}

static int forge_palette_keyword_has_folder(int category, int origin, int keyword)
{
	int i;

	for (i = 0; keyword != 0 && i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *row = &g_forge.catalog[i];

		if (row->group_tag == FORGE_GROUP_FOLDER && row->category == category &&
			row->origin == origin && row->keyword == keyword)
		{
			return 1;
		}
	}
	return 0;
}

static int get_filtered_split_indices(int category, int *out_indices, int max_out)
{
	int i, pass, count = 0;

	/* (an origin's keyword folders first, then its tags in none) */
	for (pass = 0; pass < 2; pass++)
	{
		for (i = 0; i < g_forge.catalog_count && count < max_out; i++)
		{
			struct forge_catalog_entry const *row = &g_forge.catalog[i];
			int folder = row->group_tag == FORGE_GROUP_FOLDER;

			if (row->category != category)
				continue;
			if (s_palette_origin == NONE)
			{
				if (pass == 0 && folder && row->keyword == 0)
					out_indices[count++] = i;
				continue;
			}
			if (row->origin != s_palette_origin)
				continue;
			if (s_palette_keyword != 0)
			{
				if (pass == 1 && !folder && row->keyword == s_palette_keyword && forge_catalog_entry_allowed(row))
					out_indices[count++] = i;
				continue;
			}
			if (pass == 0 && folder && row->keyword != 0)
				out_indices[count++] = i;
			else if (pass == 1 && !folder && forge_catalog_entry_allowed(row) &&
				!forge_palette_keyword_has_folder(category, row->origin, row->keyword))
			{
				out_indices[count++] = i;
			}
		}
	}
	return count;
}

static int get_filtered_catalog_indices(int *out_indices, int max_out)
{
	int i, group, count = 0;

	if (forge_catalog_category_split(g_forge.current_category))
		return get_filtered_split_indices(g_forge.current_category, out_indices, max_out);
	/* (FIREFIGHT: its spawns' kinds once FIREFIGHT SPAWNS is open, else the rest) */
	if (g_forge.current_category == _forge_cat_firefight)
	{
		for (i = 0; i < g_forge.catalog_count && count < max_out; i++)
		{
			struct forge_catalog_entry const *row = &g_forge.catalog[i];

			if (row->category == _forge_cat_firefight && forge_catalog_entry_allowed(row) &&
				forge_palette_is_ff_preset(row) == s_palette_ff_menu)
			{
				if (out_indices)
					out_indices[count] = i;
				count++;
			}
		}
		return count;
	}
	if (g_forge.current_category != _forge_cat_ai_squads)
		return get_filtered_catalog_indices_for_cat(g_forge.current_category, out_indices, max_out);
	if (s_palette_ai_group == NONE)
	{
		/* (AI ROUTE first: a route's two points, not an actor) */
		for (i = 0; i < g_forge.catalog_count && count < max_out; i++)
		{
			if (g_forge.catalog[i].group_tag == FORGE_GROUP_AI_ROUTE && forge_catalog_entry_allowed(&g_forge.catalog[i]))
				out_indices[count++] = i;
		}
		for (group = 0; group < forge_ai_group_count(); group++)
		{
			int row_index = -1, ranks = 0;

			for (i = 0; i < g_forge.catalog_count; i++)
			{
				struct forge_catalog_entry const *row = &g_forge.catalog[i];

				if (row->category != _forge_cat_ai_squads || row->group_tag == FORGE_GROUP_AI_ROUTE ||
					forge_ai_row_group(row) != group || !forge_catalog_entry_allowed(row))
				{
					continue;
				}
				if (row->group_tag == FORGE_GROUP_AI_MENU)
					row_index = i;
				else if (!forge_ai_row_is_random(row))
					ranks++;
			}
			/* (a map's own actors go as the map does, and may leave a group empty) */
			if (row_index >= 0 && ranks > 0 && count < max_out)
				out_indices[count++] = row_index;
		}
		return count;
	}
	/* (RANDOM last) */
	for (group = 0; group < 2; group++)
	{
		for (i = 0; i < g_forge.catalog_count && count < max_out; i++)
		{
			struct forge_catalog_entry const *row = &g_forge.catalog[i];

			if (row->group_tag == FORGE_GROUP_AI_SPAWN && forge_ai_row_group(row) == s_palette_ai_group &&
				forge_ai_row_is_random(row) == group && forge_catalog_entry_allowed(row))
			{
				out_indices[count++] = i;
			}
		}
	}
	return count;
}

int forge_palette_filtered_indices(int *out_indices, int max_out)
{
	return get_filtered_catalog_indices(out_indices, max_out);
}

static int s_save_modal_just_opened = 0;

static void update_save_modal_input(void)
{
	static int prev_key[256];
	static int prev_gp_a = 0, prev_gp_b = 0;
	int vk;

	director_inhibit_input(0);
	director_inhibit_facing(0);

	if (s_save_modal_just_opened)
	{
		for (vk = 0; vk < 256; vk++)
			prev_key[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
		prev_gp_a = forge_gp_button_down(FORGE_GP_A);
		prev_gp_b = forge_gp_button_down(FORGE_GP_B);
		s_save_modal_just_opened = 0;
		return;
	}

	for (vk = 'A'; vk <= 'Z'; vk++)
	{
		int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		if (down && !prev_key[vk] && g_forge.save_map_name_len < 48)
		{
			g_forge.save_map_name[g_forge.save_map_name_len++] = (char)(vk - 'A' + 'a');
			g_forge.save_map_name[g_forge.save_map_name_len] = 0;
		}
		prev_key[vk] = down;
	}
	for (vk = '0'; vk <= '9'; vk++)
	{
		int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		if (down && !prev_key[vk] && g_forge.save_map_name_len < 48)
		{
			g_forge.save_map_name[g_forge.save_map_name_len++] = (char)vk;
			g_forge.save_map_name[g_forge.save_map_name_len] = 0;
		}
		prev_key[vk] = down;
	}
	{
		int minus_down = (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0;
		if (minus_down && !prev_key[VK_OEM_MINUS] && g_forge.save_map_name_len < 48)
		{
			g_forge.save_map_name[g_forge.save_map_name_len++] = '_';
			g_forge.save_map_name[g_forge.save_map_name_len] = 0;
		}
		prev_key[VK_OEM_MINUS] = minus_down;
	}
	{
		int bk_down = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
		if (bk_down && !prev_key[VK_BACK] && g_forge.save_map_name_len > 0)
		{
			g_forge.save_map_name[--g_forge.save_map_name_len] = 0;
		}
		prev_key[VK_BACK] = bk_down;
	}
	{
		int esc_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
		int gp_b_down = forge_gp_button_down(FORGE_GP_B);
		if ((esc_down && !prev_key[VK_ESCAPE]) || (gp_b_down && !prev_gp_b))
		{
			g_forge.active_modal = _forge_modal_none;
			prev_key[VK_ESCAPE] = esc_down;
			prev_gp_b = gp_b_down;
			return;
		}
		prev_key[VK_ESCAPE] = esc_down;
		prev_gp_b = gp_b_down;
	}
	{
		int ent_down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
		int gp_a_down = forge_gp_button_down(FORGE_GP_A);
		if ((ent_down && !prev_key[VK_RETURN]) || (gp_a_down && !prev_gp_a))
		{
			forge_ui_save_copy_as(g_forge.save_map_name);
			g_forge.active_modal = _forge_modal_none;
		}
		prev_key[VK_RETURN] = ent_down;
		prev_gp_a = gp_a_down;
	}
}

static int find_valid_bsp_spawn_point(real_point3d *out_pos, float *out_yaw)
{
	struct scenario *scen = global_scenario_try_and_get();
	long i;

	if (!scen)
		return 0;

	/* 1. Standard scenario player starting locations inside the active BSP */
	for (i = 0; i < scen->players.count; i++)
	{
		struct encounter_player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
			&scen->players, i, struct encounter_player_starting_location);
		real_point3d test_pt = loc->position;
		test_pt.z += 0.3f;
		if (scenario_leaf_index_from_point(&test_pt) != NONE)
		{
			*out_pos = loc->position;
			*out_yaw = loc->facing;
			return 1;
		}
	}

	/* 2. Scenario cutscene flags inside the active BSP (useful on SP maps whose start BSP has no player spawn) */
	for (i = 0; i < scen->cutscene_flags.count; i++)
	{
		struct scenario_cutscene_flag *flg = TAG_BLOCK_GET_ELEMENT(
			&scen->cutscene_flags, i, struct scenario_cutscene_flag);
		real_point3d test_pt = flg->position;
		test_pt.z += 0.3f;
		if (scenario_leaf_index_from_point(&test_pt) != NONE)
		{
			*out_pos = flg->position;
			*out_yaw = flg->facing.yaw;
			return 1;
		}
	}

	/* 3. Any placed object inside the active BSP */
	{
		struct object_iterator iter;
		struct object_datum *obj;
		object_iterator_new(&iter, (unsigned long)-1, 0);
		while ((obj = object_iterator_next(&iter)) != NULL)
		{
			real_point3d test_pt = obj->object.position;
			test_pt.z += 0.5f;
			if (scenario_leaf_index_from_point(&test_pt) != NONE)
			{
				*out_pos = test_pt;
				*out_yaw = 0.0f;
				return 1;
			}
		}
	}

	/* 4. Fallback to first player starting location even if leaf test failed */
	if (scen->players.count > 0)
	{
		struct encounter_player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
			&scen->players, 0, struct encounter_player_starting_location);
		*out_pos = loc->position;
		*out_yaw = loc->facing;
		return 1;
	}
	return 0;
}

void forge_init_map_scenario_entities(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	long i;

	if (!scen)
		return;

	if (g_forge.live_instance_count == 0 && !s_saved_markers_fresh)
	{
		if (scen == s_orig_sp_scen && scen->players.address == XBOX_ADDRESS(s_live_sp_buf) && s_orig_sp_addr) /* AE hook */
		{
			scen->players.address = XBOX_ADDRESS(s_orig_sp_addr); /* AE hook */
			scen->players.count = s_orig_sp_count;
		}
		if (scen == s_orig_nf_scen && scen->netgame_flags.address == XBOX_ADDRESS(s_live_nf_buf) && s_orig_nf_addr) /* AE hook */
		{
			scen->netgame_flags.address = XBOX_ADDRESS(s_orig_nf_addr); /* AE hook */
			scen->netgame_flags.count = s_orig_nf_count;
		}
	}

	s_saved_markers_fresh = 0;

	/* 1. Load existing scenario player starting locations (scen->players) into g_forge.live_instances
	 *    so Red Team, Blue Team, and FFA player spawns appear as coloured ground circles + facing arrows
	 *    and can be grabbed, moved, rotated, deleted, or team-edited on any map! */
	for (i = 0; i < scen->players.count && g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES; i++)
	{
		struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
			&scen->players, i, struct player_starting_location);
		struct forge_live_instance *inst;
		int16_t t0 = loc->game_types[0];
		int16_t t1 = loc->game_types[1];
		int16_t team_desig;
		char const *label_path;

		if (t0 == 13 /* _game_engine_all_non_team (FFA) */ ||
			(t0 == 0 && t1 == 0) ||
			(loc->team_index < 0 || loc->team_index >= 2))
		{
			team_desig = 2; /* FFA Spawn - Yellow */
			label_path = "spawn\\player_ffa";
		}
		else if (loc->team_index == 1)
		{
			team_desig = 1; /* Blue Team Spawn - Blue */
			label_path = "spawn\\player_blue";
		}
		else
		{
			team_desig = 0; /* Red Team Spawn - Red */
			label_path = "spawn\\player_red";
		}

		inst = &g_forge.live_instances[g_forge.live_instance_count++];
		memset(inst, 0, sizeof(*inst));
		inst->active = 1;
		inst->group_tag = 'spwn';
		inst->source_cache_id = 0;
		inst->is_default_scenario_object = 1;
		inst->pad_inst = (uint8_t)(loc->structure_bsp_reference_index >= 0 ? loc->structure_bsp_reference_index : 0);
		inst->edit_entry_index = -1;
		inst->netgame_flag_type = t0;
		inst->team_designator = team_desig;
		inst->gametype_flags = 0x0FFFu;
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", label_path);
		inst->orig_x = loc->position.x;
		inst->orig_y = loc->position.y;
		inst->orig_z = loc->position.z;
		inst->x = loc->position.x;
		inst->y = loc->position.y;
		inst->z = loc->position.z;
		inst->yaw = loc->facing;
		inst->object_index = NONE;
	}
	sync_live_player_spawns_to_scenario();

	/* 2. Load existing scenario netgame_flags (Teleporters In/Out, KOTH Hills, Oddballs, CTF Flags, Race Nodes)
	 *    into g_forge.live_instances so they can be grabbed, moved, rotated, deleted, and linked via Reference ID! */
	for (i = 0; i < scen->netgame_flags.count && g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES; i++)
	{
		struct scenario_netgame_flag *flg = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_flags, i, struct scenario_netgame_flag);
		struct forge_live_instance *inst;
		char const *label_path = NULL;
		uint32_t grp = 'flag';

		if (flg->type == _netgame_flag_teleporter_source)
		{
			grp = 'tele';
			label_path = "teleporter\\origin";
		}
		else if (flg->type == _netgame_flag_teleporter_target)
		{
			grp = 'tele';
			label_path = "teleporter\\destination";
		}
		else if (flg->type == _netgame_flag_hill)
		{
			grp = 'flag';
			label_path = "objective\\koth_hill";
		}
		else if (flg->type == _netgame_flag_oddball_ball_spawn)
		{
			grp = 'flag';
			label_path = "objective\\oddball_spawn";
		}
		else if (flg->type == _netgame_flag_ctf_flag)
		{
			grp = 'flag';
			label_path = (flg->team_index == 1) ? "objective\\ctf_flag_blue" : "objective\\ctf_flag_red";
		}
		else if (flg->type == _netgame_flag_ctf_vehicle)
		{
			grp = 'flag';
			label_path = (flg->team_index == 1) ? "objective\\ctf_vehicle_blue" : "objective\\ctf_vehicle_red";
		}
		else if (flg->type == _netgame_flag_race_track)
		{
			grp = 'flag';
			label_path = "objective\\race_track";
		}
		else if (flg->type == _netgame_flag_race_vehicle)
		{
			grp = 'flag';
			label_path = "objective\\race_vehicle";
		}
		else if (flg->type == FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN)
		{
			grp = 'ffsp';
			label_path = "spawn\\firefight_spawn";
		}
		else if (flg->type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR)
		{
			grp = 'flag';
			label_path = "objective\\firefight_generator";
		}
		else
		{
			grp = 'flag';
			label_path = "objective\\netgame_flag";
		}

		inst = &g_forge.live_instances[g_forge.live_instance_count++];
		memset(inst, 0, sizeof(*inst));
		inst->active = 1;
		inst->group_tag = grp;
		inst->source_cache_id = 0;
		inst->is_default_scenario_object = 1;
		inst->edit_entry_index = -1;
		inst->netgame_flag_type = flg->type;
		inst->team_designator = flg->team_index;
		inst->gametype_flags = 0x0FFFu;
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", label_path);
		inst->orig_x = flg->position.x;
		inst->orig_y = flg->position.y;
		inst->orig_z = flg->position.z;
		inst->x = flg->position.x;
		inst->y = flg->position.y;
		inst->z = flg->position.z;
		inst->yaw = flg->facing;
		inst->object_index = NONE;
	}
	sync_live_netgame_flags_to_scenario();

	/* 2b. The AI's starting locations, as spawn markers */
	forge_ai_markers_scan();

	/* 3. Deterministically register all default non-networked scenario objects (scenery, machines,
	 *    controls, light fixtures) in object_header_data slot order so Host and Clients share the
	 *    exact same live_idx for every in-map scenery/device object before any player edits occur! */
	if (object_header_data)
	{
		unsigned long default_mask =
			_object_mask_scenery |
			_object_mask_machine |
			_object_mask_control |
			_object_mask_light_fixture;
		long o_idx;
		for (o_idx = data_next_index(object_header_data, NONE);
			 o_idx != NONE && g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES;
			 o_idx = data_next_index(object_header_data, o_idx))
		{
			struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(o_idx, default_mask);
			struct forge_live_instance *inst;
			long def_idx;
			uint32_t grp;
			char const *tname;
			int already = 0, k;

			if (!obj || obj->object.parent_object_index != NONE)
				continue;

			for (k = 0; k < g_forge.live_instance_count; k++)
			{
				if (g_forge.live_instances[k].active && g_forge.live_instances[k].object_index == o_idx)
				{
					already = 1;
					break;
				}
			}
			if (already)
				continue;

			def_idx = obj->definition_index;
			grp = (def_idx != NONE) ? (uint32_t)tag_get_group_tag(def_idx) : 'scen';
			tname = (def_idx != NONE) ? tag_get_name(def_idx) : "scenario_object";
			if (!tname || !tname[0])
				tname = "scenario_object";

			inst = &g_forge.live_instances[g_forge.live_instance_count++];
			memset(inst, 0, sizeof(*inst));
			inst->active = 1;
			inst->object_index = o_idx;
			inst->actor_index = NONE;
			inst->edit_entry_index = -1;
			inst->group_tag = grp;
			inst->source_cache_id = 0;
			inst->is_default_scenario_object = 1;
			inst->orig_x = obj->object.position.x;
			inst->orig_y = obj->object.position.y;
			inst->orig_z = obj->object.position.z;
			inst->x = obj->object.position.x;
			inst->y = obj->object.position.y;
			inst->z = obj->object.position.z;
			forward_up_to_euler(&obj->object.forward, &obj->object.up, &inst->yaw, &inst->pitch, &inst->roll);
			snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tname);
		}
	}

	/* 3b. A map's own teleporters: a node on a teleporter's scenery is that
	scenery's from now on, and goes where it is moved (forge_tele_absorb_node) */
	for (i = 0; i < g_forge.live_instance_count; i++)
		forge_tele_absorb_node((int32_t)i);

	/* 2. Points 7 & 13: In Single Player mode (!game_engine_running()), the engine skips spawning
	 *    netgame_equipment (weapons/grenades/powerups) and multiplayer-flagged vehicles.
	 *    Force every weapon, item, and vehicle present on the map to spawn! */
	if (g_forge_session_enabled && !game_engine_running())
	{
		for (i = 0; i < scen->netgame_equipment.count; i++)
		{
			struct scenario_netgame_equipment *eq = TAG_BLOCK_GET_ELEMENT(
				&scen->netgame_equipment, i, struct scenario_netgame_equipment);
			long item_def_idx = NONE;

			if (eq->item_collection.index != NONE)
			{
				uint8_t const *itmc = (uint8_t const *)tag_get('itmc', eq->item_collection.index);
				if (itmc)
				{
					int32_t perm_count = *(int32_t const *)(itmc + 0x00);
					uint8_t const *perms = XBOX_POINTER(uint8_t const, *(uint32_t const *)(itmc + 0x04)); /* AE hook */
					if (perm_count > 0 && perms)
					{
						/* item_permutation is 84 bytes; item tag_reference is at +0x24 (index at +0x30) */
						item_def_idx = *(int32_t const *)(perms + 0x30);
					}
				}
			}

			if (item_def_idx != NONE)
			{
				struct object_placement_data pdata;
				long oidx;
				object_placement_data_new(&pdata, item_def_idx, NONE);
				pdata.position = eq->position;
				pdata.position.z += 0.08f;
				euler_to_forward_up(eq->facing, 0.0f, 0.0f, &pdata.forward, &pdata.up);
				oidx = object_new(&pdata);
				if (oidx != NONE)
				{
					object_set_automatic_deactivation(oidx, FALSE);
					object_activate(oidx);
				}
			}
		}

		/* Also ensure all scenario vehicles are spawned if the single-player game skipped any MP-flagged ones */
		for (i = 0; i < scen->vehicles.count; i++)
		{
			struct scenario_placeholder_datum *sv = TAG_BLOCK_GET_ELEMENT(
				&scen->vehicles, i, struct scenario_placeholder_datum);
			if (sv->object.palette_entry_index >= 0 && sv->object.palette_entry_index < scen->vehicle_palette.count)
			{
				struct scenario_object_palette_entry *pal = TAG_BLOCK_GET_ELEMENT(
					&scen->vehicle_palette, sv->object.palette_entry_index, struct scenario_object_palette_entry);
				if (pal->reference.index != NONE)
				{
					/* Check if a vehicle already exists within 1.5 world units of this scenario spawn */
					struct object_iterator viter;
					struct object_datum *vobj;
					int already_spawned = 0;
					object_iterator_new(&viter, _object_mask_vehicle, 0);
					while ((vobj = object_iterator_next(&viter)) != NULL)
					{
						float dx = vobj->object.position.x - sv->object.position.x;
						float dy = vobj->object.position.y - sv->object.position.y;
						float dz = vobj->object.position.z - sv->object.position.z;
						if (dx * dx + dy * dy + dz * dz < 2.25f)
						{
							already_spawned = 1;
							break;
						}
					}
					if (!already_spawned)
					{
						struct object_placement_data pdata;
						long v_idx;
						object_placement_data_new(&pdata, pal->reference.index, NONE);
						pdata.position = sv->object.position;
						pdata.position.z += 0.12f;
						euler_to_forward_up(sv->object.rotation.yaw, sv->object.rotation.pitch, sv->object.rotation.roll, &pdata.forward, &pdata.up);
						v_idx = object_new(&pdata);
						if (v_idx != NONE)
						{
							object_set_automatic_deactivation(v_idx, FALSE);
							object_activate(v_idx);
						}
					}
				}
			}
		}
	}

	/* (the map's deadly trigger volumes: Forge's VOLUMES, forge_volumes.c) */
	forge_volumes_adopt_map();
}

static int count_active_instances_for_catalog_entry(struct forge_catalog_entry const *ce)
{
	int i, count = 0;
	if (!ce || !ce->tag_path[0])
		return 0;
	/* (a FIREFIGHT SPAWNS kind: the markers of its kind; vehicle AI's light
	and heavy both) */
	if (ce->group_tag == 'ffsp' && _strnicmp(ce->tag_path, FORGE_FF_PRESET_PATH, strlen(FORGE_FF_PRESET_PATH)) == 0)
	{
		int kind = forge_ff_spawn_kind(FORGE_FF_UNPACK_TYPE(ce->preset));

		for (i = 0; i < g_forge.live_instance_count; i++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[i];

			if (inst->active && inst->group_tag == 'ffsp' &&
				FORGE_FF_UNPACK_TYPE(inst->team_designator) != FF_ROUTE_SPAWN_TYPE &&
				forge_ff_spawn_kind(FORGE_FF_UNPACK_TYPE(inst->team_designator)) == kind)
			{
				count++;
			}
		}
		return count;
	}
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (g_forge.live_instances[i].active &&
			g_forge.live_instances[i].group_tag == ce->group_tag &&
			_stricmp(g_forge.live_instances[i].tag_path, ce->tag_path) == 0)
		{
			count++;
		}
	}
	return count;
}

static void format_clean_uppercase_item_name(char const *display_name, char *out_buf, size_t out_sz)
{
	char const *p = display_name ? display_name : "";
	size_t i = 0;
	if (!out_buf || out_sz == 0)
		return;
	/* Preserve "[c] " custom .ocetags badge and "[MAP] " local map badge while stripping generic "[WEAP] ", "[VEHI] ", etc. */
	if (_strnicmp(p, "[c]", 3) == 0)
	{
		p += 3;
		while (*p == ' ')
			p++;
		if (out_sz >= 5)
		{
			out_buf[0] = '[';
			out_buf[1] = 'C';
			out_buf[2] = ']';
			out_buf[3] = ' ';
			i = 4;
		}
	}
	else if (_strnicmp(p, "[map]", 5) == 0)
	{
		p += 5;
		while (*p == ' ')
			p++;
		if (out_sz >= 7)
		{
			out_buf[0] = '[';
			out_buf[1] = 'M';
			out_buf[2] = 'A';
			out_buf[3] = 'P';
			out_buf[4] = ']';
			out_buf[5] = ' ';
			i = 6;
		}
	}
	else if (p[0] == '[')
	{
		char const *rb = strchr(p, ']');
		if (rb)
		{
			p = rb + 1;
			while (*p == ' ')
				p++;
		}
	}
	while (*p && i + 1 < out_sz)
	{
		char ch = *p++;
		if (ch >= 'a' && ch <= 'z')
			ch = (char)(ch - 'a' + 'A');
		out_buf[i++] = ch;
	}
	out_buf[i] = '\0';
}

/* whether the object held has Object Properties' SNAP POINTS row: a stock
scenery's or door's tag (a profile's has its page in the Tag Injector) */
/* the palette's tabs in the order shown: the enum's is the pack's palette
file's numbers, so TELEPORTERS, added last, shows after SPAWNS */
static enum forge_palette_category forge_palette_category_step(enum forge_palette_category category, int dir)
{
	static enum forge_palette_category const s_order[NUMBER_OF_FORGE_PALETTE_CATEGORIES] = {
		_forge_cat_weapons,
		_forge_cat_equipment,
		_forge_cat_vehicles,
		_forge_cat_bipeds,
		_forge_cat_scenery,
		_forge_cat_effects,
		_forge_cat_decals,
		_forge_cat_barriers_devices,
		_forge_cat_lights,
		_forge_cat_spawns,
		_forge_cat_teleporters,
		_forge_cat_objectives,
		_forge_cat_ai_squads,
		_forge_cat_firefight,
		_forge_cat_volumes
	};
	int i;

	for (i = 0; i < NUMBER_OF_FORGE_PALETTE_CATEGORIES; i++)
	{
		if (s_order[i] == category)
			return s_order[(i + NUMBER_OF_FORGE_PALETTE_CATEGORIES + dir) % NUMBER_OF_FORGE_PALETTE_CATEGORIES];
	}
	return _forge_cat_weapons;
}

static int forge_props_snap_row(struct forge_live_instance const *inst)
{
	return (inst->group_tag == 'scen' || inst->group_tag == 'mach' || inst->group_tag == 'ctrl') &&
		forge_snap_points_stock_tag(inst->group_tag, inst->tag_path, NULL, 0);
}

/* Object Settings' ALWAYS SPAWN row: a placed vehicle's (the map's own are
the gametype's, as its placements say), after DELETE OBJECT; else -1 */
static int forge_props_always_spawn_row(struct forge_live_instance const *inst, int snap_row, int test_drop_row)
{
	return inst->group_tag == 'vehi' && !inst->is_default_scenario_object && !inst->is_ai_unit &&
		!snap_row && test_drop_row < 0 && !forge_tele_is(inst) && !forge_ff_is_dropship_zone(inst) &&
		!forge_ff_is_route_point(inst) ? 5 : -1;
}

/* a placed scenery's or effect's SCALE row, after its other rows (-1: none) */
static int forge_props_scale_row(struct forge_live_instance const *inst, int snap_row, int test_drop_row)
{
	if ((inst->group_tag != 'scen' && inst->group_tag != 'deca') || inst->is_default_scenario_object ||
		inst->is_ai_unit || inst->edit_entry_index < 0 || forge_tele_is(inst))
	{
		return -1;
	}
	return test_drop_row >= 0 ? test_drop_row + 1 : snap_row ? 6 : 5;
}

static float forge_inst_scale(struct forge_live_instance const *inst)
{
	return inst->scale > 0.0f ? inst->scale : 1.0f;
}

/* SCALE's next size: tenths up to 2, halves to 5, wholes past */
static float forge_scale_step(float scale, int dir)
{
	float step = scale < 2.0f - 0.001f || (dir < 0 && scale <= 2.0f + 0.001f) ? 0.1f :
		scale < 5.0f - 0.001f || (dir < 0 && scale <= 5.0f + 0.001f) ? 0.5f : 1.0f;
	float next = scale + step * (float)dir;

	/* (back onto its step's grid) */
	next = (float)floor((double)(next / step) + 0.5) * step;
	return next < FORGE_SCALE_MIN ? FORGE_SCALE_MIN : next > FORGE_SCALE_MAX ? FORGE_SCALE_MAX : next;
}

/* a placed scenery's or effect's SCALE set (the Object Settings row, a peer's
!FZ:, a saved map's OBJECT_SCALE): kept in the map's record for the save */
void forge_3d_scale_apply(int32_t live_idx, float scale)
{
	struct forge_live_instance *inst;
	struct object_datum *obj;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	/* (a decal's: the size it is made again at, forge_decals_update) */
	if (!inst->active || (inst->group_tag != 'scen' && inst->group_tag != 'deca'))
		return;
	scale = scale < FORGE_SCALE_MIN ? FORGE_SCALE_MIN : scale > FORGE_SCALE_MAX ? FORGE_SCALE_MAX : scale;
	inst->scale = fabs((double)scale - 1.0) > 0.001 ? scale : 0.0f;
	if (inst->edit_entry_index >= 0 && inst->edit_entry_index < g_forge.edit_state.placed_object_count)
		g_forge.edit_state.placed_objects[inst->edit_entry_index].scale = inst->scale;
	obj = forge_valid_object_get(inst->object_index);
	if (obj)
	{
		union real_point3d position = obj->object.position;
		union real_vector3d forward = obj->object.forward;
		union real_vector3d up = obj->object.up;

		obj->object.scale = forge_inst_scale(inst);
		object_set_position(inst->object_index, &position, &forward, &up);
	}
}

/* a placed vehicle's ALWAYS SPAWN set (the Object Settings row, a peer's
!FV:): kept in the map's record for the save */
void forge_3d_always_spawn_apply(int32_t live_idx, int on)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active || inst->group_tag != 'vehi' || inst->is_default_scenario_object)
		return;
	inst->always_spawn = (uint8_t)(on != 0);
	if (inst->edit_entry_index >= 0 && inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
		g_forge.edit_state.placed_objects[inst->edit_entry_index].always_spawn = inst->always_spawn;
}

/* a placed emitter's DEAL DAMAGE set (the Object Settings row, a peer's
!FN:, a saved map's OBJECT_NO_DAMAGE): kept in the map's record for the save */
void forge_3d_no_damage_apply(int32_t live_idx, int no_damage)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active || inst->group_tag != 'scen')
		return;
	inst->no_damage = (uint8_t)(no_damage != 0);
	if (inst->edit_entry_index >= 0 && inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
		g_forge.edit_state.placed_objects[inst->edit_entry_index].no_damage = inst->no_damage;
}

/* a placed scenery's COLLISION set (the Object Settings row, a peer's !FK:, a
saved map's OBJECT_NO_COLLISION): kept in the map's record for the save, and
on its object now (held, it collides with nothing anyway) */
void forge_3d_no_collision_apply(int32_t live_idx, int no_collision)
{
	struct forge_live_instance *inst;
	struct object_datum *obj;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (!inst->active || inst->group_tag != 'scen')
		return;
	inst->no_collision = (uint8_t)(no_collision != 0);
	if (inst->edit_entry_index >= 0 && inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
		g_forge.edit_state.placed_objects[inst->edit_entry_index].no_collision = inst->no_collision;
	obj = forge_valid_object_get(inst->object_index);
	if (obj && live_idx != g_forge.grabbed_live_index)
		SET_FLAG(obj->object.flags, _object_no_collisions_bit, inst->no_collision ? TRUE : FALSE);
}

/* each frame, in Forge and in a match of the map: the scenery with COLLISION
off kept so (a release, a peer's hold and release, a respawn of the object
give it its collision back) */
static void forge_3d_no_collision_update(void)
{
	int32_t i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		struct object_datum *obj;

		if (!inst->active || !inst->no_collision || inst->object_index == NONE)
			continue;
		obj = forge_valid_object_get(inst->object_index);
		if (obj)
			SET_FLAG(obj->object.flags, _object_no_collisions_bit, TRUE);
	}
}

/* Object Settings' COLLISION row: a placed scenery's (not a teleporter's),
after its DEAL DAMAGE, else after its SCALE; else -1 */
static int forge_props_collision_row(struct forge_live_instance const *inst, int scale_row, int damage_row)
{
	if (scale_row < 0 || inst->group_tag != 'scen')
		return -1;
	return (damage_row >= 0 ? damage_row : scale_row) + 1;
}

extern int forge_runtime_is_particle_emitter(uint32_t group_tag, char const *tag_path);

/* Object Settings' DEAL DAMAGE row: a placed emitter's (fire, plasma fire),
or a scenery's whose effects deal damage, after its SCALE row; else -1 */
static int forge_props_damage_row(struct forge_live_instance const *inst, int scale_row)
{
	struct object_datum *obj;

	if (scale_row < 0 || inst->group_tag != 'scen')
		return -1;
	if (inst->no_damage || forge_runtime_is_particle_emitter(inst->group_tag, inst->tag_path))
		return scale_row + 1;
	obj = forge_valid_object_get(inst->object_index);
	return obj && forge_volumes_tag_deals_damage(obj->definition_index) ? scale_row + 1 : -1;
}

static char const *gametype_flags_label(uint16_t flags)
{
	if (flags == 0x0001u) return "CTF Only";
	if (flags == 0x0002u) return "Slayer Only";
	if (flags == 0x0004u) return "Oddball Only";
	if (flags == 0x0008u) return "King of the Hill";
	if (flags == 0x0010u) return "Race Only";
	return "All Modes (Default)";
}

static uint16_t cycle_gametype_flags(uint16_t flags, int dir)
{
	static uint16_t const modes[6] = { 0x0FFFu, 0x0001u, 0x0002u, 0x0004u, 0x0008u, 0x0010u };
	int i, cur = 0;
	for (i = 0; i < 6; i++)
	{
		if (modes[i] == flags)
		{
			cur = i;
			break;
		}
	}
	cur = (cur + 6 + dir) % 6;
	return modes[cur];
}

int forge_has_held_item_to_cancel(void)
{
	return (g_forge_session_enabled &&
			g_forge.mode == _forge_mode_3d_monitor &&
			(g_forge.active_modal != _forge_modal_none ||
			 (g_forge.grabbed_live_index >= 0 &&
			  g_forge.grabbed_live_index < g_forge.live_instance_count &&
			  g_forge.live_instances[g_forge.grabbed_live_index].active)));
}

void forge_cancel_or_release_held_item(void)
{
	if (g_forge.grabbed_live_index >= 0 && g_forge.grabbed_live_index < g_forge.live_instance_count)
	{
		if (g_forge.grabbed_is_new_spawn)
		{
			if (g_forge.koth_group_placing)
			{
				int k;
				for (k = 0; k < 10; k++)
				{
					int32_t nidx = g_forge.koth_group_live_indices[k];
					if (nidx >= 0 && nidx < g_forge.live_instance_count)
						forge_3d_delete_instance(nidx, 1);
				}
				g_forge.koth_group_placing = 0;
			}
			else
			{
				forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
			}
			forge_set_status("Cancelled Item Spawn");
		}
		else
		{
			int32_t restore_live = g_forge.grabbed_live_index;
			long restore_obj = g_forge.grabbed_object_index;
			g_forge.grabbed_live_index = -1;
			g_forge.grabbed_object_index = NONE;
			forge_3d_move_instance_ext(
				restore_live,
				g_forge.grabbed_orig_x,
				g_forge.grabbed_orig_y,
				g_forge.grabbed_orig_z,
				g_forge.grabbed_orig_yaw,
				g_forge.grabbed_orig_pitch,
				g_forge.grabbed_orig_roll,
				0,
				0);
			forge_network_send_transform_ext(
				restore_live,
				restore_obj,
				g_forge.grabbed_orig_x,
				g_forge.grabbed_orig_y,
				g_forge.grabbed_orig_z,
				g_forge.grabbed_orig_yaw,
				g_forge.grabbed_orig_pitch,
				g_forge.grabbed_orig_roll,
				0,
				1);
			/* (its settings may have changed while it was held) */
			forge_history_end(restore_live);
			forge_set_status("Restored Item Position");
		}
	}
	g_forge.grabbed_live_index = -1;
	g_forge.grabbed_object_index = NONE;
	g_forge.grabbed_is_new_spawn = 0;
	g_forge.koth_group_placing = 0;
}

/* what is held put down where it is, as a click puts it */
static void forge_place_held_item(void)
{
	if (g_forge.grabbed_live_index < 0)
		return;
	{
		int32_t released_live_idx = g_forge.grabbed_live_index;
		struct forge_live_instance *released = &g_forge.live_instances[released_live_idx];
		int was_new_spawn = g_forge.grabbed_is_new_spawn;
		g_forge.grabbed_live_index = -1;
		g_forge.grabbed_object_index = NONE;
		g_forge.grabbed_is_new_spawn = 0;

		/* Apply release transform (is_held = 0) so vehicles wake up physics and become drivable immediately on Host & Clients! */
		forge_3d_move_instance_ext(
			released_live_idx,
			released->x,
			released->y,
			released->z,
			released->yaw,
			released->pitch,
			released->roll,
			0,
			0);

		if (g_forge.koth_group_placing)
		{
			int k;
			for (k = 0; k < 10; k++)
			{
				int32_t nidx = g_forge.koth_group_live_indices[k];
				if (nidx >= 0 && nidx < g_forge.live_instance_count && g_forge.live_instances[nidx].active)
				{
					struct forge_live_instance const *ni = &g_forge.live_instances[nidx];
					forge_network_send_transform_ext(nidx, ni->object_index, ni->x, ni->y, ni->z, ni->yaw, ni->pitch, ni->roll, 0, 1);
					/* (the hill's ten, one step of the history) */
					forge_history_note_placed(nidx, k == 0);
				}
			}
			forge_set_status("Placed KOTH Hill #%d", g_forge.koth_group_hill_id);
			g_forge.koth_group_placing = 0;
		}
		else
		{
			forge_network_send_transform_ext(
				released_live_idx,
				released->object_index,
				released->x,
				released->y,
				released->z,
				released->yaw,
				released->pitch,
				released->roll,
				0,
				1);
			if (was_new_spawn)
				forge_history_note_placed(released_live_idx, 1);
			else
				forge_history_end(released_live_idx);
			forge_set_status("Placed '%s'", released->tag_path);
		}
	}
}

/* ---------- where vehicles, weapons and powerups go back to

A vehicle, weapon or powerup has an origin, as an AI spawn has: where it was
placed. Played with in Spartan mode, it stays where it is left, and going into
Forge mode puts it back; one used up, or carried off, is made again there.
The host's, as the objects are. */
static void forge_origin_object_settle(struct forge_live_instance const *inst, int32_t live_idx)
{
	forge_3d_move_instance_ext(live_idx, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 0);
	forge_network_send_transform_ext(live_idx, inst->object_index, inst->x, inst->y, inst->z,
		inst->yaw, inst->pitch, inst->roll, 0, 1);
}

static int forge_origin_group(uint32_t group_tag)
{
	return group_tag == 'vehi' || group_tag == 'weap' || group_tag == 'eqip';
}

/* the map's own, which a game makes itself, have their places as they stand a
moment into the level (FORGE_ORIGINS_NOTE_FRAMES: its objects are all there),
before anyone has moved them */
#define FORGE_ORIGINS_NOTE_FRAMES 30
static int s_origins_noted;
static int s_origins_note_frames;

static void forge_origins_note(void)
{
	long object_index;

	if (s_origins_noted || !global_scenario_try_and_get() || game_connection() == _game_connection_network_client)
		return;
	s_origins_noted = 1;
	for (object_index = data_next_index(object_header_data, NONE);
		 object_index != NONE && g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES;
		 object_index = data_next_index(object_header_data, object_index))
	{
		struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(object_index,
			_object_mask_vehicle | _object_mask_weapon | _object_mask_equipment);
		struct forge_live_instance *inst;
		int i, tracked = 0;

		/* AE: a weapon in a unit's inventory (a vehicle's gun) has no parent and is not connected to the map: it is not a map object to
		track, and moving it would connect it while its unit holds it */
		if (!obj || obj->object.parent_object_index != NONE || obj->definition_index == NONE ||
			!TEST_FLAG(obj->object.flags, _object_connected_to_map_bit))
			continue;
		for (i = 0; i < g_forge.live_instance_count && !tracked; i++)
			tracked = g_forge.live_instances[i].active && g_forge.live_instances[i].object_index == object_index;
		if (tracked)
			continue;
		inst = &g_forge.live_instances[g_forge.live_instance_count++];
		memset(inst, 0, sizeof(*inst));
		inst->active = 1;
		inst->object_index = object_index;
		inst->actor_index = NONE;
		inst->edit_entry_index = -1;
		inst->group_tag = (uint32_t)tag_get_group_tag(obj->definition_index);
		inst->is_default_scenario_object = 1;
		inst->orig_x = inst->x = obj->object.position.x;
		inst->orig_y = inst->y = obj->object.position.y;
		inst->orig_z = inst->z = obj->object.position.z;
		forward_up_to_euler(&obj->object.forward, &obj->object.up, &inst->yaw, &inst->pitch, &inst->roll);
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tag_get_name(obj->definition_index));
	}
}

/* another object of the tag at an origin: a game's own respawn of it */
static long forge_origin_object_there(struct forge_live_instance const *inst, long definition_index)
{
	long object_index;

	for (object_index = data_next_index(object_header_data, NONE); object_index != NONE;
		 object_index = data_next_index(object_header_data, object_index))
	{
		struct object_datum *obj = forge_valid_object_get(object_index);

		/* (AE: one connected to the map: a unit's inventory weapon is not there) */
		if (obj && obj->definition_index == definition_index && obj->object.parent_object_index == NONE &&
			TEST_FLAG(obj->object.flags, _object_connected_to_map_bit))
		{
			float dx = obj->object.position.x - inst->x;
			float dy = obj->object.position.y - inst->y;
			float dz = obj->object.position.z - inst->z;

			if (dx * dx + dy * dy + dz * dz < 1.0f)
				return object_index;
		}
	}
	return NONE;
}

static void forge_origins_reset(void)
{
	int32_t i;

	if (game_connection() == _game_connection_network_client)
		return;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[i];
		struct object_datum *obj;

		if (!inst->active || !forge_origin_group(inst->group_tag))
			continue;
		obj = forge_valid_object_get(inst->object_index);
		/* (one carried stays with whoever has it, and another takes its place) */
		if (!obj || obj->object.parent_object_index != NONE)
		{
			uint32_t actual_group = inst->group_tag;
			long definition_index = forge_resolve_engine_tag(inst->group_tag, inst->tag_path, &actual_group);

			if (definition_index == NONE)
				continue;
			inst->object_index = forge_origin_object_there(inst, definition_index);
			if (inst->object_index == NONE)
			{
				inst->object_index = forge_create_configured_engine_object(definition_index, inst->group_tag,
					inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, inst->always_spawn, inst->scale);
			}
			if (inst->object_index == NONE)
				continue;
		}
		else if (obj->object.type == _object_type_vehicle)
		{
			forge_eject_all_passengers_from_vehicle(inst->object_index);
		}
		forge_origin_object_settle(inst, i);
	}
}

static real_point3d s_last_valid_spartan_pos = { 0.0f, 0.0f, 0.0f };
static int s_has_valid_spartan_pos = 0;

/* Computes a guaranteed-safe Spartan feet position from the Monitor camera position:
 * 1. Pushes horizontally clear of any walls within Spartan cylinder radius (0.38u).
 * 2. Pushes below any low ceiling within 0.25u above eye level.
 * 3. Raycasts down 0.75u: if a floor is closer than 0.75u, places Spartan feet 0.06u ABOVE the floor
 *    (never cam_z - 0.65f underneath the floor!).
 * 4. Verifies BSP leaf validity and falls back to the last known valid Spartan position if needed. */
static void forge_compute_safe_spartan_stand_pos(float cam_x, float cam_y, float cam_z, real_point3d *out_pos)
{
	unsigned long both_flags = FLAG(_collision_test_structure_bit) |
							   FLAG(_collision_test_front_facing_surfaces_bit) |
							   FLAG(_collision_test_back_facing_surfaces_bit);
	real_point3d eye_pt, stand_pt;
	struct collision_result coll, vcoll;
	real_vector3d ray_vec, step_vec;
	int k;

	eye_pt.x = cam_x;
	eye_pt.y = cam_y;
	eye_pt.z = cam_z;

	/* 1. Ensure eye_pt starts inside a valid BSP leaf */
	if (scenario_leaf_index_from_point(&eye_pt) == NONE)
	{
		static float const s_SavedOffsets[18][3] = {
			{ 0.0f, 0.0f, 0.30f }, { 0.0f, 0.0f, -0.30f },
			{ 0.30f, 0.0f, 0.0f }, { -0.30f, 0.0f, 0.0f },
			{ 0.0f, 0.30f, 0.0f }, { 0.0f, -0.30f, 0.0f },
			{ 0.0f, 0.0f, 0.65f }, { 0.0f, 0.0f, -0.65f },
			{ 0.60f, 0.0f, 0.20f }, { -0.60f, 0.0f, 0.20f },
			{ 0.0f, 0.60f, 0.20f }, { 0.0f, -0.60f, 0.20f },
			{ 1.00f, 0.0f, 0.35f }, { -1.00f, 0.0f, 0.35f },
			{ 0.0f, 1.00f, 0.35f }, { 0.0f, -1.00f, 0.35f },
			{ 0.0f, 0.0f, 1.20f }, { 0.0f, 0.0f, -1.20f }
		};
		for (k = 0; k < 18; k++)
		{
			real_point3d probe;
			probe.x = cam_x + s_SavedOffsets[k][0];
			probe.y = cam_y + s_SavedOffsets[k][1];
			probe.z = cam_z + s_SavedOffsets[k][2];
			if (scenario_leaf_index_from_point(&probe) != NONE)
			{
				eye_pt = probe;
				break;
			}
		}
		if (scenario_leaf_index_from_point(&eye_pt) == NONE && s_has_valid_spartan_pos)
		{
			*out_pos = s_last_valid_spartan_pos;
			return;
		}
	}

	/* 2. Push horizontally clear of any nearby walls within Spartan cylinder radius (0.38u) */
	for (k = 0; k < 8; k++)
	{
		float ang = (float)k * 0.785398163f;
		float wall_pad = 0.38f;
		ray_vec.i = (float)cos((double)ang) * wall_pad;
		ray_vec.j = (float)sin((double)ang) * wall_pad;
		ray_vec.k = 0.0f;
		memset(&coll, 0, sizeof(coll));
		if (collision_test_vector(both_flags, &eye_pt, &ray_vec, NONE, &coll))
		{
			float d = coll.t * wall_pad;
			if (d < wall_pad)
			{
				float push = (wall_pad - d) + 0.02f;
				real_point3d cand = eye_pt;
				cand.x += coll.plane.n.i * push;
				cand.y += coll.plane.n.j * push;
				step_vec.i = cand.x - eye_pt.x;
				step_vec.j = cand.y - eye_pt.y;
				step_vec.k = 0.0f;
				memset(&vcoll, 0, sizeof(vcoll));
				if (!collision_test_vector(both_flags, &eye_pt, &step_vec, NONE, &vcoll) &&
					scenario_leaf_index_from_point(&cand) != NONE)
				{
					eye_pt = cand;
				}
			}
		}
	}

	/* 3. Push below any low ceiling directly above eye_pt */
	ray_vec.i = 0.0f;
	ray_vec.j = 0.0f;
	ray_vec.k = 0.28f;
	memset(&coll, 0, sizeof(coll));
	if (collision_test_vector(both_flags, &eye_pt, &ray_vec, NONE, &coll))
	{
		float d = coll.t * 0.28f;
		if (d < 0.25f)
		{
			real_point3d cand = eye_pt;
			cand.z -= (0.25f - d);
			step_vec.i = 0.0f;
			step_vec.j = 0.0f;
			step_vec.k = cand.z - eye_pt.z;
			memset(&vcoll, 0, sizeof(vcoll));
			if (!collision_test_vector(both_flags, &eye_pt, &step_vec, NONE, &vcoll) &&
				scenario_leaf_index_from_point(&cand) != NONE)
			{
				eye_pt = cand;
			}
		}
	}

	/* 4. Raycast down to find the floor so Spartan feet are NEVER placed below a floor! */
	ray_vec.i = 0.0f;
	ray_vec.j = 0.0f;
	ray_vec.k = -0.75f;
	memset(&coll, 0, sizeof(coll));
	if (collision_test_vector(both_flags, &eye_pt, &ray_vec, NONE, &coll))
	{
		stand_pt.x = eye_pt.x;
		stand_pt.y = eye_pt.y;
		stand_pt.z = coll.point.z + 0.06f;
	}
	else
	{
		stand_pt.x = eye_pt.x;
		stand_pt.y = eye_pt.y;
		stand_pt.z = eye_pt.z - 0.65f;
	}

	/* 5. Verify eye_pt -> stand_pt segment does not cross any surface and lands in a valid BSP leaf */
	step_vec.i = stand_pt.x - eye_pt.x;
	step_vec.j = stand_pt.y - eye_pt.y;
	step_vec.k = stand_pt.z - eye_pt.z;
	memset(&vcoll, 0, sizeof(vcoll));
	if (collision_test_vector(both_flags, &eye_pt, &step_vec, NONE, &vcoll) ||
		scenario_leaf_index_from_point(&stand_pt) == NONE)
	{
		stand_pt = eye_pt;
	}

	if (scenario_leaf_index_from_point(&stand_pt) != NONE)
	{
		s_last_valid_spartan_pos = stand_pt;
		s_has_valid_spartan_pos = 1;
		*out_pos = stand_pt;
	}
	else if (s_has_valid_spartan_pos)
	{
		*out_pos = s_last_valid_spartan_pos;
	}
	else
	{
		*out_pos = stand_pt;
	}
}

static void forge_sync_unit_to_monitor_pose(
	long unit_idx,
	float cam_x, float cam_y, float cam_z,
	float yaw, float pitch, float roll,
	int is_monitor,
	int reposition,
	float min_reposition_dist_sq)
{
	struct unit_datum *u;
	real_point3d u_pos;
	real_vector3d m_fwd, m_up, h_fwd, h_up;

	if (unit_idx == NONE)
		return;
	u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
	if (!u)
		return;

	if (is_monitor)
	{
		u_pos.x = cam_x;
		u_pos.y = cam_y;
		u_pos.z = cam_z - 0.65f;
	}
	else
	{
		forge_compute_safe_spartan_stand_pos(cam_x, cam_y, cam_z, &u_pos);
	}
	euler_to_forward_up(yaw, pitch, roll, &m_fwd, &m_up);
	euler_to_forward_up(yaw, 0.0f, 0.0f, &h_fwd, &h_up);

	if (is_monitor)
	{
		SET_FLAG(u->object.flags, _object_no_collisions_bit, TRUE);
		SET_FLAG(u->object.flags, _object_at_rest_bit, TRUE);
		SET_FLAG(u->object.damage_flags, _object_cannot_take_damage_bit, TRUE);
		SET_FLAG(u->object.damage_flags, _object_shield_depleted_bit, FALSE);
		u->object.body_vitality = 1.0f;
		u->object.shield_vitality = 1.0f;
		u->object.translational_velocity.i = 0.0f;
		u->object.translational_velocity.j = 0.0f;
		u->object.translational_velocity.k = 0.0f;
		u->object.angular_velocity.i = 0.0f;
		u->object.angular_velocity.j = 0.0f;
		u->object.angular_velocity.k = 0.0f;
		forge_hide_spartan_linked_attachments_and_children(unit_idx, 1);
		object_set_automatic_deactivation(unit_idx, FALSE);
		if (reposition)
		{
			float ux_d = u->object.position.x - u_pos.x;
			float uy_d = u->object.position.y - u_pos.y;
			float uz_d = u->object.position.z - u_pos.z;
			if (min_reposition_dist_sq <= 0.0f ||
				(ux_d * ux_d + uy_d * uy_d + uz_d * uz_d) > min_reposition_dist_sq)
			{
				if (scenario_leaf_index_from_point(&u_pos) != NONE)
					object_set_position(unit_idx, &u_pos, &h_fwd, &h_up);
				else
				{
					u->object.position = u_pos;
					u->object.forward = h_fwd;
					u->object.up = h_up;
				}
			}
		}
	}
	else
	{
		SET_FLAG(u->object.flags, _object_no_collisions_bit, FALSE);
		SET_FLAG(u->object.flags, _object_at_rest_bit, FALSE);
		SET_FLAG(u->object.damage_flags, _object_cannot_take_damage_bit, FALSE);
		forge_hide_spartan_linked_attachments_and_children(unit_idx, 0);
		if (reposition)
		{
			if (scenario_leaf_index_from_point(&u_pos) != NONE)
				object_set_position(unit_idx, &u_pos, &h_fwd, &h_up);
			else
			{
				u->object.position = u_pos;
				u->object.forward = h_fwd;
				u->object.up = h_up;
			}
		}
		object_activate(unit_idx);
	}

	u->unit.desired_facing_vector = is_monitor ? m_fwd : h_fwd;
	u->unit.desired_aiming_vector = is_monitor ? m_fwd : h_fwd;
	u->unit.aiming_vector = is_monitor ? m_fwd : h_fwd;
	u->unit.desired_looking_vector = is_monitor ? m_fwd : h_fwd;
	u->unit.looking_vector = is_monitor ? m_fwd : h_fwd;
	u->object.translational_velocity.i = 0.0f;
	u->object.translational_velocity.j = 0.0f;
	u->object.translational_velocity.k = 0.0f;
	u->object.angular_velocity.i = 0.0f;
	u->object.angular_velocity.j = 0.0f;
	u->object.angular_velocity.k = 0.0f;
}

void forge_apply_host_player_monitor_state(
	int player_abs_idx,
	int is_monitor,
	float x, float y, float z,
	float yaw, float pitch)
{
	long p_idx;
	int was_monitor;

	if (player_abs_idx < 0 || player_abs_idx >= 128)
		return;

	was_monitor = g_forge.player_is_monitor[player_abs_idx];
	g_forge.player_is_monitor[player_abs_idx] = (uint8_t)(is_monitor ? 1 : 0);
	g_forge.player_has_monitor_pos[player_abs_idx] = (uint8_t)(is_monitor ? 1 : 0);
	g_forge.player_monitor_pos[player_abs_idx][0] = x;
	g_forge.player_monitor_pos[player_abs_idx][1] = y;
	g_forge.player_monitor_pos[player_abs_idx][2] = z;
	g_forge.player_monitor_yaw[player_abs_idx] = yaw;
	g_forge.player_monitor_pitch[player_abs_idx] = pitch;

	if (!is_monitor)
	{
		g_forge.player_has_render_pos[player_abs_idx] = 0;
	}
	else if (!g_forge.player_has_render_pos[player_abs_idx] || !was_monitor)
	{
		g_forge.player_has_render_pos[player_abs_idx] = 1;
		g_forge.player_monitor_render_pos[player_abs_idx][0] = x;
		g_forge.player_monitor_render_pos[player_abs_idx][1] = y;
		g_forge.player_monitor_render_pos[player_abs_idx][2] = z;
		g_forge.player_monitor_render_yaw[player_abs_idx] = yaw;
		g_forge.player_monitor_render_pitch[player_abs_idx] = pitch;
	}
	else
	{
		float dx = x - g_forge.player_monitor_render_pos[player_abs_idx][0];
		float dy = y - g_forge.player_monitor_render_pos[player_abs_idx][1];
		float dz = z - g_forge.player_monitor_render_pos[player_abs_idx][2];
		if (dx * dx + dy * dy + dz * dz > 36.0f)
		{
			g_forge.player_monitor_render_pos[player_abs_idx][0] = x;
			g_forge.player_monitor_render_pos[player_abs_idx][1] = y;
			g_forge.player_monitor_render_pos[player_abs_idx][2] = z;
			g_forge.player_monitor_render_yaw[player_abs_idx] = yaw;
			g_forge.player_monitor_render_pitch[player_abs_idx] = pitch;
		}
	}

	if (!player_data)
		return;

	for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
	{
		if ((int)DATUM_INDEX_TO_ABSOLUTE_INDEX(p_idx) == player_abs_idx)
		{
			struct player_datum *pl = player_get(p_idx);
			if (pl && pl->unit_index != NONE)
			{
				struct unit_datum *u_chk = (struct unit_datum *)object_try_and_get_and_verify_type(pl->unit_index, _object_mask_unit);
				if (is_monitor && u_chk && (u_chk->object.parent_object_index != NONE || u_chk->unit.parent_seat_index != NONE))
				{
					/* A player seated inside a vehicle cannot switch into Monitor Mode */
					g_forge.player_is_monitor[player_abs_idx] = 0;
					g_forge.player_has_monitor_pos[player_abs_idx] = 0;
					g_forge.player_has_render_pos[player_abs_idx] = 0;
					forge_hide_spartan_linked_attachments_and_children(pl->unit_index, 0);
					return;
				}
				forge_hide_spartan_linked_attachments_and_children(pl->unit_index, is_monitor ? 1 : 0);
				if (game_connection() == _game_connection_network_server &&
					player_abs_idx != forge_get_local_player_abs_index())
				{
					forge_sync_unit_to_monitor_pose(
						pl->unit_index,
						x, y, z,
						yaw, pitch, 0.0f,
						is_monitor ? 1 : 0,
						(!is_monitor || !was_monitor) ? 1 : 0,
						0.0f);
				}
			}
			break;
		}
	}
}

static void forge_monitor_enforce_wall_clearance(real_point3d *pt, float keep_dist)
{
	static float const s_ProbeDirs[6][3] = {
		{  1.0f,  0.0f,  0.0f },
		{ -1.0f,  0.0f,  0.0f },
		{  0.0f,  1.0f,  0.0f },
		{  0.0f, -1.0f,  0.0f },
		{  0.0f,  0.0f,  1.0f },
		{  0.0f,  0.0f, -1.0f }
	};
	unsigned long both_flags = FLAG(_collision_test_structure_bit) |
							   FLAG(_collision_test_front_facing_surfaces_bit) |
							   FLAG(_collision_test_back_facing_surfaces_bit);
	int d;

	for (d = 0; d < 6; d++)
	{
		real_vector3d pvec, svec;
		struct collision_result pcoll, vcoll;
		pvec.i = s_ProbeDirs[d][0] * keep_dist;
		pvec.j = s_ProbeDirs[d][1] * keep_dist;
		pvec.k = s_ProbeDirs[d][2] * keep_dist;
		memset(&pcoll, 0, sizeof(pcoll));
		if (collision_test_vector(both_flags, pt, &pvec, NONE, &pcoll))
		{
			float dist = pcoll.t * keep_dist;
			if (dist < keep_dist)
			{
				float push = (keep_dist - dist);
				float dot = s_ProbeDirs[d][0] * pcoll.plane.n.i +
							s_ProbeDirs[d][1] * pcoll.plane.n.j +
							s_ProbeDirs[d][2] * pcoll.plane.n.k;
				real_point3d cand = *pt;
				if (dot < 0.0f)
				{
					cand.x += pcoll.plane.n.i * push;
					cand.y += pcoll.plane.n.j * push;
					cand.z += pcoll.plane.n.k * push;
				}
				else
				{
					cand.x -= s_ProbeDirs[d][0] * push;
					cand.y -= s_ProbeDirs[d][1] * push;
					cand.z -= s_ProbeDirs[d][2] * push;
				}
				svec.i = cand.x - pt->x;
				svec.j = cand.y - pt->y;
				svec.k = cand.z - pt->z;
				memset(&vcoll, 0, sizeof(vcoll));
				if (!collision_test_vector(both_flags, pt, &svec, NONE, &vcoll) &&
					scenario_leaf_index_from_point(&cand) != NONE)
				{
					*pt = cand;
				}
			}
		}
	}
}

static int forge_monitor_try_step(
	real_point3d const *from_pt,
	float dx, float dy, float dz,
	float pad,
	real_point3d *out_pt,
	real_plane3d *out_hit_plane)
{
	unsigned long both_flags = FLAG(_collision_test_structure_bit) |
							   FLAG(_collision_test_front_facing_surfaces_bit) |
							   FLAG(_collision_test_back_facing_surfaces_bit);
	float len = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
	real_vector3d dir, ray_vec, step_vec, perp1, perp2;
	struct collision_result best_coll, fcoll, vcoll;
	int hit = 0;
	int f;

	*out_pt = *from_pt;
	if (len <= 0.0001f)
		return 0;

	dir.i = dx / len;
	dir.j = dy / len;
	dir.k = dz / len;

	/* Build two perpendicular axes for 4 radial feeler rays around the camera cylinder */
	if ((float)fabs((double)dir.k) < 0.85f)
	{
		float plen = (float)sqrt((double)(dir.i * dir.i + dir.j * dir.j));
		perp1.i = -dir.j / plen;
		perp1.j =  dir.i / plen;
		perp1.k =  0.0f;
	}
	else
	{
		float plen = (float)sqrt((double)(dir.i * dir.i + dir.k * dir.k));
		perp1.i = -dir.k / plen;
		perp1.j =  0.0f;
		perp1.k =  dir.i / plen;
	}
	perp2.i = dir.j * perp1.k - dir.k * perp1.j;
	perp2.j = dir.k * perp1.i - dir.i * perp1.k;
	perp2.k = dir.i * perp1.j - dir.j * perp1.i;

	ray_vec.i = dir.i * (len + pad);
	ray_vec.j = dir.j * (len + pad);
	ray_vec.k = dir.k * (len + pad);

	memset(&best_coll, 0, sizeof(best_coll));
	if (collision_test_vector(both_flags, from_pt, &ray_vec, NONE, &best_coll))
		hit = 1;

	/* Cast 4 feeler rays around the Monitor's 0.16u radius so grazing walls/corners never clip */
	for (f = 0; f < 4; f++)
	{
		float s1 = (f == 0) ? 0.16f : ((f == 1) ? -0.16f : 0.0f);
		float s2 = (f == 2) ? 0.16f : ((f == 3) ? -0.16f : 0.0f);
		real_point3d f_orig;
		real_vector3d f_ofs;
		f_ofs.i = perp1.i * s1 + perp2.i * s2;
		f_ofs.j = perp1.j * s1 + perp2.j * s2;
		f_ofs.k = perp1.k * s1 + perp2.k * s2;
		f_orig.x = from_pt->x + f_ofs.i;
		f_orig.y = from_pt->y + f_ofs.j;
		f_orig.z = from_pt->z + f_ofs.k;
		memset(&vcoll, 0, sizeof(vcoll));
		if (scenario_leaf_index_from_point(&f_orig) != NONE &&
			!collision_test_vector(both_flags, from_pt, &f_ofs, NONE, &vcoll))
		{
			memset(&fcoll, 0, sizeof(fcoll));
			if (collision_test_vector(both_flags, &f_orig, &ray_vec, NONE, &fcoll))
			{
				if (!hit || fcoll.t < best_coll.t)
				{
					best_coll = fcoll;
					hit = 1;
				}
			}
		}
	}

	if (hit)
	{
		float hit_dist = best_coll.t * (len + pad);
		float allow = hit_dist - pad;
		if (out_hit_plane)
			*out_hit_plane = best_coll.plane;
		if (allow > 0.0005f)
		{
			real_point3d cand;
			cand.x = from_pt->x + dir.i * allow;
			cand.y = from_pt->y + dir.j * allow;
			cand.z = from_pt->z + dir.k * allow;
			step_vec.i = cand.x - from_pt->x;
			step_vec.j = cand.y - from_pt->y;
			step_vec.k = cand.z - from_pt->z;
			memset(&vcoll, 0, sizeof(vcoll));
			if (!collision_test_vector(both_flags, from_pt, &step_vec, NONE, &vcoll) &&
				scenario_leaf_index_from_point(&cand) != NONE)
			{
				*out_pt = cand;
			}
		}
		return 1;
	}
	else
	{
		real_point3d cand;
		cand.x = from_pt->x + dx;
		cand.y = from_pt->y + dy;
		cand.z = from_pt->z + dz;
		step_vec.i = dx;
		step_vec.j = dy;
		step_vec.k = dz;
		memset(&vcoll, 0, sizeof(vcoll));
		if (!collision_test_vector(both_flags, from_pt, &step_vec, NONE, &vcoll) &&
			scenario_leaf_index_from_point(&cand) != NONE)
		{
			*out_pt = cand;
			return 0;
		}
		return 1;
	}
}

static boolean forge_collision_test_vector(
	unsigned long flags,
	real_point3d const *point,
	real_vector3d const *vector,
	long ignore_object_index,
	struct collision_result *collision)
{
	struct
	{
		long obj_idx;
		unsigned long saved_flags;
	} hidden[34];
	int hidden_count = 0;
	int i;
	long local_unit = player_control_get_unit_index(0);
	boolean hit;

	if (local_unit != NONE && hidden_count < 34)
	{
		struct object_datum *o = forge_valid_object_get(local_unit);
		if (o)
		{
			hidden[hidden_count].obj_idx = local_unit;
			hidden[hidden_count].saved_flags = o->object.flags;
			hidden_count++;
			o->object.flags |= FLAG(_object_invisible_bit);
		}
	}
	if (ignore_object_index != NONE && ignore_object_index != local_unit && hidden_count < 34)
	{
		struct object_datum *o = forge_valid_object_get(ignore_object_index);
		if (o)
		{
			hidden[hidden_count].obj_idx = ignore_object_index;
			hidden[hidden_count].saved_flags = o->object.flags;
			hidden_count++;
			o->object.flags |= FLAG(_object_invisible_bit);
		}
	}
	if (player_data)
	{
		long p_idx;
		for (p_idx = data_next_index(player_data, NONE);
			 p_idx != NONE && hidden_count < 34;
			 p_idx = data_next_index(player_data, p_idx))
		{
			struct player_datum *pl = player_get(p_idx);
			if (pl && pl->unit_index != NONE &&
				pl->unit_index != local_unit &&
				pl->unit_index != ignore_object_index &&
				forge_is_unit_in_monitor_mode(pl->unit_index))
			{
				struct object_datum *o = forge_valid_object_get(pl->unit_index);
				if (o)
				{
					hidden[hidden_count].obj_idx = pl->unit_index;
					hidden[hidden_count].saved_flags = o->object.flags;
					hidden_count++;
					o->object.flags |= FLAG(_object_invisible_bit);
				}
			}
		}
	}

	/* (scenery with COLLISION off is still pointed at, and placed on) */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		struct object_datum *o;

		if (inst->active && inst->no_collision && i != g_forge.grabbed_live_index &&
			(o = forge_valid_object_get(inst->object_index)) != NULL)
		{
			SET_FLAG(o->object.flags, _object_no_collisions_bit, FALSE);
		}
	}
	hit = collision_test_vector(flags, point, vector, ignore_object_index, collision);
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		struct object_datum *o;

		if (inst->active && inst->no_collision && i != g_forge.grabbed_live_index &&
			(o = forge_valid_object_get(inst->object_index)) != NULL)
		{
			SET_FLAG(o->object.flags, _object_no_collisions_bit, TRUE);
		}
	}

	for (i = 0; i < hidden_count; i++)
	{
		struct object_datum *o = forge_valid_object_get(hidden[i].obj_idx);
		if (o)
			o->object.flags = hidden[i].saved_flags;
	}
	return hit;
}

static long find_unattached_world_object_near(float ox, float oy, float oz)
{
	struct object_iterator iter;
	struct object_datum *dobj;
	long found_oidx = NONE;
	float best_d2 = 9.0f;

	object_iterator_new(&iter, (unsigned long)-1, 0);
	while ((dobj = object_iterator_next(&iter)) != NULL)
	{
		float dx = dobj->object.position.x - ox;
		float dy = dobj->object.position.y - oy;
		float dz = dobj->object.position.z - oz;
		float d2 = dx * dx + dy * dy + dz * dz * 0.25f;
		if (dobj->object.parent_object_index != NONE)
			continue;
		if (dobj->object.type == _object_type_biped &&
			((struct unit_datum *)dobj)->unit.player_index != NONE)
			continue;
		if (d2 < best_d2)
		{
			best_d2 = d2;
			found_oidx = iter.index;
		}
	}
	return found_oidx;
}

static void forge_reconcile_live_instances(void)
{
	static long s_last_game_tick = -1;
	static struct scenario *s_last_tick_scen = NULL;
	static long s_last_unit_index = NONE;
	static int s_last_forge_mode = -1;
	struct scenario *scen = global_scenario_try_and_get();
	long cur_tick = game_time_get();
	long cur_unit = player_control_get_unit_index(0);
	int reverted = 0;
	int i;

	if (game_connection() == _game_connection_network_client)
		return;

	if (scen != s_last_tick_scen)
	{
		s_last_tick_scen = scen;
		s_last_game_tick = cur_tick;
		s_last_unit_index = cur_unit;
		s_last_forge_mode = (int)g_forge.mode;
	}
	else
	{
		if (s_last_game_tick >= 0 && cur_tick < s_last_game_tick)
		{
			/* Halo's single-player death or checkpoint revert (game_state_revert) rewound the game state! */
			reverted = 1;
		}
		if (cur_unit != s_last_unit_index)
		{
			s_last_unit_index = cur_unit;
		}
		s_last_forge_mode = (int)g_forge.mode;
	}
	s_last_game_tick = cur_tick;

	/* If the local player died while holding an item, cleanly place/release it so it isn't left mid-grab */
	if (cur_unit == NONE && g_forge.grabbed_live_index >= 0)
	{
		forge_cancel_or_release_held_item();
		if (g_forge.active_modal == _forge_modal_object_properties)
			g_forge.active_modal = _forge_modal_none;
	}

	if (reverted)
	{
		sync_live_player_spawns_to_scenario();
		sync_live_netgame_flags_to_scenario();

		/* Re-delete any default scenario objects that were deleted in Forge before game_state_revert() */
		for (i = 0; i < (int)g_forge.edit_state.default_edit_count; i++)
		{
			struct forge_default_object_edit *de = &g_forge.edit_state.default_edits[i];
			long found_oidx;

			if (!de->active || !de->deleted)
				continue;

			found_oidx = find_unattached_world_object_near(de->orig_x, de->orig_y, de->orig_z);
			if (found_oidx != NONE)
				object_delete(found_oidx);
		}
	}

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[i];
		struct object_datum *obj;
		uint32_t actual_grp;
		long def_idx;

		if (inst->group_tag == 'spwn' || inst->group_tag == 'tele' || inst->group_tag == 'flag' || inst->group_tag == 'ffsp' ||
			inst->group_tag == FORGE_GROUP_AI_SPAWN)
			continue;

		if (inst->is_default_scenario_object)
		{
			if (reverted)
			{
				obj = forge_valid_object_get(inst->object_index);
				if (!obj && inst->active)
				{
					long found_oidx = find_unattached_world_object_near(inst->orig_x, inst->orig_y, inst->orig_z);
					if (found_oidx != NONE)
					{
						inst->object_index = found_oidx;
						obj = forge_valid_object_get(inst->object_index);
					}
				}
				if (obj)
				{
					if (!inst->active)
					{
						object_delete(inst->object_index);
						inst->object_index = NONE;
					}
					else
					{
						forge_3d_move_instance_ext(
							i,
							inst->x, inst->y, inst->z,
							inst->yaw, inst->pitch, inst->roll,
							0, 0);
					}
				}
			}
			else if (inst->active && inst->edit_entry_index >= 0 && inst->object_index != NONE)
			{
				obj = forge_valid_object_get(inst->object_index);
				if (obj && obj->object.parent_object_index == NONE)
				{
					forge_ensure_object_connected_to_bsp(
						inst->object_index,
						obj->object.position.x,
						obj->object.position.y,
						obj->object.position.z);
				}
			}
			continue;
		}

		if (!inst->active || inst->is_ai_unit)
			continue;

		obj = forge_valid_object_get(inst->object_index);
		if (reverted || (!obj && inst->object_index != NONE))
		{
			actual_grp = inst->group_tag;
			def_idx = forge_resolve_engine_tag(inst->group_tag, inst->tag_path, &actual_grp);
			if (obj && def_idx != NONE && obj->definition_index != def_idx)
			{
				/* Slot was overwritten or reverted to a different object by game_state_revert() */
				obj = NULL;
				inst->object_index = NONE;
			}
			if (!obj && def_idx != NONE)
			{
				inst->object_index = forge_create_configured_engine_object(
					def_idx,
					inst->group_tag,
					inst->x, inst->y, inst->z,
					inst->yaw, inst->pitch, inst->roll,
					inst->always_spawn,
					inst->scale);
				obj = forge_valid_object_get(inst->object_index);
				if (obj && i == g_forge.grabbed_live_index)
					g_forge.grabbed_object_index = inst->object_index;
				if (obj && game_connection() == _game_connection_network_server && !inst->is_default_scenario_object)
				{
					forge_network_send_spawn_ext(
						i,
						inst->group_tag,
						inst->tag_path,
						inst->source_cache_id,
						inst->x, inst->y, inst->z,
						inst->yaw, inst->pitch, inst->roll,
						inst->team_designator,
						inst->object_index,
						def_idx);
				}
			}
		}

		if (obj)
		{
			if (reverted && obj->object.parent_object_index == NONE)
			{
				forge_3d_move_instance_ext(
					i,
					inst->x, inst->y, inst->z,
					inst->yaw, inst->pitch, inst->roll,
					(i == g_forge.grabbed_live_index) ? 1 : 0,
					0);
			}
			else if (obj->object.parent_object_index == NONE)
			{
				forge_ensure_object_connected_to_bsp(
					inst->object_index,
					obj->object.position.x,
					obj->object.position.y,
					obj->object.position.z);
			}
		}
	}
}

static int s_init_wait_ticks = 0;
static struct scenario *s_last_scen = NULL;
static long s_player_last_unit_index[128];
static int s_player_last_unit_init = 0;

void forge_3d_reset_session_tracking(void)
{
	forge_3d_history_clear();
	s_route_props_live_idx = -1;
	g_forge.route_follow_live_index = -1;
	forge_ai_routes_level_reset();
	if (s_route_test_flying)
		forge_ff_test_stop();
	s_route_test_flying = 0;
	s_props_factions_open = 0;
	/* (a level's test drop's pods went with it) */
	if (s_test_dropping)
		firefight_ordnance_preview_stop();
	s_test_dropping = 0;
	int k;
	s_init_wait_ticks = 0;
	s_origins_noted = 0;
	s_origins_note_frames = 0;
	s_last_scen = NULL;
	s_has_valid_spartan_pos = 0;
	for (k = 0; k < 128; k++)
		s_player_last_unit_index[k] = NONE;
	s_player_last_unit_init = 1;
}

/* the instance made the grabbed one, as it is now */
/* a volume pointed at by its edge (forge_volumes_pick_edge), and where on
it; held, it keeps where it was taken from the reticle: the offset of its
middle from that point */
static struct
{
	int32_t live_idx;
	float point[3];
} s_volume_pick = { -1 };
static struct
{
	int32_t live_idx;
	float offset[3];
} s_volume_grab = { -1 };

static void forge_select_instance(int best)
{
	struct forge_live_instance *inst = &g_forge.live_instances[best];
	struct object_datum *obj = forge_valid_object_get(inst->object_index);
	float dx, dy, dz;

	if (obj)
	{
		real_vector3d efwd, eup;
		float df, du;
		inst->x = obj->object.position.x;
		inst->y = obj->object.position.y;
		inst->z = obj->object.position.z;
		euler_to_forward_up(inst->yaw, inst->pitch, inst->roll, &efwd, &eup);
		df = (efwd.i - obj->object.forward.i) * (efwd.i - obj->object.forward.i) +
			(efwd.j - obj->object.forward.j) * (efwd.j - obj->object.forward.j) +
			(efwd.k - obj->object.forward.k) * (efwd.k - obj->object.forward.k);
		du = (eup.i - obj->object.up.i) * (eup.i - obj->object.up.i) +
			(eup.j - obj->object.up.j) * (eup.j - obj->object.up.j) +
			(eup.k - obj->object.up.k) * (eup.k - obj->object.up.k);
		if (df > 0.0004f || du > 0.0004f)
			forward_up_to_euler(&obj->object.forward, &obj->object.up, &inst->yaw, &inst->pitch, &inst->roll);
	}
	dx = inst->x - g_forge.cam_x;
	dy = inst->y - g_forge.cam_y;
	dz = inst->z - g_forge.cam_z;
	g_forge.koth_group_placing = 0;
	g_forge.grabbed_live_index = best;
	g_forge.grabbed_object_index = inst->object_index;
	g_forge.grabbed_is_new_spawn = 0;
	g_forge.grabbed_orig_x = inst->x;
	g_forge.grabbed_orig_y = inst->y;
	g_forge.grabbed_orig_z = inst->z;
	g_forge.grabbed_orig_yaw = inst->yaw;
	g_forge.grabbed_orig_pitch = inst->pitch;
	g_forge.grabbed_orig_roll = inst->roll;
	g_forge.grab_distance = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
	if (g_forge.grab_distance < 1.0f) g_forge.grab_distance = 1.0f;
	if (g_forge.grab_distance > 20.0f) g_forge.grab_distance = 20.0f;
	g_forge.grab_yaw = inst->yaw;
	g_forge.grab_pitch = inst->pitch;
	g_forge.grab_roll = inst->roll;
	/* (a volume taken by its edge: held there, its middle where it is) */
	s_volume_grab.live_idx = -1;
	if (best == s_volume_pick.live_idx && forge_volume_get(best))
	{
		float px = s_volume_pick.point[0] - g_forge.cam_x;
		float py = s_volume_pick.point[1] - g_forge.cam_y;
		float pz = s_volume_pick.point[2] - g_forge.cam_z;

		s_volume_grab.live_idx = best;
		s_volume_grab.offset[0] = inst->x - s_volume_pick.point[0];
		s_volume_grab.offset[1] = inst->y - s_volume_pick.point[1];
		s_volume_grab.offset[2] = inst->z - s_volume_pick.point[2];
		g_forge.grab_distance = (float)sqrt((double)(px * px + py * py + pz * pz));
		if (g_forge.grab_distance < 1.0f) g_forge.grab_distance = 1.0f;
		if (g_forge.grab_distance > 30.0f) g_forge.grab_distance = 30.0f;
	}
	/* (as it was, for the history's step when it is let go) */
	forge_history_begin(best);
}

void ae_forge_dev_select_instance(int best) /* AE hook */
{ /* AE hook */
	forge_select_instance(best); /* AE hook */
} /* AE hook */

/* the settings closed on an instance that was not picked up: let go where it
is, as placing it does, which also tells the other machines what changed */
static void forge_settings_selection_end(void)
{
	int32_t live_idx = g_forge.grabbed_live_index;

	g_forge.grabbed_for_settings = 0;
	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return;
	{
		struct forge_live_instance *inst = &g_forge.live_instances[live_idx];

		g_forge.grabbed_live_index = -1;
		g_forge.grabbed_object_index = NONE;
		g_forge.grabbed_is_new_spawn = 0;
		if (!inst->active)
			return;
		forge_3d_move_instance_ext(live_idx, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 0);
		forge_network_send_transform_ext(
			live_idx, inst->object_index, inst->x, inst->y, inst->z, inst->yaw, inst->pitch, inst->roll, 0, 1);
		forge_history_end(live_idx);
	}
}

/* an instance's settings and where it is: what the properties were when
they were opened, for UNDO; and each side of a step of the history */
struct forge_props_snapshot
{
	struct forge_live_instance inst;
	enum forge_snap_angle_mode snap_mode;
	int has_ai;
	struct forge_ai_spawn_entry ai;
	int leg_style[2];
	int has_volume;
	struct forge_volume volume;
};
static struct forge_props_snapshot s_props_opened;

static void forge_props_snapshot_take(int32_t live_idx, struct forge_props_snapshot *snap)
{
	struct forge_live_instance const *inst = &g_forge.live_instances[live_idx];
	int route_id = FF_ROUTE_ID(inst->team_designator);

	memset(snap, 0, sizeof(*snap));
	snap->inst = *inst;
	snap->snap_mode = g_forge.snap_mode;
	snap->has_ai = inst->group_tag == FORGE_GROUP_AI_SPAWN && forge_ai_spawn_settings_get(live_idx, &snap->ai);
	snap->has_volume = forge_volume_get(live_idx) != NULL;
	if (snap->has_volume)
		snap->volume = *forge_volume_get(live_idx);
	if (forge_ff_is_dropship_zone(inst))
	{
		snap->leg_style[0] = forge_ff_route_leg_style(route_id, FF_ROUTE_APPROACH);
		snap->leg_style[1] = forge_ff_route_leg_style(route_id, FF_ROUTE_DEPARTURE);
	}
}

static void forge_props_open(void)
{
	g_forge.active_modal = _forge_modal_object_properties;
	g_forge.props_cursor = 0;
	s_props_factions_open = 0;
	s_route_taken.count = 0;
	/* (each panel opens on its first page) */
	if (g_forge.grabbed_live_index >= 0)
	{
		forge_volume_page_back(g_forge.grabbed_live_index);
		forge_ai_spawn_page_back(g_forge.grabbed_live_index);
		forge_ai_route_page_back(g_forge.grabbed_live_index);
	}
	forge_props_snapshot_take(g_forge.grabbed_live_index, &s_props_opened);
}

/* TELEPORT TO OTHER END: the camera behind the teleporter's other end, as
a player comes out of it, with that end's settings open on the same row, so
the same press goes back. A channel's several other ends in turn */
static void forge_tele_jump(int32_t live_idx)
{
	static int32_t s_last_target[FORGE_TELE_CHANNELS][3];
	static int s_last_target_set;
	struct forge_live_instance const *from = &g_forge.live_instances[live_idx];
	struct forge_live_instance const *dest;
	int32_t ends[64];
	int count = forge_tele_other_ends(live_idx, ends, 64);
	int role = forge_tele_role(from);
	int channel = forge_tele_channel(from);
	int32_t target;
	int k, rank = 0;
	float facing, pad_z, back = 2.5f, rise = 1.3f;
	real_point3d cam;

	if (!s_last_target_set)
	{
		int c;

		for (c = 0; c < FORGE_TELE_CHANNELS; c++)
			s_last_target[c][0] = s_last_target[c][1] = s_last_target[c][2] = -1;
		s_last_target_set = 1;
	}
	if (count <= 0)
	{
		char what[96];

		forge_tele_describe(live_idx, what, sizeof(what));
		forge_set_status("%s", what);
		return;
	}
	if (count > 64)
		count = 64;
	/* (the one after the last this channel's end went to) */
	target = ends[0];
	for (k = 0; k < count; k++)
	{
		if (ends[k] > s_last_target[channel][role])
		{
			target = ends[k];
			rank = k;
			break;
		}
	}
	s_last_target[channel][role] = target;

	/* (let go of this one where it is) */
	if (g_forge.grabbed_for_settings)
		forge_settings_selection_end();
	else
		forge_place_held_item();
	g_forge.active_modal = _forge_modal_none;

	dest = &g_forge.live_instances[target];
	facing = dest->yaw + (dest->group_tag == 'tele' ? 0.0f : dest->tele_dyaw);
	pad_z = dest->z + (dest->group_tag == 'tele' ? 0.0f : dest->tele_dz);
	cam.x = dest->x - (float)cos((double)facing) * back;
	cam.y = dest->y - (float)sin((double)facing) * back;
	cam.z = pad_z + rise;
	/* (no room behind it: above it, looking down) */
	if (scenario_leaf_index_from_point(&cam) == NONE)
	{
		cam.x = dest->x;
		cam.y = dest->y;
		cam.z = pad_z + 1.8f;
		back = 0.35f;
		rise = 1.8f;
	}
	g_forge.cam_x = cam.x;
	g_forge.cam_y = cam.y;
	g_forge.cam_z = cam.z;
	g_forge.cam_yaw = facing;
	g_forge.cam_pitch = -(float)atan2((double)rise, (double)back);

	forge_select_instance(target);
	g_forge.grabbed_for_settings = 1;
	forge_props_open();
	g_forge.props_cursor = 2;
	if (count > 1)
		forge_set_status("At %s #%d (%d of %d)", forge_tele_role_name(forge_tele_role(dest)), forge_tele_channel(dest),
			rank + 1, count);
	else
		forge_set_status("At %s #%d", forge_tele_role_name(forge_tele_role(dest)), forge_tele_channel(dest));
}

/* an instance's settings put back as a snapshot has them (not where it is) */
static void forge_props_restore(int32_t live_idx, struct forge_props_snapshot const *snap)
{
	struct forge_live_instance *inst = &g_forge.live_instances[live_idx];
	struct forge_live_instance const *was = &snap->inst;

	if (snap->has_ai)
	{
		forge_ai_spawn_settings_set(live_idx, &snap->ai);
	}
	else if (snap->has_volume && forge_volume_get(live_idx))
	{
		forge_volume_set(live_idx, &snap->volume, 1);
	}
	else if (inst->group_tag == 'ffsp')
	{
		int route_now = FF_ROUTE_ID(inst->team_designator);
		int route_was = FF_ROUTE_ID(was->team_designator);

		forge_ff_marker_set_usage(live_idx, FF_ROUTE_WITH_ID(was->team_designator, route_now));
		if (forge_ff_is_dropship_zone(inst))
		{
			int route_id;

			if (route_now != 0 && route_was == 0)
				forge_ff_route_set_automatic(live_idx);
			else if (route_now == 0 && route_was != 0)
				forge_ff_route_set_custom(live_idx);
			route_id = FF_ROUTE_ID(inst->team_designator);
			forge_ff_route_set_leg_style(route_id, FF_ROUTE_APPROACH, snap->leg_style[0]);
			forge_ff_route_set_leg_style(route_id, FF_ROUTE_DEPARTURE, snap->leg_style[1]);
		}
	}
	else if (forge_tele_is(inst))
	{
		forge_tele_set(live_idx, forge_tele_role(was), forge_tele_channel(was));
	}
	else
	{
		inst->team_designator = was->team_designator;
		inst->gametype_flags = was->gametype_flags;
		if (inst->always_spawn != was->always_spawn)
		{
			forge_3d_always_spawn_apply(live_idx, was->always_spawn);
			forge_network_send_always_spawn(live_idx, was->always_spawn);
		}
		if (inst->scale != was->scale)
		{
			forge_3d_scale_apply(live_idx, forge_inst_scale(was));
			forge_network_send_scale(live_idx, forge_inst_scale(was));
		}
		if (inst->no_damage != was->no_damage)
		{
			forge_3d_no_damage_apply(live_idx, was->no_damage);
			forge_network_send_no_damage(live_idx, was->no_damage);
		}
		if (inst->no_collision != was->no_collision)
		{
			forge_3d_no_collision_apply(live_idx, was->no_collision);
			forge_network_send_no_collision(live_idx, was->no_collision);
		}
		if (inst->group_tag == 'spwn')
		{
			sync_live_player_spawns_to_scenario();
		}
		else if (inst->group_tag == 'tele' || inst->group_tag == 'flag')
		{
			sync_live_netgame_flags_to_scenario();
		}
		else if (!inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
			inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
		{
			g_forge.edit_state.placed_objects[inst->edit_entry_index].team_index = inst->team_designator;
			g_forge.edit_state.placed_objects[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
		}
		else if (inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
			inst->edit_entry_index < (int)g_forge.edit_state.default_edit_count)
		{
			g_forge.edit_state.default_edits[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
		}
	}
}

/* the properties back as forge_props_open found them, which stay open */
static void forge_props_undo(void)
{
	int32_t live_idx = g_forge.grabbed_live_index;
	struct forge_live_instance const *was = &s_props_opened.inst;

	g_forge.snap_mode = s_props_opened.snap_mode;
	forge_props_restore(live_idx, &s_props_opened);
	/* (an object in the player's hands is where they hold it) */
	if (g_forge.grabbed_for_settings)
		forge_3d_move_instance_ext(live_idx, was->x, was->y, was->z, was->yaw, was->pitch, was->roll, 0, 1);
	forge_set_status("Properties Restored");
}

/* ---------- the undo history

UNDO and REDO (the MAP SETTINGS menu's rows, everywhere; ctrl+z and ctrl+y on
a desktop's keyboard): what was moved, turned or had its properties changed,
and what was placed, a few steps back. A step is the object as it was and as
it is after (each whole, so any step stands alone); a placing's undoing deletes
it, its redoing places it again. Deleting stays the hold to delete's, not
undone: a deleted object's steps go, as do those of a slot something else
takes. Each machine has its own, of what it did; what it puts back goes to the
others as a move, a placing or a delete does. */

#define FORGE_HISTORY_STEPS 20

enum
{
	_forge_history_undo = 0,
	_forge_history_redo,
	NUMBER_OF_FORGE_HISTORIES
};

enum
{
	_forge_step_change = 0, /* moved, turned, its properties */
	_forge_step_place
};

struct forge_history_step
{
	int kind;
	/* (steps of one group undone together: a KOTH hill's ten) */
	int group;
	int32_t live_idx;
	/* (a placing's `before`: nothing) */
	struct forge_props_snapshot before;
	struct forge_props_snapshot after;
};

static struct forge_history_step s_history[NUMBER_OF_FORGE_HISTORIES][FORGE_HISTORY_STEPS];
static int s_history_count[NUMBER_OF_FORGE_HISTORIES];
static int s_history_group;
/* (what an undo itself places, moves and deletes is no step) */
static int s_history_applying;
/* the object picked up (or turned where it is), as it was */
static int32_t s_history_pending_idx = -1;
static struct forge_props_snapshot s_history_pending;

void forge_3d_history_clear(void)
{
	s_history_count[_forge_history_undo] = 0;
	s_history_count[_forge_history_redo] = 0;
	s_history_pending_idx = -1;
}

int forge_3d_history_steps(int redo)
{
	return s_history_count[redo ? _forge_history_redo : _forge_history_undo];
}

static void forge_history_push(int history, struct forge_history_step const *step)
{
	if (s_history_count[history] >= FORGE_HISTORY_STEPS)
	{
		/* (the oldest goes) */
		memmove(&s_history[history][0], &s_history[history][1],
			(size_t)(FORGE_HISTORY_STEPS - 1) * sizeof(s_history[history][0]));
		s_history_count[history] = FORGE_HISTORY_STEPS - 1;
	}
	s_history[history][s_history_count[history]++] = *step;
}

/* a new step of the player's own: what was undone can't be redone after it */
static void forge_history_record(struct forge_history_step const *step)
{
	forge_history_push(_forge_history_undo, step);
	s_history_count[_forge_history_redo] = 0;
}

static void forge_history_forget(int32_t live_idx)
{
	int history;

	if (s_history_applying || live_idx < 0)
		return;
	if (s_history_pending_idx == live_idx)
		s_history_pending_idx = -1;
	for (history = 0; history < NUMBER_OF_FORGE_HISTORIES; history++)
	{
		int from, to = 0;

		for (from = 0; from < s_history_count[history]; from++)
		{
			if (s_history[history][from].live_idx == live_idx)
				continue;
			if (to != from)
				s_history[history][to] = s_history[history][from];
			to++;
		}
		s_history_count[history] = to;
	}
}

/* an instance moved to another slot (a client's placing, linked to the host's) */
void forge_3d_history_remap(int32_t from_idx, int32_t to_idx)
{
	int history, k;

	if (from_idx < 0 || to_idx < 0 || from_idx == to_idx)
		return;
	if (s_history_pending_idx == from_idx)
		s_history_pending_idx = to_idx;
	for (history = 0; history < NUMBER_OF_FORGE_HISTORIES; history++)
	{
		for (k = 0; k < s_history_count[history]; k++)
		{
			if (s_history[history][k].live_idx == from_idx)
				s_history[history][k].live_idx = to_idx;
		}
	}
}

static int forge_history_float_same(float a, float b, float tolerance)
{
	return a - b <= tolerance && b - a <= tolerance;
}

static int forge_history_moved(struct forge_props_snapshot const *a, struct forge_props_snapshot const *b)
{
	struct forge_live_instance const *ia = &a->inst, *ib = &b->inst;

	return !forge_history_float_same(ia->x, ib->x, 0.001f) || !forge_history_float_same(ia->y, ib->y, 0.001f) ||
		!forge_history_float_same(ia->z, ib->z, 0.001f) || !forge_history_float_same(ia->yaw, ib->yaw, 0.0005f) ||
		!forge_history_float_same(ia->pitch, ib->pitch, 0.0005f) ||
		!forge_history_float_same(ia->roll, ib->roll, 0.0005f);
}

static int forge_history_same(struct forge_props_snapshot const *a, struct forge_props_snapshot const *b)
{
	struct forge_live_instance const *ia = &a->inst, *ib = &b->inst;

	if (forge_history_moved(a, b))
		return 0;
	if (ia->team_designator != ib->team_designator || ia->gametype_flags != ib->gametype_flags ||
		ia->always_spawn != ib->always_spawn || ia->no_damage != ib->no_damage ||
		ia->no_collision != ib->no_collision || ia->scale != ib->scale ||
		ia->tele_dz != ib->tele_dz || ia->tele_dyaw != ib->tele_dyaw || strcmp(ia->tag_path, ib->tag_path) != 0)
		return 0;
	if (a->has_ai != b->has_ai || (a->has_ai && memcmp(&a->ai, &b->ai, sizeof(a->ai)) != 0))
		return 0;
	if (a->has_volume != b->has_volume || (a->has_volume && memcmp(&a->volume, &b->volume, sizeof(a->volume)) != 0))
		return 0;
	return a->leg_style[0] == b->leg_style[0] && a->leg_style[1] == b->leg_style[1];
}

static int forge_history_live(int32_t live_idx)
{
	return live_idx >= 0 && live_idx < g_forge.live_instance_count && g_forge.live_instances[live_idx].active;
}

static void forge_history_begin(int32_t live_idx)
{
	if (s_history_applying || !forge_history_live(live_idx))
		return;
	forge_props_snapshot_take(live_idx, &s_history_pending);
	s_history_pending_idx = live_idx;
}

/* let go of (or turned, or its properties closed): a step, if it changed */
static void forge_history_end(int32_t live_idx)
{
	struct forge_history_step step;

	if (s_history_applying || s_history_pending_idx != live_idx || !forge_history_live(live_idx))
		return;
	s_history_pending_idx = -1;
	memset(&step, 0, sizeof(step));
	step.kind = _forge_step_change;
	step.group = ++s_history_group;
	step.live_idx = live_idx;
	step.before = s_history_pending;
	forge_props_snapshot_take(live_idx, &step.after);
	if (!forge_history_same(&step.before, &step.after))
		forge_history_record(&step);
}

static void forge_history_note_placed(int32_t live_idx, int new_group)
{
	struct forge_history_step step;

	if (s_history_applying || !forge_history_live(live_idx))
		return;
	memset(&step, 0, sizeof(step));
	step.kind = _forge_step_place;
	if (new_group)
		++s_history_group;
	step.group = s_history_group;
	step.live_idx = live_idx;
	forge_props_snapshot_take(live_idx, &step.after);
	forge_history_record(&step);
}

/* an instance as a snapshot has it: its settings, then where it is */
static void forge_history_put(int32_t live_idx, struct forge_props_snapshot const *snap)
{
	struct forge_live_instance const *was = &snap->inst;

	forge_props_restore(live_idx, snap);
	forge_3d_move_instance_ext(live_idx, was->x, was->y, was->z, was->yaw, was->pitch, was->roll, 0, 0);
	forge_network_send_transform_ext(live_idx, g_forge.live_instances[live_idx].object_index,
		was->x, was->y, was->z, was->yaw, was->pitch, was->roll, 0, 1);
}

/* a step undone (or redone): FALSE when what it was of is gone */
static int forge_history_step_apply(struct forge_history_step *step, int undo)
{
	struct forge_live_instance const *was = &step->after.inst;
	int32_t live_idx = step->live_idx;

	if (step->kind == _forge_step_place && !undo)
	{
		int32_t placed = forge_3d_spawn_object_at(was->group_tag, was->tag_path, was->source_cache_id,
			was->x, was->y, was->z, was->yaw, was->pitch, was->roll, was->team_designator, 1);

		if (placed < 0)
			return 0;
		forge_3d_history_remap(live_idx, placed);
		step->live_idx = placed;
		forge_history_put(placed, &step->after);
		return 1;
	}
	if (!forge_history_live(live_idx) || g_forge.live_instances[live_idx].group_tag != was->group_tag)
		return 0;
	if (step->kind == _forge_step_place)
		forge_3d_delete_instance(live_idx, 1);
	else
		forge_history_put(live_idx, undo ? &step->before : &step->after);
	return 1;
}

static void forge_history_short_name(char const *tag_path, char *text, size_t size)
{
	char const *name = tag_path;
	char const *p;

	for (p = tag_path; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			name = p + 1;
	}
	snprintf(text, size, "%s", name);
}

/* UNDO (`redo` FALSE) or REDO: the last group of steps that can be. TRUE when
one was */
int forge_3d_history_step(int redo)
{
	int from = redo ? _forge_history_redo : _forge_history_undo;
	int to = redo ? _forge_history_undo : _forge_history_redo;

	/* (what is held first goes back where it was, or a new one away) */
	if (g_forge.grabbed_live_index >= 0)
		forge_cancel_or_release_held_item();
	while (s_history_count[from] > 0)
	{
		int group = s_history[from][s_history_count[from] - 1].group;
		int applied = 0;
		int last_kind = _forge_step_change, last_moved = 0;
		char name[64];

		name[0] = 0;
		s_history_applying = 1;
		while (s_history_count[from] > 0 && s_history[from][s_history_count[from] - 1].group == group)
		{
			struct forge_history_step step = s_history[from][--s_history_count[from]];

			if (forge_history_step_apply(&step, !redo))
			{
				forge_history_push(to, &step);
				last_kind = step.kind;
				last_moved = step.kind == _forge_step_change && forge_history_moved(&step.before, &step.after);
				forge_history_short_name(step.after.inst.tag_path, name, sizeof(name));
				applied++;
			}
		}
		s_history_applying = 0;
		if (applied)
		{
			char count[16];

			count[0] = 0;
			if (applied > 1)
				snprintf(count, sizeof(count), " x%d", applied);
			forge_set_status("%s %s '%s'%s (%d left)", redo ? "Redid" : "Undid",
				last_kind == _forge_step_place ? "placing" : last_moved ? "moving" : "the settings of", name,
				count, s_history_count[from]);
			return 1;
		}
	}
	forge_set_status(redo ? "Nothing to redo" : "Nothing to undo");
	return 0;
}

/* DROP TO FLOOR for what is not held, which the held object's update does not move */
static void forge_settings_selection_drop_to_floor(void)
{
	int32_t live_idx = g_forge.grabbed_live_index;
	struct forge_live_instance const *inst = &g_forge.live_instances[live_idx];
	real_point3d from;
	real_vector3d down;
	struct collision_result collision;

	from.x = inst->x;
	from.y = inst->y;
	from.z = inst->z + 2.0f;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -200.0f;
	memset(&collision, 0, sizeof(collision));
	if (forge_collision_test_vector(
		FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&from, &down, forge_valid_object_get(inst->object_index) ? inst->object_index : NONE, &collision))
	{
		forge_3d_move_instance_ext(live_idx, inst->x, inst->y, collision.point.z + 0.08f +
			(forge_volume_get(live_idx) ? forge_volume_get(live_idx)->size[2] * 0.5f : 0.0f),
			inst->yaw, inst->pitch, inst->roll, 0, 1);
	}
}

/* how far the reticle selects (world units, 3.048 m each) */
#define FORGE_SELECT_RANGE 10.0f
/* how long the delete button is held */
#define FORGE_DELETE_HOLD_MS 800u
/* the hold's progress, for the HUD: 0 to 1 */
static float s_delete_hold_fraction;
/* and UNDO's, the same length */
static float s_undo_hold_fraction;

/* whether the level's walls and ground leave a point in sight of the camera.
It stops short of the point, which may be on a surface or just under it */
static int forge_point_in_sight(float x, float y, float z)
{
	real_point3d from;
	real_vector3d vector;
	struct collision_result collision;
	float dx = x - g_forge.cam_x, dy = y - g_forge.cam_y, dz = z - g_forge.cam_z;
	float distance = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
	float reach;

	if (distance < 0.5f)
		return 1;
	reach = (distance - 0.25f) / distance;
	from.x = g_forge.cam_x;
	from.y = g_forge.cam_y;
	from.z = g_forge.cam_z;
	vector.i = dx * reach;
	vector.j = dy * reach;
	vector.k = dz * reach;
	memset(&collision, 0, sizeof(collision));
	return !collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit), &from, &vector, NONE, &collision);
}

#include "forge_map_settings_menu.inl"

/* (the palette's button: pressed and let go, the palette; held, the map's
settings. When it went down, and whether holding it did something) */
static uint32_t s_palette_press_ms;
static int s_palette_press_used;
/* the palette button's hold toward MAP SETTINGS, for the HUD: 0 to 1
(0 for a quick tap's first moments, so the palette's tap shows no bar) */
static float s_map_settings_hold_fraction;
#define FORGE_MAP_SETTINGS_HOLD_SHOW_MS 120u
/* (B or Esc pressed in the map settings this frame: its pages go back) */
static int s_map_settings_back;

void forge_3d_update(float dt)
{
	static int prev_f1, prev_f2, prev_f4, prev_f5, prev_f8, prev_esc, prev_x, prev_bk, prev_lmb;
	static int prev_up, prev_down, prev_left, prev_right, prev_enter, prev_g, prev_z, prev_f, prev_rot;
	static int prev_copy, prev_look_settings;
	static uint32_t s_del_hold_start_ms = 0;
	static int s_del_hold_done = 0;
	static uint32_t s_undo_hold_start_ms = 0;
	static int s_undo_hold_done = 0;
	static int s_gp_a_held_over = 0, s_gp_b_held_over = 0;
	static uint32_t s_gp_b_hold_start_ms = 0;
	static int s_gp_b_hold_consumed = 0;
	static int32_t s_hover_rot_live_idx = -1;
	static uint32_t s_last_stick_nav_ms = 0;
	static int s_prev_modal = 0;
	struct scenario *scen;
	int cur_f1, cur_f2, cur_f4, cur_f5, cur_f8, cur_esc, cur_x, cur_bk, cur_lmb, cur_rot;
	int cur_up, cur_down, cur_left, cur_right, cur_enter, cur_g, cur_z, cur_f, cur_del, cur_copy, cur_look_settings;
	float gp_lx = 0.0f, gp_ly = 0.0f, gp_rx = 0.0f, gp_ry = 0.0f, gp_lt = 0.0f, gp_rt = 0.0f;

	if (!s_player_last_unit_init)
		forge_3d_reset_session_tracking();
	forge_effect_spots_update();
	forge_decals_update();
	forge_palette_live_preview_update();

	if (g_forge_session_enabled)
	{
		forge_runtime_init();
		forge_poll_controller_state(&gp_lx, &gp_ly, &gp_rx, &gp_ry, &gp_lt, &gp_rt);
	}

	if (g_forge.active_modal == _forge_modal_save_copy_as && s_prev_modal != _forge_modal_save_copy_as)
		s_save_modal_just_opened = 1;
	/* (the press that closed a menu acts on nothing more: it is let go first.
	With the menus' buttons swapped it is even the other button out here) */
	if (s_prev_modal != _forge_modal_none && g_forge.active_modal == _forge_modal_none)
		s_gp_a_held_over = s_gp_b_held_over = 1;
	s_gp_a_held_over = s_gp_a_held_over && g_forge.active_modal == _forge_modal_none && forge_gp_button_down(FORGE_GP_A);
	s_gp_b_held_over = s_gp_b_held_over && g_forge.active_modal == _forge_modal_none && forge_gp_button_down(FORGE_GP_B);
	if (s_gp_a_held_over)
		s_gp_btn_down[FORGE_GP_A] = 0;
	if (s_gp_b_held_over)
		s_gp_btn_down[FORGE_GP_B] = 0;
	s_prev_modal = g_forge.active_modal;
	forge_pointer_poll();

	if (forge_is_main_menu_active())
	{
		forge_3d_reset_session_tracking();
		return;
	}
	scen = global_scenario_try_and_get();
	if (!scen || scen->type == 2 /* _scenario_type_main_menu */)
	{
		forge_3d_reset_session_tracking();
		return;
	}
	if (scen != s_last_scen)
	{
		s_last_scen = scen;
		s_init_wait_ticks = 0;
		s_origins_noted = 0;
		s_origins_note_frames = 0;
		s_has_valid_spartan_pos = 0;
		g_forge.monitor_cam_initialized = 0;
		/* (what the level plays, on every machine: its bake, and a profile) */
		if (g_forge_session_enabled)
			forge_runtime_reload_catalogs();
		/* (a level Forge has no part in keeps none of Forge's lists: forge_level_is_forges) */
		if (g_forge.live_instance_count == 0 && forge_level_is_forges(forge_get_active_map_name()))
			forge_init_map_scenario_entities();
		forge_load_saved_map_state_if_present(forge_get_active_map_name());
	}

	/* (the map's volumes hurt whoever is in them, in a match of the map too:
	forge_volumes.c) */
	forge_volumes_update();
	/* (the scenery with COLLISION off, the same) */
	forge_3d_no_collision_update();
	/* (the AI spawns' CLOAKED and HEIGHT, in a match of the map too: forge_ai.c) */
	forge_ai_units_update();

	/* Only run Forge editor controls when inside a Forge session */
	if (!g_forge_session_enabled)
		return;
	/* (a test spawn of MAP SETTINGS' going on: forge_map_settings.c) */
	forge_map_settings_test_tick(g_forge.mode == _forge_mode_3d_monitor);
	if (s_origins_note_frames < FORGE_ORIGINS_NOTE_FRAMES)
		s_origins_note_frames++;
	else
		forge_origins_note();

	if (g_forge.live_instance_count == 0 && (scen->players.count > 0 || scen->netgame_flags.count > 0))
	{
		forge_init_map_scenario_entities();
		forge_load_saved_map_state_if_present(forge_get_active_map_name());
	}

	/* Point 4: When starting a game, spawn as the Spartan at a default player spawn point */
	if (!g_forge.monitor_cam_initialized)
	{
		long unit_idx = player_control_get_unit_index(0);
		real_point3d spawn_pos;
		float spawn_yaw = 0.0f;
		int ready = 0;

		s_init_wait_ticks++;
		if (unit_idx != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
			if (u)
			{
				real_point3d test_pt = u->object.position;
				test_pt.z += 0.3f;
				if (scenario_leaf_index_from_point(&test_pt) != NONE)
				{
					spawn_pos = u->object.position;
					spawn_yaw = (float)atan2((double)u->object.forward.j, (double)u->object.forward.i);
					ready = 1;
				}
				else if (find_valid_bsp_spawn_point(&spawn_pos, &spawn_yaw))
				{
					real_vector3d fwd, up;
					euler_to_forward_up(spawn_yaw, 0.0f, 0.0f, &fwd, &up);
					SET_FLAG(u->object.flags, _object_no_collisions_bit, FALSE);
					object_set_position(unit_idx, &spawn_pos, &fwd, &up);
					ready = 1;
				}
			}
		}
		else if (s_init_wait_ticks >= 4 && find_valid_bsp_spawn_point(&spawn_pos, &spawn_yaw))
		{
			ready = 1;
		}

		if (ready)
		{
			s_init_wait_ticks = 0;
			g_forge.cam_x = spawn_pos.x;
			g_forge.cam_y = spawn_pos.y;
			g_forge.cam_z = spawn_pos.z + 0.65f;
			g_forge.cam_yaw = spawn_yaw;
			g_forge.cam_pitch = 0.0f;
			g_forge.monitor_cam_initialized = 1;
			g_forge.mode = _forge_mode_play; /* Spawn as Spartan at default player spawn */
			if (forge_is_multiplayer_session())
			{
				forge_network_send_monitor_mode(
					0,
					g_forge.cam_x, g_forge.cam_y, g_forge.cam_z,
					g_forge.cam_yaw, g_forge.cam_pitch);
			}
		}
		else
		{
			forge_network_sync_tick();
			forge_reconcile_live_instances();
			return;
		}
	}

	/* Run collaborative multiplayer sync (late-join state sync, client object adoption, Monitor state heartbeat) */
	forge_network_sync_tick();

	/* Reconcile live Forge instances so placed items never vanish after player death, checkpoint revert, or BSP surface placement */
	forge_reconcile_live_instances();

	/* Ensure any vehicle driven by a player has powered seat_power[0] = 1.0f so flyers/hovercraft thrust works */
	{
		long unit_idx = player_control_get_unit_index(0);
		if (unit_idx != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
			if (u && u->object.parent_object_index != NONE)
			{
				struct unit_datum *veh = (struct unit_datum *)object_try_and_get_and_verify_type(
					u->object.parent_object_index, _object_mask_vehicle);
				if (veh)
				{
					veh->unit.seat_power[0] = 1.0f;
					veh->unit.seat_power[1] = 1.0f;
				}
			}
		}
	}

	/* Point 2 & Point 8: Always keep Monitor floating at cam_x/y/z, invulnerable, zero-velocity, and hide Spartan-linked
	 * green lights/weapons EVEN when F2 Forge Menu, Save Modal, or Escape Pause Menu is open! */
	if (g_forge.mode == _forge_mode_3d_monitor)
	{
		int local_p_abs = forge_get_local_player_abs_index();
		long unit_idx = player_control_get_unit_index(0);
		if (local_p_abs >= 0 && local_p_abs < 128)
		{
			g_forge.player_is_monitor[local_p_abs] = 1;
			g_forge.player_has_monitor_pos[local_p_abs] = 1;
			g_forge.player_monitor_pos[local_p_abs][0] = g_forge.cam_x;
			g_forge.player_monitor_pos[local_p_abs][1] = g_forge.cam_y;
			g_forge.player_monitor_pos[local_p_abs][2] = g_forge.cam_z;
			g_forge.player_monitor_yaw[local_p_abs] = g_forge.cam_yaw;
			g_forge.player_monitor_pitch[local_p_abs] = g_forge.cam_pitch;
		}
		if (unit_idx != NONE)
		{
			forge_sync_unit_to_monitor_pose(
				unit_idx,
				g_forge.cam_x, g_forge.cam_y, g_forge.cam_z,
				g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll,
				1, 1, 0.0f);
		}
	}
	else
	{
		int local_p_abs = forge_get_local_player_abs_index();
		if (local_p_abs >= 0 && local_p_abs < 128)
		{
			g_forge.player_is_monitor[local_p_abs] = 0;
			g_forge.player_has_monitor_pos[local_p_abs] = 0;
		}
	}

	/* Per-player Monitor vs Spartan visibility, smooth remote Monitor interpolation & Host-side physics protection */
	if (player_data)
	{
		int local_p_abs = forge_get_local_player_abs_index();
		float lerp_t = (dt > 0.0f && dt < 0.25f) ? (1.0f - (float)exp((double)(-18.0f * dt))) : 0.35f;
		uint8_t slot_seen[128];
		int s_idx;
		long p_idx;

		memset(slot_seen, 0, sizeof(slot_seen));
		for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
		{
			struct player_datum *pl = player_get(p_idx);
			int p_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(p_idx);
			if (p_abs >= 0 && p_abs < 128 && pl && pl->unit_index != NONE)
			{
				slot_seen[p_abs] = 1;
				if (p_abs != local_p_abs && s_player_last_unit_index[p_abs] != pl->unit_index)
				{
					/* Remote player just joined, rejoined, or respawned into a new biped: reset Monitor state so
					 * the Host never teleports a newly rejoined player back to their pre-quit Monitor coordinates! */
					if (s_player_last_unit_index[p_abs] == NONE)
					{
						g_forge.player_is_monitor[p_abs] = 0;
						g_forge.player_has_monitor_pos[p_abs] = 0;
						g_forge.player_has_render_pos[p_abs] = 0;
					}
					s_player_last_unit_index[p_abs] = pl->unit_index;
				}
			}
		}
		for (s_idx = 0; s_idx < 128; s_idx++)
		{
			if (!slot_seen[s_idx] && s_idx != local_p_abs)
			{
				s_player_last_unit_index[s_idx] = NONE;
				g_forge.player_is_monitor[s_idx] = 0;
				g_forge.player_has_monitor_pos[s_idx] = 0;
				g_forge.player_has_render_pos[s_idx] = 0;
			}
		}

		for (p_idx = data_next_index(player_data, NONE); p_idx != NONE; p_idx = data_next_index(player_data, p_idx))
		{
			struct player_datum *pl = player_get(p_idx);
			if (pl && pl->unit_index != NONE)
			{
				int p_abs = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(p_idx);
				int is_mon = forge_is_unit_in_monitor_mode(pl->unit_index);
				forge_hide_spartan_linked_attachments_and_children(pl->unit_index, is_mon ? 1 : 0);
				if (is_mon && p_abs != local_p_abs && p_abs >= 0 && p_abs < 128 &&
					g_forge.player_has_monitor_pos[p_abs])
				{
					if (!g_forge.player_has_render_pos[p_abs])
					{
						g_forge.player_has_render_pos[p_abs] = 1;
						g_forge.player_monitor_render_pos[p_abs][0] = g_forge.player_monitor_pos[p_abs][0];
						g_forge.player_monitor_render_pos[p_abs][1] = g_forge.player_monitor_pos[p_abs][1];
						g_forge.player_monitor_render_pos[p_abs][2] = g_forge.player_monitor_pos[p_abs][2];
						g_forge.player_monitor_render_yaw[p_abs] = g_forge.player_monitor_yaw[p_abs];
						g_forge.player_monitor_render_pitch[p_abs] = g_forge.player_monitor_pitch[p_abs];
					}
					else
					{
						float dyaw = g_forge.player_monitor_yaw[p_abs] - g_forge.player_monitor_render_yaw[p_abs];
						float dpitch = g_forge.player_monitor_pitch[p_abs] - g_forge.player_monitor_render_pitch[p_abs];
						while (dyaw > 3.14159265f) dyaw -= 6.2831853f;
						while (dyaw < -3.14159265f) dyaw += 6.2831853f;
						g_forge.player_monitor_render_pos[p_abs][0] += (g_forge.player_monitor_pos[p_abs][0] - g_forge.player_monitor_render_pos[p_abs][0]) * lerp_t;
						g_forge.player_monitor_render_pos[p_abs][1] += (g_forge.player_monitor_pos[p_abs][1] - g_forge.player_monitor_render_pos[p_abs][1]) * lerp_t;
						g_forge.player_monitor_render_pos[p_abs][2] += (g_forge.player_monitor_pos[p_abs][2] - g_forge.player_monitor_render_pos[p_abs][2]) * lerp_t;
						g_forge.player_monitor_render_yaw[p_abs] += dyaw * lerp_t;
						g_forge.player_monitor_render_pitch[p_abs] += dpitch * lerp_t;
					}

					/* Sync remote Monitor unit pose every frame on BOTH Host and Client with 0.0f distance threshold
					 * so the hidden biped (and overhead player name tag) never falls under gravity and snaps back! */
					forge_sync_unit_to_monitor_pose(
						pl->unit_index,
						g_forge.player_monitor_render_pos[p_abs][0],
						g_forge.player_monitor_render_pos[p_abs][1],
						g_forge.player_monitor_render_pos[p_abs][2],
						g_forge.player_monitor_render_yaw[p_abs],
						g_forge.player_monitor_render_pitch[p_abs],
						0.0f,
						1, 1, 0.0f);
				}
			}
		}
	}

	/* When the native in-game Escape Pause Menu is open, pause Forge flight/editor input */
	if (ui_widgets_active())
		return;

	/* Validate grabbed_object_index each tick so a despawned/recreated object never leaves a stale handle */
	if (g_forge.grabbed_object_index != NONE && !forge_valid_object_get(g_forge.grabbed_object_index))
	{
		if (g_forge.grabbed_live_index >= 0 && g_forge.grabbed_live_index < g_forge.live_instance_count)
			g_forge.live_instances[g_forge.grabbed_live_index].object_index = NONE;
		g_forge.grabbed_object_index = NONE;
	}

	cur_f1 = forge_action_down(FORGE_BIND_TOGGLE_MODE);
	cur_f2 = forge_action_down(FORGE_BIND_PALETTE);
	cur_f4 = forge_action_down(FORGE_BIND_THIRD_PERSON);
	cur_f5 = forge_action_down(FORGE_BIND_QUICK_SAVE);
	cur_f8 = (GetAsyncKeyState(0x77) & 0x8000) != 0; /* VK_F8 */
	cur_esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
	if (s_pointer_input.back && g_forge.active_modal != _forge_modal_save_copy_as)
	{
		cur_esc = 1;
		prev_esc = 0;
	}
	cur_x = forge_action_down(FORGE_BIND_OBJECT_OPTIONS);
	cur_bk = ((GetAsyncKeyState(VK_BACK) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_B);
	cur_lmb = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) ||
			  (g_forge.active_modal == _forge_modal_none && forge_gp_button_down(FORGE_GP_A));

	/* When holding an item on controller and both Palette & Object Options share FORGE_GP_X,
	 * route FORGE_GP_X to Object Properties instead of opening the Spawn Palette (unless keyboard F2 is pressed). */
	if (g_forge.grabbed_live_index >= 0 &&
		g_forge.active_modal != _forge_modal_spawn_palette &&
		g_forge.bind_gp[FORGE_BIND_PALETTE] == g_forge.bind_gp[FORGE_BIND_OBJECT_OPTIONS] &&
		!(g_forge.bind_vk[FORGE_BIND_PALETTE] > 0 && (GetAsyncKeyState(g_forge.bind_vk[FORGE_BIND_PALETTE]) & 0x8000)))
	{
		cur_f2 = 0;
	}

	/* Hot-reload Forge UI XML & PNG backgrounds live in-game with F8 */
	if (cur_f8 && !prev_f8)
	{
		forge_ui_theme_load(1);
		forge_set_status("Reloaded Forge UI Theme");
	}

	{
		int modal_before_hotkeys = g_forge.active_modal;
		int allow_f1 = !(modal_before_hotkeys != _forge_modal_none && g_forge.using_controller &&
						 !(g_forge.bind_vk[FORGE_BIND_TOGGLE_MODE] > 0 &&
						   (GetAsyncKeyState(g_forge.bind_vk[FORGE_BIND_TOGGLE_MODE]) & 0x8000)));
		int allow_f4 = !(modal_before_hotkeys != _forge_modal_none &&
						 !(g_forge.bind_vk[FORGE_BIND_THIRD_PERSON] > 0 &&
						   (GetAsyncKeyState(g_forge.bind_vk[FORGE_BIND_THIRD_PERSON]) & 0x8000)));
		int allow_f5 = !(modal_before_hotkeys != _forge_modal_none && g_forge.using_controller &&
						 !(g_forge.bind_vk[FORGE_BIND_QUICK_SAVE] > 0 &&
						   (GetAsyncKeyState(g_forge.bind_vk[FORGE_BIND_QUICK_SAVE]) & 0x8000)));

		/* (MAP SETTINGS' TEST SPAWN leaves Forge mode as the button would:
		forge_map_settings_test_tick does the rest) */
		int test_leaves = forge_map_settings_menu_test_requested() && g_forge.mode == _forge_mode_3d_monitor;

		/* Mode & Modal Hotkeys (Point 2: Seamless location continuity between Spartan and Forge Mode; Point 11: Overhead viewer removed) */
		if ((cur_f1 && !prev_f1 && allow_f1) || test_leaves || ae_forge_dev_mode_toggle(g_forge.mode == _forge_mode_3d_monitor)) /* AE hook */
		{
			int local_p_abs = forge_get_local_player_abs_index();
			long unit_idx = player_control_get_unit_index(0);
			/* (what is held stays where it is, as a click would put it) */
			forge_place_held_item();
			forge_origins_note();
			g_forge.active_modal = _forge_modal_none;
			if (g_forge.mode == _forge_mode_3d_monitor)
			{
				real_point3d sp_pos;
				sp_pos.x = g_forge.cam_x;
				sp_pos.y = g_forge.cam_y;
				sp_pos.z = g_forge.cam_z - 0.65f;
				g_forge.mode = _forge_mode_play;
				if (local_p_abs >= 0 && local_p_abs < 128)
				{
					g_forge.player_is_monitor[local_p_abs] = 0;
					g_forge.player_has_monitor_pos[local_p_abs] = 0;
				}
				if (unit_idx != NONE)
				{
					struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit);
					if (u)
					{
						real_vector3d fwd, up, look;
						forge_compute_safe_spartan_stand_pos(g_forge.cam_x, g_forge.cam_y, g_forge.cam_z, &sp_pos);
						euler_to_forward_up(g_forge.cam_yaw, 0.0f, 0.0f, &fwd, &up);
						/* (the body stands level, the view stays where the monitor's was) */
						look.i = (float)(cos((double)g_forge.cam_yaw) * cos((double)g_forge.cam_pitch));
						look.j = (float)(sin((double)g_forge.cam_yaw) * cos((double)g_forge.cam_pitch));
						look.k = (float)sin((double)g_forge.cam_pitch);
						SET_FLAG(u->object.flags, _object_no_collisions_bit, FALSE);
						SET_FLAG(u->object.flags, _object_at_rest_bit, FALSE);
						SET_FLAG(u->object.damage_flags, _object_cannot_take_damage_bit, FALSE);
						forge_hide_spartan_linked_attachments_and_children(unit_idx, 0);
						object_set_position(unit_idx, &sp_pos, &fwd, &up);
						u->unit.desired_facing_vector = fwd;
						u->unit.desired_aiming_vector = look;
						u->unit.aiming_vector = look;
						u->unit.desired_looking_vector = look;
						u->unit.looking_vector = look;
						/* (the player's own angles drive the unit from here on) */
						player_control_set_facing(0, &look);
						u->object.translational_velocity.i = 0.0f;
						u->object.translational_velocity.j = 0.0f;
						u->object.translational_velocity.k = 0.0f;
						u->object.angular_velocity.i = 0.0f;
						u->object.angular_velocity.j = 0.0f;
						u->object.angular_velocity.k = 0.0f;
						object_activate(unit_idx);
					}
				}
				forge_network_send_monitor_mode(0, g_forge.cam_x, g_forge.cam_y, g_forge.cam_z, g_forge.cam_yaw, g_forge.cam_pitch);
				forge_ai_forge_mode_changed(0);
				forge_set_status("Spartan Mode");
			}
			else
			{
				struct unit_datum *u = (unit_idx != NONE)
					? (struct unit_datum *)object_try_and_get_and_verify_type(unit_idx, _object_mask_unit)
					: NULL;
				if (u && (u->object.parent_object_index != NONE || u->unit.parent_seat_index != NONE))
				{
					forge_set_status("Exit vehicle before entering Monitor Mode");
				}
				else
				{
					if (u)
					{
						if (scenario_leaf_index_from_point(&u->object.position) != NONE)
						{
							s_last_valid_spartan_pos = u->object.position;
							s_has_valid_spartan_pos = 1;
						}
						g_forge.cam_x = u->object.position.x;
						g_forge.cam_y = u->object.position.y;
						g_forge.cam_z = u->object.position.z + 0.65f;
						g_forge.cam_yaw = (float)atan2((double)u->unit.aiming_vector.j, (double)u->unit.aiming_vector.i);
						g_forge.cam_pitch = (float)asin((double)u->unit.aiming_vector.k);
						forge_hide_spartan_linked_attachments_and_children(unit_idx, 1);
					}
					g_forge.mode = _forge_mode_3d_monitor;
					if (local_p_abs >= 0 && local_p_abs < 128)
					{
						g_forge.player_is_monitor[local_p_abs] = 1;
						g_forge.player_has_monitor_pos[local_p_abs] = 1;
						g_forge.player_monitor_pos[local_p_abs][0] = g_forge.cam_x;
						g_forge.player_monitor_pos[local_p_abs][1] = g_forge.cam_y;
						g_forge.player_monitor_pos[local_p_abs][2] = g_forge.cam_z;
						g_forge.player_monitor_yaw[local_p_abs] = g_forge.cam_yaw;
						g_forge.player_monitor_pitch[local_p_abs] = g_forge.cam_pitch;
					}
					forge_network_send_monitor_mode(1, g_forge.cam_x, g_forge.cam_y, g_forge.cam_z, g_forge.cam_yaw, g_forge.cam_pitch);
					forge_ai_forge_mode_changed(1);
					forge_origins_reset();
					forge_set_status("Forge Mode");
				}
			}
		}
		/* (the palette's button: let go of quickly, the palette, as it was
		on its press; held, MAP SETTINGS. A press with either open closes it) */
		{
			uint32_t now_ms = (uint32_t)system_milliseconds();

			if (cur_f2 && !prev_f2)
			{
				s_palette_press_ms = now_ms ? now_ms : 1u;
				s_palette_press_used = 0;
				if (modal_before_hotkeys == _forge_modal_spawn_palette || modal_before_hotkeys == _forge_modal_map_settings)
				{
					g_forge.active_modal = _forge_modal_none;
					s_palette_press_used = 1;
				}
			}
			if (cur_f2 && s_palette_press_ms && !s_palette_press_used &&
				now_ms - s_palette_press_ms >= FORGE_MAP_SETTINGS_HOLD_MS)
			{
				s_palette_press_used = 1;
				if (g_forge.mode == _forge_mode_3d_monitor)
					forge_map_settings_menu_show();
			}
			s_map_settings_hold_fraction = 0.0f;
			if (cur_f2 && s_palette_press_ms && !s_palette_press_used &&
				g_forge.mode == _forge_mode_3d_monitor &&
				now_ms - s_palette_press_ms >= FORGE_MAP_SETTINGS_HOLD_SHOW_MS)
			{
				s_map_settings_hold_fraction = (float)(now_ms - s_palette_press_ms) / (float)FORGE_MAP_SETTINGS_HOLD_MS;
				if (s_map_settings_hold_fraction > 1.0f)
					s_map_settings_hold_fraction = 1.0f;
			}
		}
		/* Point 1: Only allow F2 / Controller B to toggle Forge Menu when the player is in Forge Mode (_forge_mode_3d_monitor)! */
		if (!cur_f2 && prev_f2 && s_palette_press_ms && !s_palette_press_used)
		{
			if (g_forge.mode == _forge_mode_3d_monitor)
			{
				if (modal_before_hotkeys == _forge_modal_spawn_palette)
				{
					g_forge.active_modal = _forge_modal_none;
				}
				else
				{
					if (g_forge.grabbed_live_index >= 0)
						forge_cancel_or_release_held_item();
					g_forge.active_modal = _forge_modal_spawn_palette;
					g_forge.h3_menu_stage = 1; /* Unified single-stage tabbed menu: cycle categories with [<-] / [->] */
					if ((g_forge.current_category == _forge_cat_bipeds ||
						 g_forge.current_category == _forge_cat_ai_squads) &&
						get_filtered_catalog_indices_for_cat(g_forge.current_category, NULL, 1) <= 0)
					{
						g_forge.current_category = _forge_cat_weapons;
					}
				}
			}
		}
		if (cur_f4 && !prev_f4 && allow_f4 && g_forge.mode == _forge_mode_3d_monitor)
		{
			g_forge.third_person_view = !g_forge.third_person_view;
			forge_set_status("3rd-Person View: %s", g_forge.third_person_view ? "ON" : "OFF");
		}
		/* F5 in-game is strictly for Quick Save only (available after initial save via Esc -> SAVE MAP..) */
		if (cur_f5 && !prev_f5 && allow_f5)
		{
			if (g_forge.has_saved_map_once && g_forge.save_map_name[0])
			{
				forge_ui_save_copy_as(g_forge.save_map_name);
			}
			else
			{
				forge_set_status("Save the Map From the Menu First");
			}
		}
		if (cur_x && !prev_x && g_forge.mode == _forge_mode_3d_monitor && g_forge.grabbed_live_index >= 0)
		{
			if (modal_before_hotkeys == _forge_modal_object_properties)
				g_forge.active_modal = _forge_modal_none;
			else
				forge_props_open();
		}
		s_map_settings_back = 0;
		if (((cur_esc && !prev_esc) || (cur_bk && !prev_bk)) &&
			modal_before_hotkeys == _forge_modal_map_settings)
		{
			/* (its pages go back to its first: forge_map_settings_menu_input) */
			s_map_settings_back = 1;
		}
		else if (((cur_esc && !prev_esc) || (cur_bk && !prev_bk)) &&
			modal_before_hotkeys != _forge_modal_none &&
			modal_before_hotkeys != _forge_modal_save_copy_as)
		{
			int props_live = g_forge.grabbed_live_index;

			/* (out of an AI spawn's UNIT/SPAWN/GOAL page, to its first page,
			on the row it was opened from) */
			if (modal_before_hotkeys == _forge_modal_object_properties && props_live >= 0 &&
				forge_ai_spawn_page_back(props_live))
			{
				int rows[NUMBER_OF_FORGE_AI_ROWS];
				int count = forge_ai_spawn_rows(props_live, rows), k;

				g_forge.props_cursor = 0;
				for (k = 0; k < count; k++)
				{
					if (rows[k] == forge_ai_spawn_page_return_row())
						g_forge.props_cursor = k;
				}
			}
			/* (out of a volume's EFFECT or SIZE, to its first page) */
			else if (modal_before_hotkeys == _forge_modal_object_properties && props_live >= 0 &&
				forge_volume_is(props_live) && forge_volume_page_back(props_live))
			{
				int rows[NUMBER_OF_FORGE_VOLUME_ROWS];
				int count = forge_volume_rows(props_live, rows), k;

				g_forge.props_cursor = 0;
				for (k = 0; k < count; k++)
				{
					if (rows[k] == forge_volume_page_return_row())
						g_forge.props_cursor = k;
				}
			}
			/* (out of an AI route's settings, to its point) */
			else if (modal_before_hotkeys == _forge_modal_object_properties && props_live >= 0 &&
				forge_ai_route_page_back(props_live))
			{
				int rows[NUMBER_OF_FORGE_AI_ROUTE_ROWS];
				int count = forge_ai_route_rows(props_live, rows), k;

				g_forge.props_cursor = 0;
				for (k = 0; k < count; k++)
				{
					if (rows[k] == _forge_ai_route_row_settings)
						g_forge.props_cursor = k;
				}
			}
			/* (out of a drop point's FACTIONS TO SPAWN, to its panel) */
			else if (modal_before_hotkeys == _forge_modal_object_properties && s_props_factions_open)
			{
				s_props_factions_open = 0;
				g_forge.props_cursor = FORGE_FF_ZONE_FIRST_ROW;
			}
			/* (out of FIREFIGHT SPAWNS, to FIREFIGHT's rows) */
			else if (modal_before_hotkeys == _forge_modal_spawn_palette && s_palette_ff_menu)
			{
				s_palette_ff_menu = 0;
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
			}
			/* (out of a group of actors, to the groups) */
			else if (modal_before_hotkeys == _forge_modal_spawn_palette && s_palette_ai_group != NONE)
			{
				s_palette_ai_group = NONE;
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
			}
			/* (out of a keyword's folder to its origin's, out of that to the origins) */
			else if (modal_before_hotkeys == _forge_modal_spawn_palette &&
				forge_catalog_category_split(g_forge.current_category) && s_palette_origin != NONE)
			{
				if (s_palette_keyword != 0)
					s_palette_keyword = 0;
				else
					s_palette_origin = NONE;
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
			}
			else
			{
				g_forge.active_modal = _forge_modal_none;
			}
		}
		else if (cur_esc && !prev_esc &&
				 modal_before_hotkeys == _forge_modal_none &&
				 g_forge.mode == _forge_mode_3d_monitor &&
				 g_forge.grabbed_live_index >= 0)
		{
			forge_cancel_or_release_held_item();
		}

		/* Controller Hold {B} (~350ms) to cancel / drop held item back to original transform */
		if (g_forge.mode == _forge_mode_3d_monitor &&
			modal_before_hotkeys == _forge_modal_none &&
			g_forge.grabbed_live_index >= 0 &&
			forge_gp_button_down(FORGE_GP_B))
		{
			uint32_t now_ms = (uint32_t)system_milliseconds();
			if (s_gp_b_hold_start_ms == 0)
				s_gp_b_hold_start_ms = now_ms ? now_ms : 1;
			else if (!s_gp_b_hold_consumed && (now_ms - s_gp_b_hold_start_ms) >= 350)
			{
				s_gp_b_hold_consumed = 1;
				forge_cancel_or_release_held_item();
			}
		}
		else
		{
			s_gp_b_hold_start_ms = 0;
			s_gp_b_hold_consumed = 0;
		}
	}

	prev_f1 = cur_f1;
	prev_f2 = cur_f2;
	prev_f4 = cur_f4;
	prev_f5 = cur_f5;
	prev_f8 = cur_f8;
	prev_esc = cur_esc;
	prev_x = cur_x;

	if (g_forge.grabbed_for_settings &&
		(g_forge.grabbed_live_index < 0 || g_forge.active_modal != _forge_modal_object_properties))
	{
		forge_settings_selection_end();
	}

	if (g_forge.active_modal == _forge_modal_save_copy_as)
	{
		update_save_modal_input();
		if (g_forge.active_modal == _forge_modal_save_copy_as && s_pointer_input.back)
		{
			g_forge.active_modal = _forge_modal_none;
		}
		else if (g_forge.active_modal == _forge_modal_save_copy_as && s_pointer_input.enter)
		{
			forge_ui_save_copy_as(g_forge.save_map_name);
			g_forge.active_modal = _forge_modal_none;
		}
		prev_bk = cur_bk;
		prev_lmb = cur_lmb;
		return;
	}

	cur_up = ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_DPAD_UP);
	cur_down = ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_DPAD_DOWN);
	cur_left = ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_DPAD_LEFT);
	cur_right = ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_DPAD_RIGHT);
	if (g_forge.active_modal != _forge_modal_none)
	{
		uint32_t now_ms = (uint32_t)system_milliseconds();
		if (now_ms - s_last_stick_nav_ms >= 150)
		{
			if (gp_ly > 0.55f)       { cur_up = 1; prev_up = 0; s_last_stick_nav_ms = now_ms; }
			else if (gp_ly < -0.55f) { cur_down = 1; prev_down = 0; s_last_stick_nav_ms = now_ms; }
			else if (gp_lx < -0.55f) { cur_left = 1; prev_left = 0; s_last_stick_nav_ms = now_ms; }
			else if (gp_lx > 0.55f)  { cur_right = 1; prev_right = 0; s_last_stick_nav_ms = now_ms; }
		}
	}
	cur_enter = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || forge_gp_button_down(FORGE_GP_A);
	/* (what the pointer did, as a press of its own) */
	if (s_pointer_input.enter) { cur_enter = 1; prev_enter = 0; }
	if (s_pointer_input.left) { cur_left = 1; prev_left = 0; }
	if (s_pointer_input.right) { cur_right = 1; prev_right = 0; }
	if (s_pointer_input.up) { cur_up = 1; prev_up = 0; }
	if (s_pointer_input.down) { cur_down = 1; prev_down = 0; }
	cur_rot = (g_forge.active_modal == _forge_modal_none) ? forge_action_down(FORGE_BIND_ROTATE_HOLD) : 0;
	cur_g = (g_forge.active_modal == _forge_modal_none && !cur_rot) ? forge_action_down(FORGE_BIND_ROT_SNAP) : 0;
	cur_z = (g_forge.active_modal == _forge_modal_none) ? forge_action_down(FORGE_BIND_SNAP_FLOOR) : 0;
	cur_f = (g_forge.active_modal == _forge_modal_none) ? forge_action_down(FORGE_BIND_SNAP_SURFACE) : 0;
	cur_del = (g_forge.active_modal == _forge_modal_none &&
			   ((!cur_rot && forge_action_down(FORGE_BIND_DELETE_OBJECT)) ||
				(GetAsyncKeyState(VK_BACK) & 0x8000))) != 0;
	cur_copy = (g_forge.active_modal == _forge_modal_none && !cur_rot) ? forge_action_down(FORGE_BIND_COPY_OBJECT) : 0;
	cur_look_settings = (g_forge.active_modal == _forge_modal_none && !cur_rot) ?
		forge_action_down(FORGE_BIND_LOOK_SETTINGS) : 0;
#if !defined(HALO_ANDROID) && !defined(HALO_SWITCH) && !defined(HALO_PS5) && !defined(HALO_WEB)
	/* UNDO and REDO on a desktop's keyboard: ctrl+z, ctrl+y (or ctrl+shift+z).
	The Z of it is no DROP TO FLOOR while it is held */
	{
		static int s_prev_undo_key = 0, s_prev_redo_key = 0, s_chord_held = 0;
		int ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		int z_down = (GetAsyncKeyState('Z') & 0x8000) != 0;
		int y_down = (GetAsyncKeyState('Y') & 0x8000) != 0;
		int undo_key = ctrl && z_down && !shift;
		int redo_key = ctrl && (y_down || (z_down && shift));

		if (undo_key || redo_key)
			s_chord_held = 1;
		else if (!z_down && !y_down)
			s_chord_held = 0;
		if (s_chord_held)
			cur_z = 0;
		if (g_forge.active_modal == _forge_modal_none && g_forge.mode == _forge_mode_3d_monitor)
		{
			if (undo_key && !s_prev_undo_key)
				forge_3d_history_step(0);
			else if (redo_key && !s_prev_redo_key)
				forge_3d_history_step(1);
		}
		s_prev_undo_key = undo_key;
		s_prev_redo_key = redo_key;
	}
#endif

	/* MAP SETTINGS (the palette's button held: forge_map_settings_menu.inl) */
	if (g_forge.active_modal == _forge_modal_map_settings)
	{
		/* (typing: the keyboard's keys are the name's, not the game's) */
		if (s_map_settings_menu.page == _map_settings_page_name)
		{
			director_inhibit_input(0);
			director_inhibit_facing(0);
		}
		forge_map_settings_menu_input(cur_up && !prev_up, cur_down && !prev_down, cur_left && !prev_left,
			cur_right && !prev_right, cur_enter && !prev_enter, s_map_settings_back);
		s_map_settings_back = 0;
		/* (not the press that closed it, once it is) */
		if (g_forge.active_modal != _forge_modal_map_settings)
		{
			prev_enter = 1;
			prev_lmb = 1;
		}
		goto update_prev_keys;
	}

	/* Unified Tabbed Spawn Palette Menu: [<-] / [->] switches category, [Up] / [Down] / Scroll selects item */
	if (g_forge.active_modal == _forge_modal_spawn_palette)
	{
		int filtered[512];
		int fcount;
		int const visible_rows = 10;

		g_forge.h3_menu_stage = 1;
		if ((g_forge.current_category == _forge_cat_bipeds ||
			 g_forge.current_category == _forge_cat_ai_squads) &&
			get_filtered_catalog_indices_for_cat(g_forge.current_category, NULL, 1) <= 0)
		{
			g_forge.current_category = _forge_cat_weapons;
		}

		if ((cur_left && !prev_left) || (cur_right && !prev_right))
			forge_palette_folders_reset();
		if (cur_left && !prev_left)
		{
			do {
				g_forge.current_category = forge_palette_category_step(g_forge.current_category, -1);
			} while ((g_forge.current_category == _forge_cat_bipeds ||
					  g_forge.current_category == _forge_cat_ai_squads) &&
					 get_filtered_catalog_indices_for_cat(g_forge.current_category, NULL, 1) <= 0);
			g_forge.palette_cursor = 0;
			g_forge.palette_scroll = 0;
		}
		if (cur_right && !prev_right)
		{
			do {
				g_forge.current_category = forge_palette_category_step(g_forge.current_category, 1);
			} while ((g_forge.current_category == _forge_cat_bipeds ||
					  g_forge.current_category == _forge_cat_ai_squads) &&
					 get_filtered_catalog_indices_for_cat(g_forge.current_category, NULL, 1) <= 0);
			g_forge.palette_cursor = 0;
			g_forge.palette_scroll = 0;
		}

		fcount = get_filtered_catalog_indices(filtered, 512);
		if (g_forge.palette_cursor >= fcount)
			g_forge.palette_cursor = (fcount > 0) ? (fcount - 1) : 0;
		if (g_forge.palette_scroll > g_forge.palette_cursor)
			g_forge.palette_scroll = g_forge.palette_cursor;

		if (cur_up && !prev_up && g_forge.palette_cursor > 0)
		{
			g_forge.palette_cursor--;
			if (g_forge.palette_cursor < g_forge.palette_scroll)
				g_forge.palette_scroll = g_forge.palette_cursor;
		}
		if (cur_down && !prev_down && g_forge.palette_cursor + 1 < fcount)
		{
			g_forge.palette_cursor++;
			if (g_forge.palette_cursor >= g_forge.palette_scroll + visible_rows)
				g_forge.palette_scroll = g_forge.palette_cursor - (visible_rows - 1);
		}
		if (cur_enter && !prev_enter && fcount > 0 && g_forge.palette_cursor < fcount)
		{
			struct forge_catalog_entry const *ce = &g_forge.catalog[filtered[g_forge.palette_cursor]];
			real_vector3d cam_fwd, cam_up;
			real_point3d ray_orig;
			real_vector3d ray_vec;
			struct collision_result coll;
			unsigned long cflags = FLAG(_collision_test_structure_bit) |
								   FLAG(_collision_test_objects_bit) |
								   FLAG(_collision_test_front_facing_surfaces_bit);
			short p_idx = local_player_get_player_index(0);
			long local_unit = (p_idx != NONE && player_get(p_idx)) ? player_get(p_idx)->unit_index : NONE;
			float spawn_dist = 4.5f;
			float sx, sy, sz;
			int32_t spawned_idx = -1;

			/* (FIREFIGHT SPAWNS opens its kinds) */
			if (ce->group_tag == FORGE_GROUP_FF_MENU)
			{
				s_palette_ff_menu = 1;
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
				prev_enter = 1;
				goto update_prev_keys;
			}
			/* (a group of actors opens) */
			if (ce->group_tag == FORGE_GROUP_AI_MENU)
			{
				s_palette_ai_group = forge_ai_row_group(ce);
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
				prev_enter = 1;
				goto update_prev_keys;
			}
			/* (an origin's or a keyword's folder opens) */
			if (ce->group_tag == FORGE_GROUP_FOLDER)
			{
				if (s_palette_origin == NONE)
					s_palette_origin = ce->origin;
				else
					s_palette_keyword = ce->keyword;
				g_forge.palette_cursor = 0;
				g_forge.palette_scroll = 0;
				prev_enter = 1;
				goto update_prev_keys;
			}

			if (g_forge.grabbed_live_index >= 0)
				forge_cancel_or_release_held_item();

			/* Perform a fresh raycast along the current camera view ray so newly spawned items
			 * stick directly to the placement cursor and camera angle! */
			euler_to_forward_up(g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll, &cam_fwd, &cam_up);
			ray_orig.x = g_forge.cam_x;
			ray_orig.y = g_forge.cam_y;
			ray_orig.z = g_forge.cam_z;
			ray_vec.i = cam_fwd.i * 20.0f;
			ray_vec.j = cam_fwd.j * 20.0f;
			ray_vec.k = cam_fwd.k * 20.0f;
			memset(&coll, 0, sizeof(coll));
			if (collision_test_vector(cflags, &ray_orig, &ray_vec, local_unit, &coll))
			{
				float dx = coll.point.x - g_forge.cam_x;
				float dy = coll.point.y - g_forge.cam_y;
				float dz = coll.point.z - g_forge.cam_z;
				spawn_dist = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
				if (spawn_dist < 1.5f) spawn_dist = 1.5f;
				if (spawn_dist > 20.0f) spawn_dist = 20.0f;
				g_forge.ray_hit_valid = 1;
				g_forge.ray_hit_x = coll.point.x;
				g_forge.ray_hit_y = coll.point.y;
				g_forge.ray_hit_z = coll.point.z;
				sx = coll.point.x;
				sy = coll.point.y;
				sz = coll.point.z + 0.08f;
			}
			else
			{
				spawn_dist = 4.5f;
				sx = g_forge.cam_x + cam_fwd.i * spawn_dist;
				sy = g_forge.cam_y + cam_fwd.j * spawn_dist;
				sz = g_forge.cam_z + cam_fwd.k * spawn_dist;
			}

			g_forge.koth_group_placing = 0;

			/* Item 4: Spawning [KOTH] Hill Marker drops a 4m diameter circle made of 10 linked points! */
			if (ce->group_tag == 'flag' && strstr(ce->tag_path, "koth") != NULL)
			{
				int cand_id, j, k, used;
				int16_t hill_id = 0;
				float r_init = 2.0f * (1.0f / 3.048f); /* 2.0m radius = 4.0m diameter in Halo world units */

				for (cand_id = 0; cand_id < 16; cand_id++)
				{
					used = 0;
					for (j = 0; j < g_forge.live_instance_count; j++)
					{
						if (g_forge.live_instances[j].active &&
							g_forge.live_instances[j].group_tag == 'flag' &&
							g_forge.live_instances[j].netgame_flag_type == 8 &&
							g_forge.live_instances[j].team_designator == cand_id)
						{
							used = 1;
							break;
						}
					}
					if (!used)
					{
						hill_id = (int16_t)cand_id;
						break;
					}
				}

				g_forge.koth_group_hill_id = hill_id;
				g_forge.koth_group_radius = r_init;
				for (k = 0; k < 10; k++)
				{
					float ang = (float)k * (6.283185307f / 10.0f);
					float nx = sx + r_init * (float)cos((double)ang);
					float ny = sy + r_init * (float)sin((double)ang);
					int32_t nidx = forge_3d_spawn_object_at(
						ce->group_tag,
						ce->tag_path,
						ce->source_cache_id,
						nx, ny, sz,
						ang, 0.0f, 0.0f,
						hill_id,
						1);
					g_forge.koth_group_live_indices[k] = nidx;
					if (k == 0)
						spawned_idx = nidx;
				}
				if (spawned_idx >= 0)
					g_forge.koth_group_placing = 1;
			}
			else
			{
				/* (a drop area stands, as its pods land) */
				float pitch = (ce->group_tag == 'scen' && _stricmp(ce->tag_path, FORGE_DROP_AREA_TAG) == 0) ?
					FORGE_DROP_AREA_PITCH : 0.0f;

				/* (a FIREFIGHT SPAWNS kind: a Firefight marker of its usage; a
				volume stands on what it is put on, its middle half its height up) */
				int ff_preset = forge_palette_is_ff_preset(ce);

				if (ce->group_tag == FORGE_GROUP_VOLUME)
					sz += FORGE_VOLUME_SIZE_DEFAULT * 0.5f;
				spawned_idx = forge_3d_spawn_object_at(
					ce->group_tag,
					ff_preset ? FORGE_FF_SPAWN_PATH : ce->tag_path,
					ce->source_cache_id,
					sx, sy, sz,
					g_forge.cam_yaw, pitch, 0.0f,
					ff_preset ? (int16_t)ce->preset : 0,
					1);
				/* (an AI route: its second point, which follows the first while it is held) */
				if (spawned_idx >= 0 && ce->group_tag == FORGE_GROUP_AI_ROUTE)
					forge_ai_route_begin(spawned_idx);
			}

			/* Hitting Enter confirms the previewed item and attaches it to the placement beam */
			if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000))
			{
				g_forge.active_modal = _forge_modal_none;
				if (spawned_idx >= 0 && g_forge.mode == _forge_mode_3d_monitor)
				{
					g_forge.grabbed_live_index = spawned_idx;
					g_forge.grabbed_object_index = g_forge.live_instances[spawned_idx].object_index;
					g_forge.grabbed_is_new_spawn = 1;
					g_forge.grab_distance = spawn_dist;
					g_forge.grab_yaw = g_forge.cam_yaw;
					g_forge.grab_pitch = g_forge.live_instances[spawned_idx].pitch;
					g_forge.grab_roll = 0.0f;
					if (g_forge.koth_group_placing)
					{
						forge_set_status("KOTH Hill #%d",
							g_forge.koth_group_hill_id);
					}
					else
					{
						forge_set_status("Holding %s", ce->display_name);
					}
				}
			}
			/* Prevent this same Enter/A press from immediately dropping the newly spawned item */
			prev_enter = 1;
			prev_lmb = 1;
			goto update_prev_keys;
		}
		goto update_prev_keys;
	}

	forge_ff_routes_update(dt);
	forge_ai_routes_update();

	/* Halo 3 Object Properties Modal (Points 5, 6, 8, 13: Team for Spawns/CTF, Reference/Channel ID for Teleporters & Objectives, Firefight Spawn options, Spawn Gametype Mode for objects) */
	if (g_forge.active_modal == _forge_modal_object_properties && g_forge.grabbed_live_index >= 0)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[g_forge.grabbed_live_index];
		int is_ff_spawn = (inst->group_tag == 'ffsp');
		int is_route_point = forge_ff_is_route_point(inst);
		int is_zone = forge_ff_is_dropship_zone(inst);
		int snap_row = forge_props_snap_row(inst);
		int is_ai_spawn = (inst->group_tag == FORGE_GROUP_AI_SPAWN);
		/* (a teleporter's: TYPE, CHANNEL, TELEPORT TO OTHER END, then the usual) */
		int is_tele = forge_tele_is(inst);
		/* (a drop area's TEST DROP, after its other rows) */
		int test_drop_row = forge_is_drop_area(inst) ? (snap_row ? 6 : 5) : -1;
		/* (a placed vehicle's ALWAYS SPAWN, after DELETE OBJECT) */
		int always_row = forge_props_always_spawn_row(inst, snap_row, test_drop_row);
		/* (a placed scenery's or effect's SCALE, after the rest) */
		int scale_row = forge_props_scale_row(inst, snap_row, test_drop_row);
		/* (a placed emitter's DEAL DAMAGE, after its SCALE) */
		int damage_row = forge_props_damage_row(inst, scale_row);
		/* (a placed scenery's COLLISION, after those) */
		int collision_row = forge_props_collision_row(inst, scale_row, damage_row);
		/* (a volume's own rows: forge_volumes.h's) */
		int is_volume = forge_volume_is(g_forge.grabbed_live_index);
		/* (an AI spawn's and a route point's rows as shown: each has a page) */
		int is_ai_route = forge_ai_route_is(inst);
		int ai_rows[NUMBER_OF_FORGE_AI_ROWS];
		int ai_row_count = is_ai_spawn ? forge_ai_spawn_rows(g_forge.grabbed_live_index, ai_rows) : 0;
		int route_rows[NUMBER_OF_FORGE_AI_ROUTE_ROWS];
		int route_row_count = is_ai_route ? forge_ai_route_rows(g_forge.grabbed_live_index, route_rows) : 0;
		int zone_rows[FORGE_FF_ZONE_MAX_ROWS];
		int zone_row_count = is_zone ? forge_ff_zone_rows(inst, zone_rows) : 0;
		int volume_rows[NUMBER_OF_FORGE_VOLUME_ROWS];
		int volume_row_count = is_volume ? forge_volume_rows(g_forge.grabbed_live_index, volume_rows) : 0;
		int max_props_row = is_volume ? volume_row_count - 1 :
			is_ai_route ? route_row_count - 1 :
			is_ai_spawn ? ai_row_count - 1 : is_tele ? (snap_row ? 6 : 5) :
			(is_zone ? FORGE_FF_ZONE_FIRST_ROW - 1 + zone_row_count :
				(is_route_point ? 6 : (always_row >= 0 ? always_row : collision_row >= 0 ? collision_row :
					damage_row >= 0 ? damage_row : scale_row >= 0 ? scale_row :
					(test_drop_row >= 0 ? test_drop_row : ((is_ff_spawn || snap_row) ? 5 : 4)))));
		/* (DONE and UNDO, side by side under the rows) */
		int props_done = max_props_row + 1;
		int props_undo = max_props_row + 2;
		int dir = (cur_left && !prev_left) ? -1 : ((cur_right && !prev_right) || (cur_enter && !prev_enter) ? 1 : 0);

		/* FACTIONS TO SPAWN: a drop point's page of HUMAN, COVENANT and FLOOD,
		each on or off (one at least), then BACK */
		if (s_props_factions_open && !is_zone)
			s_props_factions_open = 0;
		if (s_props_factions_open)
		{
			int back_row = 3;

			if (g_forge.props_cursor > back_row)
				g_forge.props_cursor = back_row;
			if (cur_up && !prev_up)
				g_forge.props_cursor = g_forge.props_cursor == 0 ? back_row : g_forge.props_cursor - 1;
			if (cur_down && !prev_down)
				g_forge.props_cursor = g_forge.props_cursor >= back_row ? 0 : g_forge.props_cursor + 1;
			if (g_forge.props_cursor == back_row)
			{
				if (cur_enter && !prev_enter)
				{
					int k;

					s_props_factions_open = 0;
					g_forge.props_cursor = FORGE_FF_ZONE_FIRST_ROW;
					for (k = 0; k < zone_row_count; k++)
					{
						if (zone_rows[k] == _forge_ff_zone_row_factions)
							g_forge.props_cursor = FORGE_FF_ZONE_FIRST_ROW + k;
					}
				}
			}
			else if (dir != 0)
			{
				unsigned kinds = forge_ff_drop_kinds(inst);
				unsigned bit = 1U << s_props_faction_kinds[g_forge.props_cursor];

				if ((kinds & ~bit & FF_DROP_KINDS_ALL) == 0)
				{
					forge_set_status("A Drop Point Spawns One Faction at Least");
				}
				else
				{
					kinds ^= bit;
					forge_ff_marker_set_usage(g_forge.grabbed_live_index,
						FF_ROUTE_WITH_ID(FORGE_FF_PACK_USAGE(ff_drop_point_type(kinds),
							FORGE_FF_UNPACK_ROUND(inst->team_designator), FORGE_FF_UNPACK_WAVE(inst->team_designator)),
							FF_ROUTE_ID(inst->team_designator)));
				}
			}
			goto update_prev_keys;
		}
		if (g_forge.props_cursor > props_undo)
			g_forge.props_cursor = props_undo;
		/* (round the panel: up from the first row is DONE, down from the buttons the first row) */
		if (cur_up && !prev_up)
		{
			g_forge.props_cursor = g_forge.props_cursor == 0 ? props_done :
				g_forge.props_cursor == props_undo ? max_props_row : g_forge.props_cursor - 1;
		}
		if (cur_down && !prev_down)
			g_forge.props_cursor = g_forge.props_cursor >= props_done ? 0 : g_forge.props_cursor + 1;
		s_undo_hold_fraction = 0.0f;
		if (g_forge.props_cursor >= props_done)
		{
			if ((cur_left && !prev_left) || (cur_right && !prev_right))
				g_forge.props_cursor = g_forge.props_cursor == props_done ? props_undo : props_done;
			if (g_forge.props_cursor == props_done && cur_enter && !prev_enter)
			{
				g_forge.active_modal = _forge_modal_none;
				/* (not the press that places what is held, this frame) */
				prev_enter = 1;
			}
			else if (g_forge.props_cursor == props_undo && cur_enter && !s_undo_hold_done)
			{
				uint32_t now_ms = (uint32_t)system_milliseconds();

				if (s_undo_hold_start_ms == 0)
					s_undo_hold_start_ms = now_ms ? now_ms : 1;
				s_undo_hold_fraction = (float)(now_ms - s_undo_hold_start_ms) / (float)FORGE_DELETE_HOLD_MS;
				if (s_undo_hold_fraction >= 1.0f)
				{
					s_undo_hold_fraction = 0.0f;
					s_undo_hold_done = 1;
					forge_props_undo();
				}
			}
			dir = 0;
		}
		if (!cur_enter || g_forge.props_cursor != props_undo)
		{
			s_undo_hold_start_ms = 0;
			s_undo_hold_done = 0;
		}
		if (dir != 0)
		{
			if (is_volume)
			{
				int row = g_forge.props_cursor < volume_row_count ? volume_rows[g_forge.props_cursor] : -1;

				if (row == _forge_volume_row_rot_snap)
				{
					g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
				}
				else if (row == _forge_volume_row_floor)
				{
					cur_z = 1;
				}
				/* (EFFECT >, SIZE > and BACK open and close their pages on a
				press; the others step left and right) */
				else if (row >= 0 && row != _forge_volume_row_delete &&
					(!FORGE_VOLUME_ROW_IS_PAGE_BUTTON(row) || (cur_enter && !prev_enter)))
				{
					int press = forge_volume_row_step(g_forge.grabbed_live_index, row, dir);
					char what[96];

					if (press == FORGE_VOLUME_PRESS_PAGE_OPENED)
					{
						g_forge.props_cursor = 0;
					}
					else if (press == FORGE_VOLUME_PRESS_PAGE_CLOSED)
					{
						int rows[NUMBER_OF_FORGE_VOLUME_ROWS];
						int count = forge_volume_rows(g_forge.grabbed_live_index, rows), k;

						g_forge.props_cursor = 0;
						for (k = 0; k < count; k++)
						{
							if (rows[k] == forge_volume_page_return_row())
								g_forge.props_cursor = k;
						}
					}
					else if (press && row != _forge_volume_row_keep_ratio)
					{
						forge_volume_describe(g_forge.grabbed_live_index, what, sizeof(what));
						forge_set_status("%s", what);
					}
				}
				else if (row == _forge_volume_row_delete && cur_enter && !prev_enter)
				{
					int map_volume = forge_volume_get(g_forge.grabbed_live_index)->map_index >= 0;

					forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
					if (!map_volume)
					{
						g_forge.grabbed_live_index = -1;
						g_forge.grabbed_object_index = NONE;
						g_forge.active_modal = _forge_modal_none;
						goto update_prev_keys;
					}
				}
			}
			else if (is_ai_route)
			{
				int row = g_forge.props_cursor < route_row_count ? route_rows[g_forge.props_cursor] : -1;

				if (row == _forge_ai_route_row_rot_snap)
				{
					g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
				}
				else if (row == _forge_ai_route_row_floor)
				{
					cur_z = 1;
				}
				else if (row == _forge_ai_route_row_delete && cur_enter && !prev_enter)
				{
					forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
					g_forge.grabbed_live_index = -1;
					g_forge.grabbed_object_index = NONE;
					g_forge.active_modal = _forge_modal_none;
					goto update_prev_keys;
				}
				else if (row >= 0)
				{
					int press = forge_ai_route_row_press(g_forge.grabbed_live_index, row, dir, cur_enter && !prev_enter);

					if (press == FORGE_AI_ROUTE_PRESS_PAGE_OPENED)
					{
						g_forge.props_cursor = 0;
					}
					else if (press == FORGE_AI_ROUTE_PRESS_PAGE_CLOSED)
					{
						int rows[NUMBER_OF_FORGE_AI_ROUTE_ROWS];
						int count = forge_ai_route_rows(g_forge.grabbed_live_index, rows), k;

						for (k = 0; k < count; k++)
						{
							if (rows[k] == _forge_ai_route_row_settings)
								g_forge.props_cursor = k;
						}
					}
					else if (press == FORGE_AI_ROUTE_PRESS_DELETED)
					{
						g_forge.grabbed_live_index = -1;
						g_forge.grabbed_object_index = NONE;
						g_forge.active_modal = _forge_modal_none;
						goto update_prev_keys;
					}
				}
			}
			else if (is_ai_spawn)
			{
				int row = g_forge.props_cursor < ai_row_count ? ai_rows[g_forge.props_cursor] : -1;

				if (row == _forge_ai_row_rot_snap)
				{
					g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
				}
				else if (row == _forge_ai_row_floor)
				{
					cur_z = 1;
				}
				else if (row == _forge_ai_row_delete && cur_enter && !prev_enter)
				{
					forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
					g_forge.grabbed_live_index = -1;
					g_forge.grabbed_object_index = NONE;
					g_forge.active_modal = _forge_modal_none;
					goto update_prev_keys;
				}
				/* (UNIT/SPAWN/GOAL SETTINGS > and BACK open and close their
				pages on a press) */
				else if (row >= 0 && (!FORGE_AI_ROW_IS_PAGE_BUTTON(row) || (cur_enter && !prev_enter)))
				{
					int press = forge_ai_spawn_row_step(g_forge.grabbed_live_index, row, dir);

					if (press == FORGE_AI_ROUTE_PRESS_PAGE_OPENED)
					{
						g_forge.props_cursor = 0;
					}
					else if (press == FORGE_AI_ROUTE_PRESS_PAGE_CLOSED)
					{
						int rows[NUMBER_OF_FORGE_AI_ROWS];
						int count = forge_ai_spawn_rows(g_forge.grabbed_live_index, rows), k;

						g_forge.props_cursor = 0;
						for (k = 0; k < count; k++)
						{
							if (rows[k] == forge_ai_spawn_page_return_row())
								g_forge.props_cursor = k;
						}
					}
				}
			}
			else if (is_tele)
			{
				int role = forge_tele_role(inst);
				int channel = forge_tele_channel(inst);
				char what[96];

				if (g_forge.props_cursor == 0 || g_forge.props_cursor == 1)
				{
					if (g_forge.props_cursor == 0)
					{
						/* (a node sends or receives; a scenery may also be just a model) */
						role = inst->group_tag == 'tele' ? 3 - role : (role + 3 + dir) % 3;
					}
					else
					{
						channel = (channel + FORGE_TELE_CHANNELS + dir) % FORGE_TELE_CHANNELS;
					}
					forge_tele_set(g_forge.grabbed_live_index, role, channel);
					forge_tele_describe(g_forge.grabbed_live_index, what, sizeof(what));
					forge_set_status("%s", what);
				}
				else if (g_forge.props_cursor == 2)
				{
					if (cur_enter && !prev_enter)
					{
						forge_tele_jump(g_forge.grabbed_live_index);
						goto update_prev_keys;
					}
				}
				else if (g_forge.props_cursor == 3)
				{
					g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
				}
				else if (g_forge.props_cursor == 4)
				{
					cur_z = 1;
				}
				else if (g_forge.props_cursor == 5 && cur_enter && !prev_enter)
				{
					forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
					g_forge.grabbed_live_index = -1;
					g_forge.grabbed_object_index = NONE;
					g_forge.active_modal = _forge_modal_none;
					goto update_prev_keys;
				}
				else if (g_forge.props_cursor == 6 && snap_row)
				{
					forge_snap_tag_layout_step(inst->group_tag, inst->tag_path, dir);
				}
			}
			else if (is_ff_spawn)
			{
				int cur_type = FORGE_FF_UNPACK_TYPE(inst->team_designator);
				int cur_rnd  = FORGE_FF_UNPACK_ROUND(inst->team_designator);
				int cur_wav  = FORGE_FF_UNPACK_WAVE(inst->team_designator);
				int cur_route = FF_ROUTE_ID(inst->team_designator);
				if (is_route_point && g_forge.props_cursor <= 2)
				{
					if (g_forge.props_cursor > 0 && cur_enter && !prev_enter)
						forge_ff_route_insert_point(g_forge.grabbed_live_index, g_forge.props_cursor == 2);
				}
				else if (g_forge.props_cursor == 0)
				{
					cur_type = forge_ff_spawn_type_step(cur_type, dir);
					forge_ff_marker_set_usage(g_forge.grabbed_live_index,
						FF_ROUTE_WITH_ID(FORGE_FF_PACK_USAGE(cur_type, cur_rnd, cur_wav), cur_route));
				}
				else if (g_forge.props_cursor == 1)
				{
					cur_rnd = (cur_rnd + NUMBER_OF_FORGE_FF_ROUND_FILTERS + dir) % NUMBER_OF_FORGE_FF_ROUND_FILTERS;
					forge_ff_marker_set_usage(g_forge.grabbed_live_index,
						FF_ROUTE_WITH_ID(FORGE_FF_PACK_USAGE(cur_type, cur_rnd, cur_wav), cur_route));
				}
				else if (g_forge.props_cursor == 2)
				{
					cur_wav = (cur_wav + NUMBER_OF_FORGE_FF_WAVE_FILTERS + dir) % NUMBER_OF_FORGE_FF_WAVE_FILTERS;
					forge_ff_marker_set_usage(g_forge.grabbed_live_index,
						FF_ROUTE_WITH_ID(FORGE_FF_PACK_USAGE(cur_type, cur_rnd, cur_wav), cur_route));
				}
				else if (is_route_point && g_forge.props_cursor == 6)
				{
					int32_t zone_idx = forge_ff_route_zone(cur_route);

					if (cur_enter && !prev_enter && zone_idx >= 0)
						forge_ff_route_test_flight(zone_idx);
				}
				else if (is_zone && g_forge.props_cursor >= FORGE_FF_ZONE_FIRST_ROW)
				{
					switch (forge_ff_zone_row_at(inst, g_forge.props_cursor))
					{
					case _forge_ff_zone_row_route:
						if (cur_route != 0)
							forge_ff_route_set_automatic(g_forge.grabbed_live_index);
						else
							forge_ff_route_set_custom(g_forge.grabbed_live_index);
						break;
					case _forge_ff_zone_row_arrival:
					case _forge_ff_zone_row_departure:
					{
						int leg = forge_ff_zone_row_at(inst, g_forge.props_cursor) == _forge_ff_zone_row_arrival ?
							FF_ROUTE_APPROACH : FF_ROUTE_DEPARTURE;

						forge_ff_route_set_leg_style(cur_route, leg,
							(forge_ff_route_leg_style(cur_route, leg) + FF_ROUTE_STYLE_COUNT + dir) % FF_ROUTE_STYLE_COUNT);
						break;
					}
					case _forge_ff_zone_row_factions:
						s_props_factions_open = 1;
						g_forge.props_cursor = 0;
						break;
					case _forge_ff_zone_row_test_drop:
						if (cur_enter && !prev_enter)
							forge_ff_route_test_flight(g_forge.grabbed_live_index);
						break;
					case _forge_ff_zone_row_test_flood:
						if (cur_enter && !prev_enter)
							forge_ff_zone_test_flood(g_forge.grabbed_live_index);
						break;
					default:
						break;
					}
				}
				else if (g_forge.props_cursor == 3)
				{
					g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
				}
				else if (g_forge.props_cursor == 4)
				{
					cur_z = 1;
				}
				else if (g_forge.props_cursor == 5 && cur_enter && !prev_enter)
				{
					forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
					g_forge.grabbed_live_index = -1;
					g_forge.grabbed_object_index = NONE;
					g_forge.active_modal = _forge_modal_none;
					goto update_prev_keys;
				}
			}
			else if (g_forge.props_cursor == 0)
			{
				if (inst->group_tag == 'spwn')
				{
					/* Cycle Player Spawn Team: 0 = Red Team (Red), 1 = Blue Team (Blue), 2 = FFA (Yellow) */
					int cur_t = (inst->team_designator >= 0 && inst->team_designator <= 2) ? (int)inst->team_designator : 2;
					inst->team_designator = (int16_t)((cur_t + 3 + dir) % 3);
					sync_live_player_spawns_to_scenario();
				}
				else if (inst->group_tag == 'flag' &&
					(inst->netgame_flag_type == _netgame_flag_ctf_flag || inst->netgame_flag_type == _netgame_flag_ctf_vehicle))
				{
					/* Cycle CTF Flag Team: 0 = Red Team, 1 = Blue Team */
					int cur_t = (inst->team_designator == 1) ? 1 : 0;
					inst->team_designator = (int16_t)((cur_t + 2 + dir) % 2);
					sync_live_netgame_flags_to_scenario();
				}
				else if (inst->group_tag == 'flag' &&
					(inst->netgame_flag_type == _netgame_flag_race_track || inst->netgame_flag_type == _netgame_flag_race_vehicle))
				{
					/* Race Checkpoint / Vehicle Order ID (0..31) */
					inst->team_designator = (int16_t)((inst->team_designator + 32 + dir) % 32);
					sync_live_netgame_flags_to_scenario();
				}
				else if (inst->group_tag == 'effe')
				{
					/* (an effect spot's REPEAT, which the other machines hear of: !FU:) */
					int16_t usage = (int16_t)forge_effect_spot_seconds_step(forge_effect_spot_seconds(inst), dir);

					forge_3d_marker_usage_apply(g_forge.grabbed_live_index, usage);
					forge_network_send_marker_usage(g_forge.grabbed_live_index, usage);
				}
				else
				{
					/* Reference / Teleporter Channel / KOTH Hill / Oddball ID (0..15) */
					inst->team_designator = (int16_t)((inst->team_designator + 16 + dir) % 16);
					if (inst->group_tag != 'spwn' && inst->group_tag != 'tele' &&
						inst->group_tag != 'flag' && inst->group_tag != 'ffsp' &&
						!inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
						inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
					{
						g_forge.edit_state.placed_objects[inst->edit_entry_index].team_index = inst->team_designator;
					}
					if (inst->group_tag == 'tele' || inst->group_tag == 'flag')
						sync_live_netgame_flags_to_scenario();
				}
			}
			else if (g_forge.props_cursor == 1)
			{
				/* Point 13: Toggle which gametype modes this item will default spawn on */
				inst->gametype_flags = cycle_gametype_flags(inst->gametype_flags, dir);
				if (inst->group_tag == 'spwn')
				{
					sync_live_player_spawns_to_scenario();
				}
				else if (inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp')
				{
					if (!inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
						inst->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
					{
						g_forge.edit_state.placed_objects[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
					}
					if (inst->is_default_scenario_object && inst->edit_entry_index >= 0 &&
						inst->edit_entry_index < (int)g_forge.edit_state.default_edit_count)
					{
						g_forge.edit_state.default_edits[inst->edit_entry_index].gametype_flags = inst->gametype_flags;
					}
				}
			}
			else if (g_forge.props_cursor == 2)
			{
				g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + NUMBER_OF_FORGE_SNAP_MODES + dir) % NUMBER_OF_FORGE_SNAP_MODES);
			}
			else if (g_forge.props_cursor == 3)
			{
				/* Place on ground */
				cur_z = 1;
			}
			else if (g_forge.props_cursor == 4 && cur_enter && !prev_enter)
			{
				forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
				g_forge.grabbed_live_index = -1;
				g_forge.grabbed_object_index = NONE;
				g_forge.active_modal = _forge_modal_none;
				goto update_prev_keys;
			}
			else if (g_forge.props_cursor == 5 && snap_row)
			{
				forge_snap_tag_layout_step(inst->group_tag, inst->tag_path, dir);
			}
			else if (g_forge.props_cursor == test_drop_row && cur_enter && !prev_enter)
			{
				forge_drop_area_test_drop(g_forge.grabbed_live_index);
			}
			else if (g_forge.props_cursor == scale_row && scale_row >= 0)
			{
				float scale = forge_scale_step(forge_inst_scale(inst), dir);

				forge_3d_scale_apply(g_forge.grabbed_live_index, scale);
				forge_network_send_scale(g_forge.grabbed_live_index, forge_inst_scale(inst));
			}
			else if (g_forge.props_cursor == collision_row && collision_row >= 0)
			{
				forge_3d_no_collision_apply(g_forge.grabbed_live_index, !inst->no_collision);
				forge_network_send_no_collision(g_forge.grabbed_live_index, inst->no_collision);
				forge_set_status("Collision %s", inst->no_collision ?
					"off: seen, but nothing stands on it or hits it" : "on");
			}
			else if (g_forge.props_cursor == damage_row && damage_row >= 0)
			{
				struct object_datum *emitter = forge_valid_object_get(inst->object_index);
				int deals = emitter && forge_volumes_tag_deals_damage(emitter->definition_index);

				forge_3d_no_damage_apply(g_forge.grabbed_live_index, !inst->no_damage);
				forge_network_send_no_damage(g_forge.grabbed_live_index, inst->no_damage);
				forge_set_status("Deal damage %s%s", inst->no_damage ? "off: its fire burns no one" : "on",
					deals || inst->no_damage ? "" : " (this emitter's effects deal none)");
			}
			else if (g_forge.props_cursor == always_row)
			{
				forge_3d_always_spawn_apply(g_forge.grabbed_live_index, !inst->always_spawn);
				forge_network_send_always_spawn(g_forge.grabbed_live_index, inst->always_spawn);
				forge_set_status("Always spawn %s: %s", inst->always_spawn ? "on" : "off",
					inst->always_spawn ? "made in every game, whatever its gametype's vehicles" :
					"made as the gametype's vehicles allow");
			}
		}
		if (cur_z && g_forge.grabbed_for_settings)
			forge_settings_selection_drop_to_floor();
	}

	if (g_forge.mode == _forge_mode_3d_monitor)
	{
		float boost = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ? 2.5f : 1.0f;
		/* (and the map's MONITOR SPEED: a big map's flown faster) */
		float spd = g_forge.cam_speed * g_forge.controller_fly_speed * boost * dt * forge_map_settings_monitor_speed();
		float dyaw = 0.0f, dpitch = 0.0f;
		int rot_holding = 0;
		real_vector3d fwd, up, right_vec;

		if (g_forge.active_modal == _forge_modal_none && cur_rot)
		{
			if (g_forge.grabbed_live_index >= 0)
			{
				s_hover_rot_live_idx = -1;
				rot_holding = 1;
			}
			else
			{
				if (!prev_rot || s_hover_rot_live_idx < 0 ||
					s_hover_rot_live_idx >= g_forge.live_instance_count ||
					!g_forge.live_instances[s_hover_rot_live_idx].active)
				{
					int32_t cand = -1;
					if (g_forge.hovered_live_index >= 0 &&
						g_forge.hovered_live_index < g_forge.live_instance_count &&
						g_forge.live_instances[g_forge.hovered_live_index].active)
					{
						cand = g_forge.hovered_live_index;
					}
					else if (g_forge.hovered_object_index != NONE)
					{
						cand = adopt_default_scenario_object(g_forge.hovered_object_index);
					}
					s_hover_rot_live_idx = cand;
					if (cand >= 0)
					{
						struct forge_live_instance *inst = &g_forge.live_instances[cand];
						struct object_datum *obj = forge_valid_object_get(inst->object_index);
						if (obj)
						{
							inst->x = obj->object.position.x;
							inst->y = obj->object.position.y;
							inst->z = obj->object.position.z;
							forward_up_to_euler(&obj->object.forward, &obj->object.up, &inst->yaw, &inst->pitch, &inst->roll);
						}
						g_forge.grab_yaw = inst->yaw;
						g_forge.grab_pitch = inst->pitch;
						g_forge.grab_roll = inst->roll;
						forge_history_begin(cand);
					}
				}
				if (s_hover_rot_live_idx >= 0)
					rot_holding = 1;
			}
		}
		else
		{
			if (s_hover_rot_live_idx >= 0 &&
				s_hover_rot_live_idx < g_forge.live_instance_count &&
				g_forge.live_instances[s_hover_rot_live_idx].active)
			{
				struct forge_live_instance *inst = &g_forge.live_instances[s_hover_rot_live_idx];
				forge_3d_move_instance_ext(
					s_hover_rot_live_idx,
					inst->x, inst->y, inst->z,
					inst->yaw, inst->pitch, inst->roll,
					0, 1);
				forge_history_end(s_hover_rot_live_idx);
			}
			s_hover_rot_live_idx = -1;
		}

		/* Native Mouse Look + Controller Right Stick Look (when not holding rotate modifier) */
		halo_linux_mouse_look(0, &dyaw, &dpitch);
		/* (and the touch controls' swipe and the gyroscope, player_control.c's
		turn: read here, as the mouse is, for the Monitor's camera) */
		{
			float touch_yaw, touch_pitch, gyro_yaw, gyro_pitch;

			if (halo_linux_touch_look(0, &touch_yaw, &touch_pitch, &gyro_yaw, &gyro_pitch))
			{
				if (input_abstraction_port_look_inverted(0))
					touch_pitch = -touch_pitch;
				dyaw += touch_yaw + gyro_yaw;
				dpitch += touch_pitch + gyro_pitch;
			}
		}
		if (!rot_holding && ((float)fabs((double)gp_rx) > 0.001f || (float)fabs((double)gp_ry) > 0.001f))
		{
			float pad_sens = 2.6f * g_forge.controller_look_sens;
			dyaw -= gp_rx * pad_sens * dt;
			dpitch += gp_ry * pad_sens * dt;
		}

		if (rot_holding)
		{
			float pad_rot_sens = 2.6f * g_forge.controller_look_sens;
			/* Holding Left Alt (or Right Trigger on controller) locks camera look and rotates the item:
			 * Mouse / Left Stick -> Pitch & Yaw, [Q/E] / Right Stick -> Roll, [C] -> Reset Pitch/Roll */
			if (cur_rot && !prev_rot)
			{
				forge_set_status("Rotating Item");
			}
			g_forge.grab_yaw += dyaw * 1.4f;
			g_forge.grab_pitch += dpitch * 1.4f;
			if ((float)fabs((double)gp_lx) > 0.001f || (float)fabs((double)gp_ly) > 0.001f)
			{
				g_forge.grab_yaw -= gp_lx * pad_rot_sens * dt * 1.4f;
				g_forge.grab_pitch += gp_ly * pad_rot_sens * dt * 1.4f;
			}
			if ((float)fabs((double)gp_rx) > 0.001f)
			{
				g_forge.grab_roll += gp_rx * pad_rot_sens * dt * 1.4f;
			}
			if (GetAsyncKeyState(VK_LEFT) & 0x8000)  g_forge.grab_yaw += 2.0f * dt;
			if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_forge.grab_yaw -= 2.0f * dt;
			if (GetAsyncKeyState(VK_UP) & 0x8000)    g_forge.grab_pitch += 2.0f * dt;
			if (GetAsyncKeyState(VK_DOWN) & 0x8000)  g_forge.grab_pitch -= 2.0f * dt;
			if (GetAsyncKeyState('Q') & 0x8000)
				g_forge.grab_roll -= 2.0f * dt;
			if (GetAsyncKeyState('E') & 0x8000)
				g_forge.grab_roll += 2.0f * dt;
			if (GetAsyncKeyState('C') & 0x8000)
			{
				g_forge.grab_pitch = 0.0f;
				g_forge.grab_roll = 0.0f;
			}
			while (g_forge.grab_yaw > 3.14159265f)   g_forge.grab_yaw -= 6.283185307f;
			while (g_forge.grab_yaw < -3.14159265f)  g_forge.grab_yaw += 6.283185307f;
			while (g_forge.grab_pitch > 3.14159265f) g_forge.grab_pitch -= 6.283185307f;
			while (g_forge.grab_pitch < -3.14159265f) g_forge.grab_pitch += 6.283185307f;
			while (g_forge.grab_roll > 3.14159265f)  g_forge.grab_roll -= 6.283185307f;
			while (g_forge.grab_roll < -3.14159265f) g_forge.grab_roll += 6.283185307f;

			if (g_forge.grabbed_live_index < 0 && s_hover_rot_live_idx >= 0 &&
				s_hover_rot_live_idx < g_forge.live_instance_count &&
				g_forge.live_instances[s_hover_rot_live_idx].active)
			{
				struct forge_live_instance *inst = &g_forge.live_instances[s_hover_rot_live_idx];
				float syaw = snap_angle_rad(g_forge.grab_yaw, g_forge.snap_mode);
				float spitch = snap_angle_rad(g_forge.grab_pitch, g_forge.snap_mode);
				float sroll = snap_angle_rad(g_forge.grab_roll, g_forge.snap_mode);
				forge_3d_move_instance_ext(
					s_hover_rot_live_idx,
					inst->x, inst->y, inst->z,
					syaw, spitch, sroll,
					1, 1);
			}
		}
		else
		{
			g_forge.cam_yaw += dyaw;
			g_forge.cam_pitch += dpitch;
			if (g_forge.grabbed_live_index >= 0)
				g_forge.grab_yaw += dyaw;

			/* Optional keyboard fine rotation when modal is closed */
			if (g_forge.active_modal == _forge_modal_none)
			{
				if (GetAsyncKeyState(VK_LEFT) & 0x8000)
				{
					g_forge.cam_yaw += 1.6f * dt;
					if (g_forge.grabbed_live_index >= 0)
						g_forge.grab_yaw += 1.6f * dt;
				}
				if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
				{
					g_forge.cam_yaw -= 1.6f * dt;
					if (g_forge.grabbed_live_index >= 0)
						g_forge.grab_yaw -= 1.6f * dt;
				}
				if (GetAsyncKeyState(VK_UP) & 0x8000) g_forge.cam_pitch += 1.2f * dt;
				if (GetAsyncKeyState(VK_DOWN) & 0x8000) g_forge.cam_pitch -= 1.2f * dt;
			}
		}
		if (g_forge.cam_pitch > 1.48f) g_forge.cam_pitch = 1.48f;
		if (g_forge.cam_pitch < -1.48f) g_forge.cam_pitch = -1.48f;

		euler_to_forward_up(g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll, &fwd, &up);
		right_vec.i = (float)sin((double)g_forge.cam_yaw);
		right_vec.j = -(float)cos((double)g_forge.cam_yaw);
		right_vec.k = 0.0f;

		/* 6DOF Monitor Flight (WASD / Left Stick + Space/E/RB up + Ctrl/Q/LB down) with non-sticking BSP collision & wall sliding */
		{
			float move_dx = 0.0f, move_dy = 0.0f, move_dz = 0.0f;
			if (GetAsyncKeyState('W') & 0x8000)
			{
				move_dx += fwd.i * spd;
				move_dy += fwd.j * spd;
				move_dz += fwd.k * spd;
			}
			if (GetAsyncKeyState('S') & 0x8000)
			{
				move_dx -= fwd.i * spd;
				move_dy -= fwd.j * spd;
				move_dz -= fwd.k * spd;
			}
			if (GetAsyncKeyState('D') & 0x8000)
			{
				move_dx += right_vec.i * spd;
				move_dy += right_vec.j * spd;
			}
			if (GetAsyncKeyState('A') & 0x8000)
			{
				move_dx -= right_vec.i * spd;
				move_dy -= right_vec.j * spd;
			}
			if (g_forge.active_modal == _forge_modal_none && !rot_holding)
			{
				if ((float)fabs((double)gp_ly) > 0.001f)
				{
					move_dx += fwd.i * (gp_ly * spd);
					move_dy += fwd.j * (gp_ly * spd);
					move_dz += fwd.k * (gp_ly * spd);
				}
				if ((float)fabs((double)gp_lx) > 0.001f)
				{
					move_dx += right_vec.i * (gp_lx * spd);
					move_dy += right_vec.j * (gp_lx * spd);
				}
			}
			if ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (!rot_holding && (GetAsyncKeyState('E') & 0x8000)) ||
				(g_forge.active_modal == _forge_modal_none && forge_gp_button_down(FORGE_GP_RB)))
			{
				move_dz += spd;
			}
			if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (!rot_holding && (GetAsyncKeyState('Q') & 0x8000)) ||
				(g_forge.active_modal == _forge_modal_none && forge_gp_button_down(FORGE_GP_LB)))
			{
				move_dz -= spd;
			}

			if (move_dx != 0.0f || move_dy != 0.0f || move_dz != 0.0f)
			{
				float move_len = (float)sqrt((double)(move_dx * move_dx + move_dy * move_dy + move_dz * move_dz));
				if (move_len > 0.0001f)
				{
					real_point3d cur_pt, pt1, pt2, pt3;
					real_plane3d plane1, plane2;
					float pad = 0.28f;
					struct structure_bsp const *bsp = global_structure_bsp_get();
					int hit1;

					cur_pt.x = g_forge.cam_x;
					cur_pt.y = g_forge.cam_y;
					cur_pt.z = g_forge.cam_z;

					/* Keep camera at least 0.24u clear of any surrounding walls/floors/ceilings before moving */
					forge_monitor_enforce_wall_clearance(&cur_pt, 0.24f);

					memset(&plane1, 0, sizeof(plane1));
					hit1 = forge_monitor_try_step(&cur_pt, move_dx, move_dy, move_dz, pad, &pt1, &plane1);
					if (hit1)
					{
						float step1_dx = pt1.x - cur_pt.x;
						float step1_dy = pt1.y - cur_pt.y;
						float step1_dz = pt1.z - cur_pt.z;
						float moved1 = (float)sqrt((double)(step1_dx * step1_dx + step1_dy * step1_dy + step1_dz * step1_dz));
						float rem_frac = (move_len > moved1) ? ((move_len - moved1) / move_len) : 0.0f;
						float vn1 = move_dx * plane1.n.i + move_dy * plane1.n.j + move_dz * plane1.n.k;

						cur_pt = pt1;
						if (vn1 < 0.0f && rem_frac > 0.01f)
						{
							float slide_dx = (move_dx - vn1 * plane1.n.i) * rem_frac;
							float slide_dy = (move_dy - vn1 * plane1.n.j) * rem_frac;
							float slide_dz = (move_dz - vn1 * plane1.n.k) * rem_frac;
							int hit2;

							memset(&plane2, 0, sizeof(plane2));
							hit2 = forge_monitor_try_step(&cur_pt, slide_dx, slide_dy, slide_dz, pad, &pt2, &plane2);
							cur_pt = pt2;
							if (hit2)
							{
								float vn2 = slide_dx * plane2.n.i + slide_dy * plane2.n.j + slide_dz * plane2.n.k;
								if (vn2 < 0.0f)
								{
									/* Crease slide along intersection of plane1 and plane2 (verified with full collision_test_vector!) */
									float c_dx = slide_dx - vn2 * plane2.n.i;
									float c_dy = slide_dy - vn2 * plane2.n.j;
									float c_dz = slide_dz - vn2 * plane2.n.k;
									float c_dot1 = c_dx * plane1.n.i + c_dy * plane1.n.j + c_dz * plane1.n.k;
									c_dx -= c_dot1 * plane1.n.i;
									c_dy -= c_dot1 * plane1.n.j;
									c_dz -= c_dot1 * plane1.n.k;
									if (c_dx * c_dx + c_dy * c_dy + c_dz * c_dz > 0.00001f)
									{
										forge_monitor_try_step(&cur_pt, c_dx, c_dy, c_dz, pad, &pt3, NULL);
										cur_pt = pt3;
									}
								}
							}
						}

						/* If primary + slide made zero progress in a tight corner, try verified axis-separated steps */
						if ((cur_pt.x - g_forge.cam_x) * (cur_pt.x - g_forge.cam_x) +
							(cur_pt.y - g_forge.cam_y) * (cur_pt.y - g_forge.cam_y) +
							(cur_pt.z - g_forge.cam_z) * (cur_pt.z - g_forge.cam_z) < 0.000004f)
						{
							forge_monitor_try_step(&cur_pt, move_dx, move_dy, 0.0f, pad, &pt2, NULL);
							cur_pt = pt2;
							forge_monitor_try_step(&cur_pt, 0.0f, 0.0f, move_dz, pad, &pt3, NULL);
							cur_pt = pt3;
						}
					}
					else
					{
						cur_pt = pt1;
					}

					/* Post-step 6-axis wall clearance so the camera never ends a frame closer than 0.24u to any wall */
					forge_monitor_enforce_wall_clearance(&cur_pt, 0.24f);

					/* Clamp inside structure BSP world_bounds */
					if (bsp)
					{
						float margin_xy = 0.28f;
						float margin_z_low = 0.28f;
						float margin_z_high = 0.50f;
						if (bsp->world_bounds.x1 - bsp->world_bounds.x0 > 2.0f)
						{
							if (cur_pt.x < bsp->world_bounds.x0 + margin_xy)
								cur_pt.x = bsp->world_bounds.x0 + margin_xy;
							if (cur_pt.x > bsp->world_bounds.x1 - margin_xy)
								cur_pt.x = bsp->world_bounds.x1 - margin_xy;
						}
						if (bsp->world_bounds.y1 - bsp->world_bounds.y0 > 2.0f)
						{
							if (cur_pt.y < bsp->world_bounds.y0 + margin_xy)
								cur_pt.y = bsp->world_bounds.y0 + margin_xy;
							if (cur_pt.y > bsp->world_bounds.y1 - margin_xy)
								cur_pt.y = bsp->world_bounds.y1 - margin_xy;
						}
						if (bsp->world_bounds.z1 - bsp->world_bounds.z0 > 2.0f)
						{
							if (cur_pt.z < bsp->world_bounds.z0 + margin_z_low)
								cur_pt.z = bsp->world_bounds.z0 + margin_z_low;
							if (cur_pt.z > bsp->world_bounds.z1 - margin_z_high)
								cur_pt.z = bsp->world_bounds.z1 - margin_z_high;
						}
					}

					if (scenario_leaf_index_from_point(&cur_pt) != NONE)
					{
						g_forge.cam_x = cur_pt.x;
						g_forge.cam_y = cur_pt.y;
						g_forge.cam_z = cur_pt.z;
					}
				}
			}
		}

		/* Points 1, 2 & 8: Sync local player's unit position & facing to Monitor position AND keep Invulnerable! */
		{
			long unit_idx = player_control_get_unit_index(0);
			if (unit_idx != NONE)
			{
				forge_sync_unit_to_monitor_pose(
					unit_idx,
					g_forge.cam_x, g_forge.cam_y, g_forge.cam_z,
					g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll,
					1, 1, 0.0f);
			}
		}

		/* Point 4: Perform Physics Beam Raycast + Ray-Cone / Proximity World Object & Marker Scanner (strictly within 20m!) */
		{
			real_point3d ray_origin;
			real_vector3d ray_vec;
			struct collision_result coll;
			unsigned long cflags = FLAG(_collision_test_structure_bit) |
				FLAG(_collision_test_objects_bit) |
				_collision_test_objects_all_types_flags |
				FLAG(_collision_test_front_facing_surfaces_bit);
			long local_unit = player_control_get_unit_index(0);
			long ignore_obj = g_forge.grabbed_object_index;
			float best_obj_score = 4.5f;
			float best_marker_score = 1.6f;
			int best_marker_idx = -1;
			int direct_obj_hit = 0;
			/* (how far along the view the first surface is: nothing past it is selected) */
			float surface_distance = 1000000.0f;

			if (!forge_valid_object_get(ignore_obj))
				ignore_obj = local_unit;
			if (!forge_valid_object_get(ignore_obj))
				ignore_obj = NONE;

			ray_origin.x = g_forge.cam_x;
			ray_origin.y = g_forge.cam_y;
			ray_origin.z = g_forge.cam_z;
			/* Point 4: Limit interaction raycast strictly to 20m */
			ray_vec.i = fwd.i * 20.0f;
			ray_vec.j = fwd.j * 20.0f;
			ray_vec.k = fwd.k * 20.0f;

			g_forge.hovered_live_index = -1;
			memset(&coll, 0, sizeof(coll));
			if (forge_collision_test_vector(cflags, &ray_origin, &ray_vec, ignore_obj, &coll))
			{
				g_forge.ray_hit_valid = 1;
				g_forge.ray_hit_x = coll.point.x;
				g_forge.ray_hit_y = coll.point.y;
				g_forge.ray_hit_z = coll.point.z;
				{
					float hit_x = coll.point.x - g_forge.cam_x, hit_y = coll.point.y - g_forge.cam_y,
						hit_z = coll.point.z - g_forge.cam_z;

					surface_distance = (float)sqrt((double)(hit_x * hit_x + hit_y * hit_y + hit_z * hit_z));
				}
				g_forge.hovered_object_index = (coll.type == _collision_result_object) ? coll.object_index : NONE;
				if (g_forge.hovered_object_index == local_unit || surface_distance > FORGE_SELECT_RANGE)
					g_forge.hovered_object_index = NONE;
				if (g_forge.hovered_object_index != NONE)
				{
					direct_obj_hit = 1;
					best_obj_score = 0.0f;
				}
			}
			else
			{
				g_forge.ray_hit_valid = 1;
				g_forge.ray_hit_x = g_forge.cam_x + fwd.i * 8.0f;
				g_forge.ray_hit_y = g_forge.cam_y + fwd.j * 8.0f;
				g_forge.ray_hit_z = g_forge.cam_z + fwd.k * 8.0f;
				g_forge.hovered_object_index = NONE;
			}

			/* Secondary ray-cone & ray-endpoint scan over all map objects within 20m so weapons, equipment & scenery are selectable! */
			if (g_forge.hovered_object_index == NONE)
			{
				struct object_iterator iter;
				struct object_datum *obj;
				unsigned long edit_mask = _object_mask_scenery | _object_mask_vehicle |
					_object_mask_weapon | _object_mask_equipment | _object_mask_biped | _object_mask_device;
				long best_obj = NONE;

				object_iterator_new(&iter, edit_mask, 0);
				while ((obj = object_iterator_next(&iter)) != NULL)
				{
					long oidx = iter.index;
					struct unit_datum *u;
					float vx, vy, vz, cam_d2, dist_along, px, py, pz, perp_d2, hit_dx, hit_dy, hit_dz, hit_d2;

					if (oidx == local_unit || oidx == g_forge.grabbed_object_index)
						continue;
					if (obj->object.parent_object_index != NONE)
						continue;
					u = (struct unit_datum *)object_try_and_get_and_verify_type(oidx, _object_mask_biped);
					if (u && u->unit.player_index != NONE)
						continue;

					vx = obj->object.position.x - g_forge.cam_x;
					vy = obj->object.position.y - g_forge.cam_y;
					vz = obj->object.position.z - g_forge.cam_z;
					cam_d2 = vx * vx + vy * vy + vz * vz;
					if (cam_d2 > FORGE_SELECT_RANGE * FORGE_SELECT_RANGE)
						continue;

					dist_along = vx * fwd.i + vy * fwd.j + vz * fwd.k;
					if (dist_along > 0.6f && dist_along <= surface_distance + 1.0f)
					{
						px = vx - fwd.i * dist_along;
						py = vy - fwd.j * dist_along;
						pz = vz - fwd.k * dist_along;
						perp_d2 = px * px + py * py + pz * pz;
						if (perp_d2 < 1.8f && perp_d2 < best_obj_score &&
							(forge_point_in_sight(obj->object.bounding_sphere_center.x, obj->object.bounding_sphere_center.y,
								obj->object.bounding_sphere_center.z) ||
							forge_point_in_sight(obj->object.position.x, obj->object.position.y, obj->object.position.z + 0.3f)))
						{
							best_obj_score = perp_d2;
							best_obj = oidx;
						}
					}

					hit_dx = obj->object.position.x - g_forge.ray_hit_x;
					hit_dy = obj->object.position.y - g_forge.ray_hit_y;
					hit_dz = obj->object.position.z - g_forge.ray_hit_z;
					hit_d2 = hit_dx * hit_dx + hit_dy * hit_dy + hit_dz * hit_dz;
					if (hit_d2 < 3.2f && hit_d2 < best_obj_score &&
						(forge_point_in_sight(obj->object.bounding_sphere_center.x, obj->object.bounding_sphere_center.y,
							obj->object.bounding_sphere_center.z) ||
						forge_point_in_sight(obj->object.position.x, obj->object.position.y, obj->object.position.z + 0.3f)))
					{
						best_obj_score = hit_d2;
						best_obj = oidx;
					}
				}
				g_forge.hovered_object_index = best_obj;
			}

			/* Scan virtual markers (Player Spawns, CTF Flags, Oddballs, KOTH Hills, Race Nodes, Teleporters) within 20m!
			 * Compare marker distance against indirect object scan so nearby rocks/weapons never block selecting spawns/flags! */
			{
				int i;
				for (i = 0; i < g_forge.live_instance_count; i++)
				{
					struct forge_live_instance const *inst = &g_forge.live_instances[i];
					float vx, vy, vz, cam_d2, dist_along, px, py, pz, perp_d2, hit_dx, hit_dy, hit_dz, hit_d2;
					if (!inst->active || (inst->object_index != NONE && inst->group_tag != FORGE_GROUP_AI_SPAWN &&
						!forge_origin_group(inst->group_tag)) || i == g_forge.grabbed_live_index)
						continue;
					vx = inst->x - g_forge.cam_x;
					vy = inst->y - g_forge.cam_y;
					vz = (inst->z + 0.15f) - g_forge.cam_z;
					cam_d2 = vx * vx + vy * vy + vz * vz;
					if (cam_d2 > FORGE_SELECT_RANGE * FORGE_SELECT_RANGE)
						continue;

					dist_along = vx * fwd.i + vy * fwd.j + vz * fwd.k;
					if (dist_along > 0.5f && dist_along <= surface_distance + 1.0f)
					{
						px = vx - fwd.i * dist_along;
						py = vy - fwd.j * dist_along;
						pz = vz - fwd.k * dist_along;
						perp_d2 = px * px + py * py + pz * pz;
						if (perp_d2 < 1.2f && perp_d2 < best_marker_score &&
							forge_point_in_sight(inst->x, inst->y, inst->z + 0.15f))
						{
							best_marker_score = perp_d2;
							best_marker_idx = i;
						}
					}

					hit_dx = inst->x - g_forge.ray_hit_x;
					hit_dy = inst->y - g_forge.ray_hit_y;
					hit_dz = inst->z - g_forge.ray_hit_z;
					hit_d2 = hit_dx * hit_dx + hit_dy * hit_dy + hit_dz * hit_dz;
					if (hit_d2 < 1.2f && hit_d2 < best_marker_score &&
						forge_point_in_sight(inst->x, inst->y, inst->z + 0.15f))
					{
						best_marker_score = hit_d2;
						best_marker_idx = i;
					}
				}

				if (best_marker_idx >= 0 && (!direct_obj_hit || best_marker_score < 0.35f) &&
					best_marker_score <= best_obj_score + 0.25f)
				{
					g_forge.hovered_live_index = best_marker_idx;
					g_forge.hovered_object_index = NONE;
				}
			}

			/* A volume by its edges, when nothing else is pointed at: its middle
			may be far off, in a wall or under the floor. As far as the first
			surface the view meets, 30 world units at most */
			s_volume_pick.live_idx = -1;
			if (g_forge.hovered_live_index < 0 && g_forge.hovered_object_index == NONE)
			{
				float origin[3], direction[3], point[3];
				int32_t picked;

				origin[0] = g_forge.cam_x;
				origin[1] = g_forge.cam_y;
				origin[2] = g_forge.cam_z;
				direction[0] = fwd.i;
				direction[1] = fwd.j;
				direction[2] = fwd.k;
				picked = forge_volumes_pick_edge(origin, direction,
					surface_distance < 30.0f ? surface_distance + 0.5f : 30.0f, point);
				if (picked >= 0 && picked != g_forge.grabbed_live_index)
				{
					g_forge.hovered_live_index = picked;
					s_volume_pick.live_idx = picked;
					s_volume_pick.point[0] = point[0];
					s_volume_pick.point[1] = point[1];
					s_volume_pick.point[2] = point[2];
				}
			}

			/* Update hovered_object_label for HUD display (including Player Spawns, Teleporters & KOTH/Oddball/CTF/Race markers) */
			g_forge.hovered_object_label[0] = 0;
			if (g_forge.hovered_live_index >= 0 && g_forge.hovered_live_index < g_forge.live_instance_count)
			{
				struct forge_live_instance const *inst = &g_forge.live_instances[g_forge.hovered_live_index];
				if (inst->group_tag == 'spwn')
				{
					char const *tname = (inst->team_designator == 1) ? "Blue Team Spawn" :
						(inst->team_designator == 2 ? "FFA Spawn (Yellow)" : "Red Team Spawn");
					snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label), "%s", tname);
				}
				else if (inst->group_tag == FORGE_GROUP_AI_SPAWN)
				{
					forge_ai_spawn_label(inst, g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label));
				}
				else if (inst->group_tag == FORGE_GROUP_VOLUME)
				{
					forge_volume_describe(g_forge.hovered_live_index, g_forge.hovered_object_label,
						sizeof(g_forge.hovered_object_label));
				}
				else if (forge_ai_route_is(inst))
				{
					forge_ai_route_describe(g_forge.hovered_live_index, g_forge.hovered_object_label,
						sizeof(g_forge.hovered_object_label));
				}
				else if (forge_ff_is_route_point(inst))
				{
					snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
						"Dropship Route #%d: %s Point %d",
						FF_ROUTE_ID(inst->team_designator),
						FF_ROUTE_POINT_LEG(inst->team_designator) == FF_ROUTE_APPROACH ? "Approach" : "Departure",
						FF_ROUTE_POINT_ORDER(inst->team_designator) + 1);
				}
				else if (inst->group_tag == 'ffsp')
				{
					snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
						"FF Spawn: %s (%s / %s)",
						forge_ff_spawn_type_label(FORGE_FF_UNPACK_TYPE(inst->team_designator)),
						forge_ff_round_filter_label(FORGE_FF_UNPACK_ROUND(inst->team_designator)),
						forge_ff_wave_filter_label(FORGE_FF_UNPACK_WAVE(inst->team_designator)));
				}
				else if (inst->group_tag == 'tele')
				{
					forge_tele_describe(g_forge.hovered_live_index,
						g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label));
				}
				else if (inst->group_tag == 'flag')
				{
					if (inst->netgame_flag_type == _netgame_flag_ctf_flag)
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"%s CTF Flag Point", inst->team_designator == 1 ? "Blue" : "Red");
					else if (inst->netgame_flag_type == _netgame_flag_oddball_ball_spawn)
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"Oddball Spawn #%d", (int)inst->team_designator);
					else if (inst->netgame_flag_type == _netgame_flag_hill)
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"KOTH Hill Marker #%d", (int)inst->team_designator);
					else if (inst->netgame_flag_type == _netgame_flag_race_track)
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"Race Node #%d", (int)inst->team_designator);
					else if (inst->netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR)
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"Firefight Generator");
					else
					{
						char const *slash = strrchr(inst->tag_path, '\\');
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"%s (#%d)", slash ? (slash + 1) : inst->tag_path, (int)inst->team_designator);
					}
				}
			}
			else if (g_forge.hovered_object_index != NONE)
			{
				struct object_datum *hobj = forge_valid_object_get(g_forge.hovered_object_index);
				if (hobj && hobj->definition_index != NONE)
				{
					float vx = hobj->object.position.x - g_forge.cam_x;
					float vy = hobj->object.position.y - g_forge.cam_y;
					float vz = hobj->object.position.z - g_forge.cam_z;
					if (vx * vx + vy * vy + vz * vz > 400.0f)
					{
						g_forge.hovered_object_index = NONE;
					}
					else
					{
						char const *tname = tag_get_name(hobj->definition_index);
						char const *slash = tname ? strrchr(tname, '\\') : NULL;
						int32_t tele_idx = forge_tele_of_object(g_forge.hovered_object_index);
						snprintf(g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label),
							"%s", slash ? (slash + 1) : (tname ? tname : "map_object"));
						/* (a teleporter's scenery: what it does) */
						if (tele_idx >= 0)
							forge_tele_describe(tele_idx, g_forge.hovered_object_label, sizeof(g_forge.hovered_object_label));
					}
				}
				else
				{
					g_forge.hovered_object_index = NONE;
				}
			}
		}

		/* Cycle Rotation Snap Mode (G: Off / 15 deg / 45 deg / 90 deg) */
		if (cur_g && !prev_g)
		{
			static char const *const snap_names[4] = { "Off (Free)", "15 Degrees", "45 Degrees", "90 Degrees" };
			g_forge.snap_mode = (enum forge_snap_angle_mode)((g_forge.snap_mode + 1) % NUMBER_OF_FORGE_SNAP_MODES);
			forge_set_status("Rotation Snapping: %s", snap_names[g_forge.snap_mode]);
		}

		/* Copy Hovered Object ([C] / Controller D-Pad Right) when not holding an item */
		if (g_forge.active_modal == _forge_modal_none && g_forge.grabbed_live_index < 0 && cur_copy && !prev_copy)
		{
			int32_t src_idx = -1;
			if (g_forge.hovered_live_index >= 0 &&
				g_forge.hovered_live_index < g_forge.live_instance_count &&
				g_forge.live_instances[g_forge.hovered_live_index].active)
			{
				src_idx = g_forge.hovered_live_index;
			}
			else if (g_forge.hovered_object_index != NONE)
			{
				src_idx = adopt_default_scenario_object(g_forge.hovered_object_index);
			}

			if (src_idx >= 0)
			{
				struct forge_live_instance src_copy = g_forge.live_instances[src_idx];
				struct object_datum *sobj = forge_valid_object_get(src_copy.object_index);
				int32_t dup_idx = -1;
				int16_t dup_usage = src_copy.team_designator;
				int dup_allowed = 1;
				int dup_zone_route = 0;

				if (sobj)
				{
					src_copy.x = sobj->object.position.x;
					src_copy.y = sobj->object.position.y;
					src_copy.z = sobj->object.position.z;
					forward_up_to_euler(&sobj->object.forward, &sobj->object.up, &src_copy.yaw, &src_copy.pitch, &src_copy.roll);
				}

				if (forge_ff_is_route_point(&src_copy))
				{
					/* A copied route point is the next point along */
					int route_id = FF_ROUTE_ID(src_copy.team_designator);
					int leg = FF_ROUTE_POINT_LEG(src_copy.team_designator);
					int32_t points[FF_ROUTE_MAX_LEG_POINTS];
					int count = forge_ff_route_leg_points(route_id, leg, points);
					int rank = 0;

					while (rank < count && points[rank] != src_idx)
						rank++;
					dup_allowed = forge_ff_route_make_room(route_id, leg, rank + 1);
					if (dup_allowed)
						dup_usage = FF_ROUTE_POINT_USAGE(route_id, leg, rank + 1, FF_ROUTE_POINT_STYLE(src_copy.team_designator));
					else
						forge_set_status("This Leg of the Route Is Full (%d Points)", FF_ROUTE_MAX_LEG_POINTS);
				}
				else if (forge_ai_route_is(&src_copy))
				{
					/* (a copied AI route point is the next point along: room made for it) */
					dup_usage = forge_ai_route_copy_usage(src_idx);
					dup_allowed = dup_usage != 0;
				}
				else if (forge_ff_is_dropship_zone(&src_copy) && FF_ROUTE_ID(src_copy.team_designator) != 0)
				{
					/* A copied zone starts a route of its own */
					dup_zone_route = forge_ff_route_free_id();
					dup_usage = FF_ROUTE_WITH_ID(src_copy.team_designator, dup_zone_route);
				}

				if (dup_allowed)
				{
					g_forge_next_spawn_always = src_copy.always_spawn;
					g_forge_next_spawn_scale = src_copy.scale;
					/* (a copy of a map's volume is a box of Forge's) */
					dup_idx = forge_3d_spawn_object_at(
						src_copy.group_tag,
						src_copy.group_tag == FORGE_GROUP_VOLUME && !_stricmp(src_copy.tag_path, FORGE_VOLUME_MAP_PATH) ?
							FORGE_VOLUME_BOX_PATH : src_copy.tag_path,
						src_copy.source_cache_id,
						src_copy.x, src_copy.y, src_copy.z,
						src_copy.yaw, src_copy.pitch, src_copy.roll,
						dup_usage,
						1);
					if (dup_idx >= 0 && g_forge.live_instances[dup_idx].always_spawn)
						forge_network_send_always_spawn(dup_idx, 1);
					/* (a volume's shape, size and effect; an emitter's DEAL DAMAGE) */
					if (dup_idx >= 0 && forge_volume_get(src_idx) && forge_volume_get(dup_idx))
					{
						struct forge_volume copy = *forge_volume_get(src_idx);

						if (copy.shape == _forge_volume_map_box)
							copy.shape = _forge_volume_box;
						if (copy.effect == _forge_volume_map_script)
							copy.effect = _forge_volume_insta_kill;
						/* (an AI PATROL copy is a patrol of its own number) */
						if (copy.effect == _forge_volume_ai_patrol)
						{
							int number = forge_volume_patrol_free_number();

							copy.flags = (uint8_t)(copy.flags & ~FORGE_VOLUME_PATROL_MASK);
							if (number > 0)
								copy.flags = (uint8_t)(copy.flags | (number << FORGE_VOLUME_PATROL_SHIFT));
							else
								copy.effect = _forge_volume_off;
						}
						forge_volume_set(dup_idx, &copy, 1);
					}
					if (dup_idx >= 0 && src_copy.no_damage)
					{
						forge_3d_no_damage_apply(dup_idx, 1);
						forge_network_send_no_damage(dup_idx, 1);
					}
					if (dup_idx >= 0 && src_copy.no_collision)
					{
						forge_3d_no_collision_apply(dup_idx, 1);
						forge_network_send_no_collision(dup_idx, 1);
					}
					if (dup_idx >= 0 && g_forge.live_instances[dup_idx].scale > 0.0f)
						forge_network_send_scale(dup_idx, g_forge.live_instances[dup_idx].scale);
				}
				if (dup_idx >= 0)
				{
					struct forge_live_instance *dup = &g_forge.live_instances[dup_idx];
					float dx = dup->x - g_forge.cam_x;
					float dy = dup->y - g_forge.cam_y;
					float dz = dup->z - g_forge.cam_z;
					dup->gametype_flags = src_copy.gametype_flags;
					if (dup->group_tag == FORGE_GROUP_AI_SPAWN)
					{
						forge_ai_spawn_copy_settings(src_copy.edit_entry_index, dup_idx);
					}
					else if (dup->group_tag == 'spwn')
					{
						sync_live_player_spawns_to_scenario();
					}
					else if (dup->group_tag != 'tele' && dup->group_tag != 'flag' && dup->group_tag != 'ffsp' &&
						!dup->is_default_scenario_object && dup->edit_entry_index >= 0 &&
						dup->edit_entry_index < (int)g_forge.edit_state.placed_object_count)
					{
						g_forge.edit_state.placed_objects[dup->edit_entry_index].gametype_flags = dup->gametype_flags;
					}
					g_forge.koth_group_placing = 0;
					g_forge.grabbed_live_index = dup_idx;
					g_forge.grabbed_object_index = dup->object_index;
					g_forge.grabbed_is_new_spawn = 1;
					g_forge.grab_distance = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
					if (g_forge.grab_distance < 1.0f) g_forge.grab_distance = 1.0f;
					if (g_forge.grab_distance > 20.0f) g_forge.grab_distance = 20.0f;
					g_forge.grab_yaw = dup->yaw;
					g_forge.grab_pitch = dup->pitch;
					g_forge.grab_roll = dup->roll;
					if (dup_zone_route != 0 && forge_ff_route_fill(dup_idx) > 0)
						g_forge.route_follow_live_index = dup_idx;
					forge_set_status("Copied '%s'", dup->tag_path);
				}
			}
		}

		/* What the reticle is on, its settings: selected, not picked up, so it stays where it is */
		if (g_forge.active_modal == _forge_modal_none && g_forge.grabbed_live_index < 0 &&
			cur_look_settings && !prev_look_settings)
		{
			int look_idx = -1;

			if (g_forge.hovered_live_index >= 0 && g_forge.hovered_live_index < g_forge.live_instance_count &&
				g_forge.live_instances[g_forge.hovered_live_index].active)
			{
				look_idx = g_forge.hovered_live_index;
			}
			else if (g_forge.hovered_object_index != NONE)
			{
				look_idx = adopt_default_scenario_object(g_forge.hovered_object_index);
			}
			if (look_idx >= 0)
			{
				forge_select_instance(look_idx);
				g_forge.grabbed_for_settings = 1;
				forge_props_open();
			}
		}

		/* Grab / Release / Confirm Placement of Object on Physics Beam (Left Click or Enter when holding) */
		if ((g_forge.active_modal == _forge_modal_none && cur_lmb && !prev_lmb) ||
			(g_forge.active_modal == _forge_modal_none && g_forge.grabbed_live_index >= 0 && cur_enter && !prev_enter))
		{
			if (g_forge.grabbed_live_index >= 0)
			{
				forge_place_held_item();
			}
			else
			{
				int best = -1;

				/* 1. If aiming directly at a virtual marker (Player Spawn, CTF Flag, Oddball, KOTH Hill, Race Node, Teleporter), grab it! */
				if (g_forge.hovered_live_index >= 0 &&
					g_forge.hovered_live_index < g_forge.live_instance_count &&
					g_forge.live_instances[g_forge.hovered_live_index].active)
				{
					best = g_forge.hovered_live_index;
				}

				/* 2. If aiming at any world object within 20m (Forge-placed OR Default Map Scenario object), adopt/select it! */
				if (best < 0 && g_forge.hovered_object_index != NONE)
				{
					best = adopt_default_scenario_object(g_forge.hovered_object_index);
				}

				if (best >= 0)
				{
					struct forge_live_instance *inst = &g_forge.live_instances[best];
					struct object_datum *obj = forge_valid_object_get(inst->object_index);

					forge_select_instance(best);
					if (obj && obj->object.type == _object_type_vehicle)
					{
						forge_eject_all_passengers_from_vehicle(inst->object_index);
						if (forge_is_multiplayer_session())
						{
							forge_network_send_transform_ext(
								best,
								inst->object_index,
								inst->x,
								inst->y,
								inst->z,
								inst->yaw,
								inst->pitch,
								inst->roll,
								1,
								1);
						}
					}
					forge_set_status("Holding '%s'", inst->tag_path);
				}
			}
		}

		/* Update Grabbed Object Transform */
		if (g_forge.grabbed_live_index >= 0 && g_forge.grabbed_live_index < g_forge.live_instance_count &&
			!g_forge.grabbed_for_settings)
		{
			float tx, ty, tz, syaw, spitch, sroll;
			long wheel_notches = 0;
			int radius_mod_held = ((GetAsyncKeyState('C') & 0x8000) || (GetAsyncKeyState(VK_SHIFT) & 0x8000)) != 0;

			/* Scroll wheel moves held item away/towards camera, OR scales KOTH Hill radius */
			platform_scoreboard_scroll(1, &wheel_notches, NULL);
			if (g_forge.koth_group_placing && radius_mod_held && wheel_notches != 0)
			{
				g_forge.koth_group_radius += (float)wheel_notches * (0.25f / 3.048f);
				if (g_forge.koth_group_radius < (0.4f / 3.048f)) g_forge.koth_group_radius = (0.4f / 3.048f);
				if (g_forge.koth_group_radius > (15.0f / 3.048f)) g_forge.koth_group_radius = (15.0f / 3.048f);
				forge_set_status("KOTH Hill #%d Diameter: %.1fm (Radius: %.2fm)",
					g_forge.koth_group_hill_id,
					g_forge.koth_group_radius * 2.0f * 3.048f,
					g_forge.koth_group_radius * 3.048f);
			}
			else if (wheel_notches != 0)
			{
				g_forge.grab_distance += (float)wheel_notches * 1.0f;
			}

			if (g_forge.koth_group_placing)
			{
				if ((GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) || (GetAsyncKeyState(VK_OEM_6) & 0x8000))
				{
					g_forge.koth_group_radius += (1.5f / 3.048f) * dt;
					if (g_forge.koth_group_radius > (15.0f / 3.048f)) g_forge.koth_group_radius = (15.0f / 3.048f);
				}
				if ((GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) || (GetAsyncKeyState(VK_OEM_4) & 0x8000))
				{
					g_forge.koth_group_radius -= (1.5f / 3.048f) * dt;
					if (g_forge.koth_group_radius < (0.4f / 3.048f)) g_forge.koth_group_radius = (0.4f / 3.048f);
				}
			}

			if (GetAsyncKeyState(VK_PRIOR) & 0x8000) g_forge.grab_distance += 8.0f * dt;
			if (GetAsyncKeyState(VK_NEXT) & 0x8000)  g_forge.grab_distance -= 8.0f * dt;
			if (g_forge.grab_distance < 1.0f) g_forge.grab_distance = 1.0f;
			if (g_forge.grab_distance > 30.0f) g_forge.grab_distance = 30.0f;

			/* Keep the placement cursor and held object locked to the camera crosshair ray
			 * (and clamped against any closer BSP/surface hit along the beam) */
			{
				real_point3d beam_orig;
				real_vector3d beam_vec;
				struct collision_result bcoll;
				long ignore_grab = forge_valid_object_get(g_forge.grabbed_object_index) ? g_forge.grabbed_object_index : NONE;
				beam_orig.x = g_forge.cam_x;
				beam_orig.y = g_forge.cam_y;
				beam_orig.z = g_forge.cam_z;
				beam_vec.i = fwd.i * g_forge.grab_distance;
				beam_vec.j = fwd.j * g_forge.grab_distance;
				beam_vec.k = fwd.k * g_forge.grab_distance;
				memset(&bcoll, 0, sizeof(bcoll));
				if (forge_collision_test_vector(
					FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
					&beam_orig, &beam_vec, ignore_grab, &bcoll))
				{
					tx = bcoll.point.x + bcoll.plane.n.i * 0.05f;
					ty = bcoll.point.y + bcoll.plane.n.j * 0.05f;
					tz = bcoll.point.z + bcoll.plane.n.k * 0.05f;
				}
				else
				{
					tx = g_forge.cam_x + fwd.i * g_forge.grab_distance;
					ty = g_forge.cam_y + fwd.j * g_forge.grab_distance;
					tz = g_forge.cam_z + fwd.k * g_forge.grab_distance;
				}
				g_forge.ray_hit_valid = 1;
				g_forge.ray_hit_x = tx;
				g_forge.ray_hit_y = ty;
				g_forge.ray_hit_z = tz;
			}
			/* (a volume taken by its edge: its middle as far from the reticle as it was) */
			if (s_volume_grab.live_idx == g_forge.grabbed_live_index && s_volume_grab.live_idx >= 0)
			{
				tx += s_volume_grab.offset[0];
				ty += s_volume_grab.offset[1];
				tz += s_volume_grab.offset[2];
			}

			/* Hold Z to continuously snap held object to floor, then Click (or Enter) to place on ground! */
			if (cur_z)
			{
				real_point3d down_orig;
				real_vector3d down_vec;
				struct collision_result gcoll;
				long ignore_grab = forge_valid_object_get(g_forge.grabbed_object_index) ? g_forge.grabbed_object_index : NONE;
				down_orig.x = tx;
				down_orig.y = ty;
				down_orig.z = tz + 2.0f;
				down_vec.i = 0.0f;
				down_vec.j = 0.0f;
				down_vec.k = -200.0f;
				memset(&gcoll, 0, sizeof(gcoll));
				if (forge_collision_test_vector(
					FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
					&down_orig, &down_vec, ignore_grab, &gcoll))
				{
					tz = gcoll.point.z + 0.08f;
					/* (a volume stands on the floor, its middle half its height up) */
					if (forge_volume_get(g_forge.grabbed_live_index))
						tz += forge_volume_get(g_forge.grabbed_live_index)->size[2] * 0.5f;
					if (!prev_z)
						forge_set_status("Snapping to Floor (Z=%.2f)", tz);
				}
			}

			/* Hold/Press F to Snap to Surface WITHOUT forcing or flipping item orientation */
			if (cur_f)
			{
				real_point3d surf_orig;
				real_vector3d surf_vec;
				struct collision_result scoll;
				long ignore_grab = forge_valid_object_get(g_forge.grabbed_object_index) ? g_forge.grabbed_object_index : NONE;
				surf_orig.x = g_forge.cam_x;
				surf_orig.y = g_forge.cam_y;
				surf_orig.z = g_forge.cam_z;
				surf_vec.i = fwd.i * 30.0f;
				surf_vec.j = fwd.j * 30.0f;
				surf_vec.k = fwd.k * 30.0f;
				memset(&scoll, 0, sizeof(scoll));
				if (forge_collision_test_vector(
					FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
					&surf_orig, &surf_vec, ignore_grab, &scoll))
				{
					real_vector3d surf_up = scoll.plane.n;
					float sdx, sdy, sdz, sdist;
					tx = scoll.point.x + surf_up.i * 0.05f;
					ty = scoll.point.y + surf_up.j * 0.05f;
					tz = scoll.point.z + surf_up.k * 0.05f;
					g_forge.ray_hit_x = tx;
					g_forge.ray_hit_y = ty;
					g_forge.ray_hit_z = tz;
					sdx = tx - g_forge.cam_x;
					sdy = ty - g_forge.cam_y;
					sdz = tz - g_forge.cam_z;
					sdist = (float)sqrt((double)(sdx * sdx + sdy * sdy + sdz * sdz));
					if (sdist >= 1.0f && sdist <= 30.0f)
						g_forge.grab_distance = sdist;
					if (!prev_f)
						forge_set_status("Snapped to Surface (%.1f, %.1f, %.1f)", tx, ty, tz);
				}
			}

			syaw = snap_angle_rad(g_forge.grab_yaw, g_forge.snap_mode);
			spitch = snap_angle_rad(g_forge.grab_pitch, g_forge.snap_mode);
			sroll = snap_angle_rad(g_forge.grab_roll, g_forge.snap_mode);

			if (g_forge.koth_group_placing)
			{
				int k;
				for (k = 0; k < 10; k++)
				{
					int32_t nidx = g_forge.koth_group_live_indices[k];
					float ang = syaw + (float)k * (6.283185307f / 10.0f);
					float nx = tx + g_forge.koth_group_radius * (float)cos((double)ang);
					float ny = ty + g_forge.koth_group_radius * (float)sin((double)ang);
					float nz = tz;
					if (cur_z)
					{
						real_point3d n_orig;
						real_vector3d n_vec;
						struct collision_result ncoll;
						n_orig.x = nx;
						n_orig.y = ny;
						n_orig.z = tz + 2.0f;
						n_vec.i = 0.0f;
						n_vec.j = 0.0f;
						n_vec.k = -200.0f;
						memset(&ncoll, 0, sizeof(ncoll));
						if (forge_collision_test_vector(
							FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
							&n_orig, &n_vec, NONE, &ncoll))
						{
							nz = ncoll.point.z + 0.04f;
						}
					}
					if (nidx >= 0 && nidx < g_forge.live_instance_count)
						forge_3d_move_instance_ext(nidx, nx, ny, nz, ang, 0.0f, 0.0f, 1, 1);
				}
			}
			else
			{
				forge_eval_magnetic_snap_for_held_instance(
					g_forge.grabbed_live_index,
					&tx,
					&ty,
					&tz,
					&syaw,
					spitch,
					sroll);
				{
					struct forge_live_instance const *held = &g_forge.live_instances[g_forge.grabbed_live_index];
					float from_x = held->x, from_y = held->y, from_z = held->z;

					/* (an AI route's point stays on the ground under it, or
					over it when it is held under a hill: never in the air) */
					if (forge_ai_route_is(held))
					{
						float ground_z;

						if (forge_ai_route_ground(tx, ty, tz, &ground_z))
							tz = ground_z + 0.05f;
						spitch = 0.0f;
						sroll = 0.0f;
					}
					forge_3d_move_instance_ext(g_forge.grabbed_live_index, tx, ty, tz, syaw, spitch, sroll, 1, 1);
					if (g_forge.route_follow_live_index == g_forge.grabbed_live_index)
					{
						forge_ff_route_shift(
							FF_ROUTE_ID(held->team_designator), held->x - from_x, held->y - from_y, held->z - from_z);
					}
				}
			}
		}
		else
		{
			g_forge.snap_point_locked = 0;
			platform_scoreboard_scroll(0, NULL, NULL);
		}

		/* Delete Grabbed Object, once the button has been held */
		s_delete_hold_fraction = 0.0f;
		if (!cur_del || g_forge.grabbed_live_index < 0)
		{
			s_del_hold_start_ms = 0;
			if (!cur_del)
				s_del_hold_done = 0;
		}
		else if (!s_del_hold_done)
		{
			uint32_t now_ms = (uint32_t)system_milliseconds();

			if (s_del_hold_start_ms == 0)
				s_del_hold_start_ms = now_ms ? now_ms : 1;
			s_delete_hold_fraction = (float)(now_ms - s_del_hold_start_ms) / (float)FORGE_DELETE_HOLD_MS;
		}
		if (s_delete_hold_fraction >= 1.0f)
		{
			s_delete_hold_fraction = 0.0f;
			s_del_hold_start_ms = 0;
			s_del_hold_done = 1;
			if (g_forge.koth_group_placing)
			{
				int k;
				for (k = 0; k < 10; k++)
				{
					int32_t nidx = g_forge.koth_group_live_indices[k];
					if (nidx >= 0 && nidx < g_forge.live_instance_count)
						forge_3d_delete_instance(nidx, 1);
				}
				g_forge.koth_group_placing = 0;
			}
			else
			{
				forge_3d_delete_instance(g_forge.grabbed_live_index, 1);
			}
			g_forge.grabbed_live_index = -1;
			g_forge.grabbed_object_index = NONE;
			g_forge.grabbed_is_new_spawn = 0;
		}
	}
	else
	{
		platform_scoreboard_scroll(0, NULL, NULL);
	}

update_prev_keys:
	prev_up = cur_up;
	prev_down = cur_down;
	prev_left = cur_left;
	prev_right = cur_right;
	prev_enter = cur_enter;
	prev_g = cur_g;
	prev_z = cur_z;
	prev_f = cur_f;
	prev_copy = cur_copy;
	prev_look_settings = cur_look_settings;
	prev_rot = cur_rot;
	prev_bk = cur_bk;
	prev_lmb = cur_lmb;
}

static void forge_compute_live_monitor_camera_pose(
	real_point3d *out_pos,
	real_vector3d *out_fwd,
	real_vector3d *out_up)
{
	euler_to_forward_up(g_forge.cam_yaw, g_forge.cam_pitch, g_forge.cam_roll, out_fwd, out_up);
	if (g_forge.third_person_view)
	{
		/* Pull camera back 2.4 world units behind Monitor, clamped against BSP/skybox walls */
		real_point3d mon_pt, cam_pt;
		real_vector3d pullback_vec;
		struct collision_result cam_coll;
		unsigned long cflags = FLAG(_collision_test_structure_bit) |
							   FLAG(_collision_test_front_facing_surfaces_bit) |
							   FLAG(_collision_test_back_facing_surfaces_bit);
		struct structure_bsp const *bsp = global_structure_bsp_get();

		mon_pt.x = g_forge.cam_x;
		mon_pt.y = g_forge.cam_y;
		mon_pt.z = g_forge.cam_z;

		pullback_vec.i = -out_fwd->i * 2.4f;
		pullback_vec.j = -out_fwd->j * 2.4f;
		pullback_vec.k = -out_fwd->k * 2.4f + 0.25f;

		memset(&cam_coll, 0, sizeof(cam_coll));
		if (collision_test_pill(FLAG(_collision_test_structure_bit), &mon_pt, &pullback_vec, 0.22f, NONE, &cam_coll) ||
			collision_test_vector(cflags, &mon_pt, &pullback_vec, NONE, &cam_coll))
		{
			float frac = cam_coll.t - 0.12f;
			if (frac < 0.0f)
				frac = 0.0f;
			cam_pt.x = mon_pt.x + pullback_vec.i * frac;
			cam_pt.y = mon_pt.y + pullback_vec.j * frac;
			cam_pt.z = mon_pt.z + pullback_vec.k * frac;
		}
		else
		{
			cam_pt.x = mon_pt.x + pullback_vec.i;
			cam_pt.y = mon_pt.y + pullback_vec.j;
			cam_pt.z = mon_pt.z + pullback_vec.k;
		}

		if (bsp)
		{
			if (bsp->world_bounds.x1 - bsp->world_bounds.x0 > 2.0f)
			{
				if (cam_pt.x < bsp->world_bounds.x0 + 0.35f) cam_pt.x = bsp->world_bounds.x0 + 0.35f;
				if (cam_pt.x > bsp->world_bounds.x1 - 0.35f) cam_pt.x = bsp->world_bounds.x1 - 0.35f;
			}
			if (bsp->world_bounds.y1 - bsp->world_bounds.y0 > 2.0f)
			{
				if (cam_pt.y < bsp->world_bounds.y0 + 0.35f) cam_pt.y = bsp->world_bounds.y0 + 0.35f;
				if (cam_pt.y > bsp->world_bounds.y1 - 0.35f) cam_pt.y = bsp->world_bounds.y1 - 0.35f;
			}
			if (bsp->world_bounds.z1 - bsp->world_bounds.z0 > 2.0f)
			{
				if (cam_pt.z < bsp->world_bounds.z0 + 0.35f) cam_pt.z = bsp->world_bounds.z0 + 0.35f;
				if (cam_pt.z > bsp->world_bounds.z1 - 0.45f) cam_pt.z = bsp->world_bounds.z1 - 0.45f;
			}
		}

		if (scenario_leaf_index_from_point(&cam_pt) == NONE)
			cam_pt = mon_pt;

		*out_pos = cam_pt;
	}
	else
	{
		out_pos->x = g_forge.cam_x;
		out_pos->y = g_forge.cam_y;
		out_pos->z = g_forge.cam_z;
	}
}

void forge_3d_apply_monitor_camera(short local_player_index)
{
	/* IMPORTANT: observer_set_camera() stores the pointer in observer->pending_command
	 * and observer_update() reads/writes through that pointer AFTER director_update() returns!
	 * Therefore s_cmd MUST be static (never a stack local). */
	static struct observer_command s_cmd;

	if (!g_forge_session_enabled || local_player_index != 0)
		return;
	if (forge_is_main_menu_active())
		return;
	if (g_forge.mode == _forge_mode_play && g_forge.active_modal == _forge_modal_none)
		return;

	director_inhibit_input(0);
	director_inhibit_facing(0);

	if (g_forge.mode == _forge_mode_3d_monitor)
	{
		memset(&s_cmd, 0, sizeof(s_cmd));
		s_cmd.flags = FLAG(_observer_command_valid_bit) | FLAG(_observer_command_ignore_obstructions_bit);
		forge_compute_live_monitor_camera_pose(&s_cmd.focus_position, &s_cmd.forward, &s_cmd.up);
		s_cmd.focus_distance = 0.0f;
		s_cmd.field_of_view = 1.3962634f; /* 80 degrees */
		observer_set_camera(0, &s_cmd);
	}
}

int forge_3d_fill_live_observer_result(void *out_observer_result)
{
	struct observer_result *obs = (struct observer_result *)out_observer_result;
	struct location loc;

	if (!obs || !g_forge_session_enabled || !g_forge.initialized || forge_is_main_menu_active())
		return 0;
	if (g_forge.mode != _forge_mode_3d_monitor)
		return 0;

	forge_compute_live_monitor_camera_pose(&obs->position, &obs->forward, &obs->up);
	obs->velocity.i = 0.0f;
	obs->velocity.j = 0.0f;
	obs->velocity.k = 0.0f;
	scenario_location_from_point(&loc, &obs->position);
	if (loc.leaf_index != NONE)
	{
		obs->location = loc;
	}
	else
	{
		real_point3d mon_pt = { g_forge.cam_x, g_forge.cam_y, g_forge.cam_z };
		scenario_location_from_point(&loc, &mon_pt);
		if (loc.leaf_index != NONE)
			obs->location = loc;
	}
	return 1;
}

/* every route's curve, in the world pass so the level and objects hide it:
gold coming in, blue going out, green for a style's tail out of the map, and red
where the ship's belly would meet the level */
void forge_3d_dropship_routes_render(void (*draw_piece)(float const *from, float const *to, int kind))
{
	enum
	{
		MAX_PIECES = 160,
		MAX_PIECES_PER_SEGMENT = 8
	};
	int pieces = 0;
	int z;

	for (z = 0; z < g_forge.live_instance_count && pieces < MAX_PIECES; z++)
	{
		struct forge_live_instance const *zone = &g_forge.live_instances[z];
		struct ff_route_leg legs[FF_ROUTE_LEG_COUNT];
		int leg;

		if (!forge_ff_is_dropship_zone(zone) || FF_ROUTE_ID(zone->team_designator) == 0)
			continue;

		forge_ff_route_legs(zone, legs, 1);
		for (leg = 0; leg < FF_ROUTE_LEG_COUNT; leg++)
		{
			int tail_segment = legs[leg].tail_length <= 0.0f ? -1 :
				(leg == FF_ROUTE_APPROACH ? 0 : legs[leg].count - 2);
			int segment;

			for (segment = 0; segment + 1 < legs[leg].count && pieces < MAX_PIECES; segment++)
			{
				float chord = ff_route_distance(&legs[leg].points[segment], &legs[leg].points[segment + 1]);
				int steps = (int)(chord / 4.0f) + 2;
				struct ff_route_point previous = legs[leg].points[segment];
				int step;

				if (steps > MAX_PIECES_PER_SEGMENT)
					steps = MAX_PIECES_PER_SEGMENT;
				for (step = 1; step <= steps && pieces < MAX_PIECES; step++)
				{
					struct ff_route_point point = ff_route_curve(&legs[leg], segment, (float)step / (float)steps);
					float from[3], to[3];
					int kind = (segment == tail_segment) ? _forge_route_piece_tail :
						(leg == FF_ROUTE_APPROACH ? _forge_route_piece_approach : _forge_route_piece_departure);

					if (segment != tail_segment)
					{
						real_point3d belly;
						real_vector3d along;
						struct collision_result coll;

						belly.x = previous.x;
						belly.y = previous.y;
						belly.z = previous.z - 1.5f;
						along.i = point.x - previous.x;
						along.j = point.y - previous.y;
						along.k = point.z - previous.z;
						if (collision_test_vector(
							FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
							&belly, &along, NONE, &coll))
						{
							kind = _forge_route_piece_blocked;
						}
					}

					from[0] = previous.x;
					from[1] = previous.y;
					from[2] = previous.z;
					to[0] = point.x;
					to[1] = point.y;
					to[2] = point.z;
					draw_piece(from, to, kind);
					previous = point;
					pieces++;
				}
			}
		}
	}
}

static void draw_teleporter_and_objective_markers(
	short window_x0,
	short window_y0,
	short window_x1,
	short window_y1,
	long font_index)
{
	int i;

	/* Player Spawns, Teleporter Origin/Destination, Race Markers, Oddball Markers,
	 * and CTF Flags now render as true 3D translucent world models in
	 * render_world_objective_and_spawn_markers() (forge_runtime_tags.c).
	 * In 2D HUD space we only draw floating text labels while in Forge Mode.
	 * In Spartan Play mode, hide Forge-only Spawn Point and Objective markers and labels completely! */
	if (g_forge.mode != _forge_mode_3d_monitor)
		return;

	/* Only show floating marker names when the pickup reticle is over the item (or holding it) */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		union real_point3d wpt;
		union real_point2d spt;
		int is_hi;
		short sx, sy;

		if (!inst->active)
			continue;
		if (inst->group_tag != 'spwn' && inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp' && inst->group_tag != FORGE_GROUP_VOLUME &&
			inst->group_tag != FORGE_GROUP_AI_SPAWN && inst->group_tag != FORGE_GROUP_AI_ROUTE && !forge_tele_is(inst))
			continue;

		is_hi = (i == g_forge.grabbed_live_index || i == g_forge.hovered_live_index ||
			(inst->object_index != NONE && inst->object_index == g_forge.hovered_object_index));
		if (!is_hi)
			continue;

		if (inst->group_tag == FORGE_GROUP_AI_SPAWN || inst->group_tag == FORGE_GROUP_AI_ROUTE)
		{
			char ai_lbl[96];

			if (inst->group_tag == FORGE_GROUP_AI_ROUTE)
				forge_ai_route_describe(i, ai_lbl, sizeof(ai_lbl));
			else
				forge_ai_spawn_label(inst, ai_lbl, sizeof(ai_lbl));
			wpt.x = inst->x;
			wpt.y = inst->y;
			wpt.z = inst->z + 0.14f;
			if (render_camera_world_to_screen(&render.camera, &render.frustum, &wpt, &spt))
			{
				sx = (short)(window_x0 + (short)(spt.x + 0.5f));
				sy = (short)(window_y0 + (short)(spt.y + 0.5f));
				if (sx >= window_x0 + 20 && sx <= window_x1 - 20 && sy >= window_y0 + 10 && sy <= window_y1 - 10)
				{
					set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)sx, (float)sy);
					draw_ce_menu_str_box(0, font_index, (short)(sx - 140), (short)(sy + 6), (short)(sx + 140), (short)(sy + 32), 2, ai_lbl, 0xFFFFC824u);
				}
			}
			continue;
		}
		if (inst->group_tag == 'ffsp')
		{
			int ff_type = FORGE_FF_UNPACK_TYPE(inst->team_designator);
			uint32_t ring_col = (ff_type == _forge_ff_spawn_player_vehicle) ? 0xFF24FF54u :
				(ff_drop_point_kinds(ff_type) != 0 || ff_type == FF_ROUTE_SPAWN_TYPE ? 0xFFFFC824u :
				(ff_type >= _forge_ff_spawn_ai_and_vehicles ? 0xFFFF8C14u : 0xFFFF6868u));
			char ff_lbl[64];

			snprintf(ff_lbl, sizeof(ff_lbl), "FF: %s", forge_ff_spawn_type_label(ff_type));
			wpt.x = inst->x;
			wpt.y = inst->y;
			wpt.z = inst->z + 0.14f;
			if (render_camera_world_to_screen(&render.camera, &render.frustum, &wpt, &spt))
			{
				sx = (short)(window_x0 + (short)(spt.x + 0.5f));
				sy = (short)(window_y0 + (short)(spt.y + 0.5f));
				if (sx >= window_x0 + 20 && sx <= window_x1 - 20 && sy >= window_y0 + 10 && sy <= window_y1 - 10)
				{
					set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)sx, (float)sy);
					draw_ce_menu_str_box(0, font_index, (short)(sx - 100), (short)(sy + 6), (short)(sx + 100), (short)(sy + 32), 2, ff_lbl, ring_col);
				}
			}
			continue;
		}
		if (inst->group_tag == 'spwn')
		{
			uint32_t ring_col;
			char const *team_lbl;

			if (inst->team_designator == 1)
			{
				ring_col = 0xFF2896FFu;
				team_lbl = "BLUE SPAWN";
			}
			else if (inst->team_designator == 0)
			{
				ring_col = 0xFFFF6868u;
				team_lbl = "RED SPAWN";
			}
			else
			{
				ring_col = 0xFFFFFFFFu;
				team_lbl = "FFA SPAWN";
			}

			wpt.x = inst->x;
			wpt.y = inst->y;
			wpt.z = inst->z + 0.14f;
			if (render_camera_world_to_screen(&render.camera, &render.frustum, &wpt, &spt))
			{
				sx = (short)(window_x0 + (short)(spt.x + 0.5f));
				sy = (short)(window_y0 + (short)(spt.y + 0.5f));
				if (sx >= window_x0 + 20 && sx <= window_x1 - 20 && sy >= window_y0 + 10 && sy <= window_y1 - 10)
				{
					set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)sx, (float)sy);
					draw_ce_menu_str_box(0, font_index, (short)(sx - 80), (short)(sy + 6), (short)(sx + 80), (short)(sy + 32), 2, team_lbl, ring_col);
				}
			}
			continue;
		}

		wpt.x = inst->x;
		wpt.y = inst->y;
		wpt.z = inst->z + 0.28f;
		if (!render_camera_world_to_screen(&render.camera, &render.frustum, &wpt, &spt))
			continue;

		sx = (short)(window_x0 + (short)(spt.x + 0.5f));
		sy = (short)(window_y0 + (short)(spt.y + 0.5f));
		if (sx < window_x0 + 10 || sx > window_x1 - 10 || sy < window_y0 + 10 || sy > window_y1 - 10)
			continue;

		set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)sx, (float)sy);
		if (forge_tele_is(inst))
		{
			int role = forge_tele_role(inst);
			int linked = forge_tele_other_ends(i, NULL, 0) > 0;
			char lbl[64];

			if (role == 0)
				snprintf(lbl, sizeof(lbl), "TELEPORTER OFF");
			else
				snprintf(lbl, sizeof(lbl), "%s #%d%s", forge_tele_role_name(role), forge_tele_channel(inst),
					linked ? "" : " (NOT LINKED)");
			/* (green linked, amber not) */
			draw_ce_menu_str_box(0, font_index, (short)(sx - 110), (short)(sy + 8), (short)(sx + 110), (short)(sy + 34), 2, lbl,
				role != 0 && linked ? 0xFF24FF54u : 0xFFFFC824u);
		}
		else if (inst->group_tag == 'flag')
		{
			char lbl[64];
			char const *kind = "OBJECTIVE";
			uint32_t border_col = 0xFFFFFFFFu;

			if (inst->netgame_flag_type == _netgame_flag_hill || strstr(inst->tag_path, "koth"))
			{
				kind = "KOTH HILL";
				border_col = 0xFFFFFFFFu;
			}
			else if (inst->netgame_flag_type == _netgame_flag_oddball_ball_spawn || strstr(inst->tag_path, "oddball"))
			{
				kind = "ODDBALL";
				border_col = 0xFFFFFFFFu;
			}
			else if (inst->netgame_flag_type == _netgame_flag_ctf_flag ||
					 inst->netgame_flag_type == _netgame_flag_ctf_vehicle ||
					 strstr(inst->tag_path, "ctf"))
			{
				kind = (inst->team_designator == 1) ? "BLUE FLAG" : "RED FLAG";
				border_col = (inst->team_designator == 1) ? 0xFF2896FFu : 0xFFFF6868u;
			}
			else if (inst->netgame_flag_type == _netgame_flag_race_track ||
					 inst->netgame_flag_type == _netgame_flag_race_vehicle ||
					 strstr(inst->tag_path, "race"))
			{
				kind = (inst->netgame_flag_type == _netgame_flag_race_vehicle) ? "RACE VEH" : "RACE MARKER";
				border_col = 0xFFFFFFFFu;
			}

			snprintf(lbl, sizeof(lbl), "%s #%d", kind, (int)inst->team_designator);
			if (inst->netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_GENERATOR)
				snprintf(lbl, sizeof(lbl), "GENERATOR");
			draw_ce_menu_str_box(0, font_index, (short)(sx - 90), (short)(sy + 8), (short)(sx + 90), (short)(sy + 34), 2, lbl, border_col);
		}
	}

	/* Draw 3D-projected Snap Point HUD indicators while holding an object with snap points */
	if (g_forge.grabbed_live_index >= 0 && g_forge.grabbed_live_index < g_forge.live_instance_count && !g_forge.koth_group_placing)
	{
		struct forge_live_instance const *held = &g_forge.live_instances[g_forge.grabbed_live_index];
		struct forge_tag_snap_config const *held_cfg =
			(held->active && held->tag_path[0]) ? forge_snap_points_get_for_tag(held->group_tag, held->tag_path, 0) : NULL;

		if (held_cfg && held_cfg->point_count > 0)
		{
			set_forge_hud_ui_scale(1.0f, 0.0f, 0.0f);
			for (i = 0; i < g_forge.live_instance_count; i++)
			{
				struct forge_live_instance const *inst = &g_forge.live_instances[i];
				struct forge_tag_snap_config const *cfg;
				real_vector3d fwd_v, left_v, up_v;
				float dx, dy, dz;
				int p;

				if (!inst->active || !inst->tag_path[0])
					continue;
				dx = inst->x - held->x;
				dy = inst->y - held->y;
				dz = inst->z - held->z;
				if (dx * dx + dy * dy + dz * dz > 28.0f * 28.0f)
					continue;

				cfg = forge_snap_points_get_for_tag(inst->group_tag, inst->tag_path, 0);
				if (!cfg || cfg->point_count <= 0)
					continue;

				euler_to_basis_vectors(inst->yaw, inst->pitch, inst->roll, &fwd_v, &left_v, &up_v);
				for (p = 0; p < cfg->point_count; p++)
				{
					struct forge_snap_point const *sp = &cfg->points[p];
					union real_point3d wpt;
					union real_point2d spt;
					short sx, sy;
					rectangle2d sq;
					uint32_t fill_col = (i == g_forge.grabbed_live_index) ? 0xD0FFD040u : 0xC828D8FFu;

					wpt.x = inst->x + sp->x * fwd_v.i + sp->y * left_v.i + sp->z * up_v.i;
					wpt.y = inst->y + sp->x * fwd_v.j + sp->y * left_v.j + sp->z * up_v.j;
					wpt.z = inst->z + sp->x * fwd_v.k + sp->y * left_v.k + sp->z * up_v.k;
					if (!render_camera_world_to_screen(&render.camera, &render.frustum, &wpt, &spt))
						continue;

					sx = (short)(window_x0 + (short)(spt.x + 0.5f));
					sy = (short)(window_y0 + (short)(spt.y + 0.5f));
					if (sx < window_x0 + 8 || sx > window_x1 - 8 || sy < window_y0 + 8 || sy > window_y1 - 8)
						continue;

					sq.x0 = (short)(sx - 3);
					sq.y0 = (short)(sy - 3);
					sq.x1 = (short)(sx + 3);
					sq.y1 = (short)(sy + 3);
					draw_quad(&sq, fill_col);
					draw_box_border(sq.x0, sq.y0, sq.x1, sq.y1, 1, 0xFFFFFFFFu);
				}
			}

			if (g_forge.snap_point_locked)
			{
				union real_point3d lpt;
				union real_point2d spt;
				lpt.x = g_forge.snap_lock_world_x;
				lpt.y = g_forge.snap_lock_world_y;
				lpt.z = g_forge.snap_lock_world_z;
				if (render_camera_world_to_screen(&render.camera, &render.frustum, &lpt, &spt))
				{
					short sx = (short)(window_x0 + (short)(spt.x + 0.5f));
					short sy = (short)(window_y0 + (short)(spt.y + 0.5f));
					if (sx >= window_x0 + 16 && sx <= window_x1 - 16 && sy >= window_y0 + 16 && sy <= window_y1 - 16)
					{
						rectangle2d lsq = { (short)(sy - 6), (short)(sx - 6), (short)(sy + 6), (short)(sx + 6) };
						draw_quad(&lsq, 0xE030FF60u);
						draw_box_border(lsq.x0, lsq.y0, lsq.x1, lsq.y1, 2, 0xFFFFFFFFu);
						set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)sx, (float)sy);
						draw_ce_menu_str_box(
							0,
							font_index,
							(short)(sx - 150),
							(short)(sy - 26),
							(short)(sx + 150),
							(short)(sy - 4),
							2,
							g_forge.snap_lock_label[0] ? g_forge.snap_lock_label : "SNAP LOCKED",
							0xFF40FF78u);
					}
				}
			}
		}
	}

	set_forge_hud_ui_scale(1.0f, 0.0f, 0.0f);
}

void forge_3d_draw_hud(
	short local_player_index,
	short window_x0,
	short window_y0,
	short window_x1,
	short window_y1,
	long font_index,
	short line_height)
{
	static char const *const cat_names[NUMBER_OF_FORGE_PALETTE_CATEGORIES] = {
		"WEAPONS",
		"EQUIPMENT",
		"VEHICLES",
		"BIPEDS",
		"SCENERY",
		"DOORS",
		"LIGHTS",
		"SPAWNS",
		"OBJECTIVES",
		"ACTORS",
		"FIREFIGHT",
		"TELEPORTERS",
		"EFFECTS",
		"DECALS",
		"VOLUMES"
	};
	static char const *const snap_names[4] = { "OFF", "15 DEG", "45 DEG", "90 DEG" };
	char buf[256];
	short w = (short)(window_x1 - window_x0);
	short h = (short)(window_y1 - window_y0);

	(void)line_height;

	/* Never draw any Forge overlay on the main menu (levels\ui\ui) or when not in a Forge session */
	if (!g_forge_session_enabled || local_player_index != 0 || w <= 0 || h <= 0)
		return;
	if (forge_is_main_menu_active())
		return;
	if (ui_widgets_active())
		return;

	/* Ensure customizable UI theme & PNG backgrounds from Faber_data/ui/ are loaded */
	forge_ui_theme_ensure_loaded();

	s_hit_count = 0;

	/* Draw 3D-projected Player Spawns & Teleporter / Objective floating labels */
	draw_teleporter_and_objective_markers(window_x0, window_y0, window_x1, window_y1, font_index);

	{
		short bar_top_y = (short)(window_y1 - 26);
		char const *p_toggle = forge_action_prompt(FORGE_BIND_TOGGLE_MODE);
		char const *p_pal = forge_action_prompt(FORGE_BIND_PALETTE);
		char const *p_qsave = forge_action_prompt(FORGE_BIND_QUICK_SAVE);
		char const *p_rot = forge_action_prompt(FORGE_BIND_ROTATE_HOLD);
		char const *p_floor = forge_action_prompt(FORGE_BIND_SNAP_FLOOR);
		char const *p_surf = forge_action_prompt(FORGE_BIND_SNAP_SURFACE);
		char const *p_snap = forge_action_prompt(FORGE_BIND_ROT_SNAP);
		char const *p_opts = forge_action_prompt(FORGE_BIND_OBJECT_OPTIONS);
		char const *p_del = forge_action_prompt(FORGE_BIND_DELETE_OBJECT);
		char const *p_copy = forge_action_prompt(FORGE_BIND_COPY_OBJECT);
		char const *p_click = g_forge.using_controller ? "{A}" : "[CLICK]";
		char const *p_cancel = g_forge.using_controller ? "HOLD {B}" : "[ESC]";
		char const *p_dist = g_forge.using_controller ? "{LS}" : "[SCROLL]";
		char const *p_look_settings = forge_action_prompt(FORGE_BIND_LOOK_SETTINGS);

		/* Spartan Play Mode: Bottom bar dynamically sized to fit text pixel width */
		if (g_forge.mode == _forge_mode_play && g_forge.active_modal == _forge_modal_none)
		{
			if (g_forge.has_saved_map_once)
				snprintf(buf, sizeof(buf), "%s FORGE   %s QUICK SAVE", p_toggle, p_qsave);
			else
				snprintf(buf, sizeof(buf), "%s FORGE", p_toggle);
			draw_sized_bottom_pill_bar(
				font_index, window_x0, window_x1, (short)(window_y1 - 2),
				buf, NULL, 0, _forge_ui_area_bottom_bar, 0xFF2896FFu, &bar_top_y);
		}
		else if (g_forge.mode == _forge_mode_3d_monitor)
		{
			/* Clean Unarmed Monitor Reticle at screen center */
			short cx = (short)((window_x0 + window_x1) / 2);
			short cy = (short)((window_y0 + window_y1) / 2);
			uint32_t ret_col = (g_forge.grabbed_live_index >= 0 || g_forge.hovered_live_index >= 0 || g_forge.hovered_object_index != NONE || g_forge.hovered_object_label[0])
				? 0xFFFFFFFFu : 0xFF2896FFu;
			rectangle2d rt;
			short px0, py0, px1, py1;

			set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)cx, (float)cy);

			/* Center dot */
			rt.x0 = (short)(cx - 1); rt.y0 = (short)(cy - 1); rt.x1 = (short)(cx + 2); rt.y1 = (short)(cy + 2);
			draw_quad(&rt, ret_col);
			/* Four unarmed bracket ticks around center */
			rt.x0 = (short)(cx - 11); rt.y0 = (short)(cy - 1); rt.x1 = (short)(cx - 5); rt.y1 = (short)(cy + 2);
			draw_quad(&rt, ret_col);
			rt.x0 = (short)(cx + 6); rt.y0 = (short)(cy - 1); rt.x1 = (short)(cx + 12); rt.y1 = (short)(cy + 2);
			draw_quad(&rt, ret_col);
			rt.x0 = (short)(cx - 1); rt.y0 = (short)(cy - 11); rt.x1 = (short)(cx + 2); rt.y1 = (short)(cy - 5);
			draw_quad(&rt, ret_col);
			rt.x0 = (short)(cx - 1); rt.y0 = (short)(cy + 6); rt.x1 = (short)(cx + 2); rt.y1 = (short)(cy + 12);
			draw_quad(&rt, ret_col);

			/* The delete button's hold, filling under the reticle */
			if (s_delete_hold_fraction > 0.0f)
			{
				rt.x0 = (short)(cx - 41); rt.y0 = (short)(cy + 20); rt.x1 = (short)(cx + 41); rt.y1 = (short)(cy + 26);
				draw_quad(&rt, 0xC0061224u);
				rt.x0 = (short)(cx - 40); rt.y0 = (short)(cy + 21);
				rt.x1 = (short)(cx - 40 + (short)(80.0f * s_delete_hold_fraction)); rt.y1 = (short)(cy + 25);
				draw_quad(&rt, 0xFFFF4040u);
			}
			/* and the palette button's, toward MAP SETTINGS (in the reticle's blue) */
			else if (s_map_settings_hold_fraction > 0.0f)
			{
				rt.x0 = (short)(cx - 41); rt.y0 = (short)(cy + 20); rt.x1 = (short)(cx + 41); rt.y1 = (short)(cy + 26);
				draw_quad(&rt, 0xC0061224u);
				rt.x0 = (short)(cx - 40); rt.y0 = (short)(cy + 21);
				rt.x1 = (short)(cx - 40 + (short)(80.0f * s_map_settings_hold_fraction)); rt.y1 = (short)(cy + 25);
				draw_quad(&rt, 0xFF2896FFu);
			}

			/* Show hovered object/marker label right below reticle in base-game UI font
			(not over the MAP SETTINGS hold's bar) */
			if (g_forge.grabbed_live_index < 0 && g_forge.hovered_object_label[0] &&
				s_map_settings_hold_fraction <= 0.0f)
			{
				char upper_lbl[96];
				format_clean_uppercase_item_name(g_forge.hovered_object_label, upper_lbl, sizeof(upper_lbl));
				snprintf(buf, sizeof(buf), "%s %s   %s SETTINGS   %s COPY", p_click, upper_lbl, p_look_settings, p_copy);
				draw_ce_menu_str_box(0, font_index, (short)(cx - 180), (short)(cy + 16), (short)(cx + 180), (short)(cy + 42), 2, buf, 0xFFFFFFFFu);
			}

			/* Top-Right Forge Mode Status & Budget Box (positioned right below the health/shield HUD,
			 * scaled down an additional 50% and styled with 9-slice rounded Halo CE box & UI fonts) */
			px0 = (short)(window_x1 - 236);
			py0 = (short)(window_y0 + 52);
			px1 = (short)(window_x1 - 12);
			py1 = (short)(py0 + 54);
			set_forge_hud_ui_scale(FORGE_COUNTER_HUD_SCALE, (float)px1, (float)py0);
			draw_ce_pause_box(px0, py0, (short)(px1 - px0), (short)(py1 - py0), 0);

			draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 6), "FORGE", 0xFFFFFFFFu);
			snprintf(buf, sizeof(buf), "%d / %d",
				count_active_live_instances(), FORGE_MAX_LIVE_INSTANCES);
			draw_ce_menu_str_box(1, font_index, (short)(px0 + 84), (short)(py0 + 6), (short)(px1 - 14), (short)(py0 + 30), 1 /* right */,
				buf, 0xFFFFFFFFu);

			snprintf(buf, sizeof(buf), "%s SNAP: %s",
				p_snap, snap_names[g_forge.snap_mode]);
			draw_ce_menu_str_box(0, font_index, (short)(px0 + 14), (short)(py0 + 28), (short)(px1 - 14), (short)(py1 - 6), 0,
				buf, 0xFF2896FFu);

			/* Bottom Control Strip dynamically sized to fit exact pixel width (and 2 lines when holding an item) */
			if (g_forge.active_modal == _forge_modal_none)
			{
				if (g_forge.grabbed_live_index >= 0)
				{
					char line1[160];
					char line2[160];
					snprintf(line1, sizeof(line1),
						"%s PLACE  %s %.0fM  %s PITCH/YAW  %s SURFACE",
						p_click, p_dist, g_forge.grab_distance, p_rot, p_surf);
					snprintf(line2, sizeof(line2),
						"%s FLOOR  %s OPTIONS  HOLD %s DELETE  %s CANCEL",
						p_floor, p_opts, p_del, p_cancel);
					draw_sized_bottom_pill_bar(
						font_index, window_x0, window_x1, (short)(window_y1 - 2),
						line1, line2, 0, _forge_ui_area_bottom_bar, 0xFF2896FFu, &bar_top_y);
				}
				else
				{
					snprintf(buf, sizeof(buf), "%s OBJECTS (HOLD: MAP)  %s GRAB  %s COPY  %s PLAY", p_pal, p_click, p_copy,
						p_toggle);
					draw_sized_bottom_pill_bar(
						font_index, window_x0, window_x1, (short)(window_y1 - 2),
						buf, NULL, 0, _forge_ui_area_bottom_bar, 0xFF2896FFu, &bar_top_y);
				}
			}
		}

		/* ========================================================================
		 * UNIFIED TABBED FORGE OBJECT PALETTE MENU (F2) - BASE GAME MENU STYLING
		 * ======================================================================== */
		if (g_forge.active_modal == _forge_modal_spawn_palette)
		{
			int filtered[512];
			int fcount = get_filtered_catalog_indices(filtered, 512);
			int r, max_rows = 10;
			short box_w = 296;
			short box_h = 336;
			short px0 = (short)(window_x0 + 16);
			short py0 = (short)(window_y0 + 34);
			short px1 = (short)(px0 + box_w);
			short py1 = (short)(py0 + box_h);
			short spin_x0 = (short)(px0 + 14);
			short spin_y0 = (short)(py0 + 32);
			short spin_x1 = (short)(px1 - 14);
			short spin_y1 = (short)(spin_y0 + 26);
			short y_cur;
			char clean_name[64];

			set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)px0, (float)py0);

			/* Authentic Halo CE 9-slice Objectives/Pause Box container */
			draw_ce_pause_box(px0, py0, box_w, box_h, 1);

			/* Top Header Bar Text in ui\large_ui */
			draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 4),
				"FORGE PALETTE", 0xFFFFFFFFu);
			snprintf(buf, sizeof(buf), "%d / %d", count_active_live_instances(), FORGE_MAX_LIVE_INSTANCES);
			draw_ce_menu_str_box(0, font_index, (short)(px1 - 110), (short)(py0 + 6), (short)(px1 - 14), (short)(py0 + 28), 1 /* right */,
				buf, 0xFF2896FFu);

			/* Category Spinner Row (3-slice rounded option_bkds_sm + arrow_sm_left + arrow_sm_right + ui\large_ui) */
			draw_ce_rounded_row_bar(1, spin_x0, spin_y0, spin_x1, spin_y1);
			draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_left", 0, (short)(spin_x0 + 6), (short)(spin_y0 + 7), (short)(spin_x0 + 12), (short)(spin_y0 + 19), 6, 12, 0xFFFFFFFFu);
			draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_right", 0, (short)(spin_x1 - 12), (short)(spin_y0 + 7), (short)(spin_x1 - 6), (short)(spin_y0 + 19), 6, 12, 0xFFFFFFFFu);

			if (forge_catalog_category_split(g_forge.current_category) && s_palette_origin != NONE)
			{
				/* (where the list is: the tab's origin, and its keyword's folder) */
				snprintf(buf, sizeof(buf), "%s / %s (%d/%d)",
					s_palette_keyword != 0 ? forge_catalog_origin_name(s_palette_origin) : cat_names[g_forge.current_category],
					s_palette_keyword != 0 ? forge_catalog_keyword_name(s_palette_keyword) :
						forge_catalog_origin_name(s_palette_origin),
					fcount > 0 ? (g_forge.palette_cursor + 1) : 0,
					fcount);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s (%d/%d)",
					(g_forge.current_category == _forge_cat_ai_squads && s_palette_ai_group != NONE) ?
						forge_ai_group_name(s_palette_ai_group) :
					(g_forge.current_category == _forge_cat_firefight && s_palette_ff_menu) ? "FIREFIGHT SPAWNS" :
						cat_names[g_forge.current_category],
					fcount > 0 ? (g_forge.palette_cursor + 1) : 0,
					fcount);
			}
			forge_text_fit(1, font_index, buf, (short)(spin_x1 - 16 - (spin_x0 + 16)));
			forge_hit_add(_forge_hit_palette_previous, 0, spin_x0, spin_y0, (short)(spin_x0 + 40), spin_y1);
			forge_hit_add(_forge_hit_palette_next, 0, (short)(spin_x1 - 40), spin_y0, spin_x1, spin_y1);
			draw_ce_menu_str_box(1, font_index, (short)(spin_x0 + 16), (short)(spin_y0 + 2), (short)(spin_x1 - 16), spin_y1, 2 /* center */,
				buf, 0xFFFFFFFFu);

			/* Horizontal Blue Divider Line (bitmaps/blue) */
			draw_ce_blue_line((short)(px0 + 14), (short)(spin_y1 + 3), (short)(px1 - 14), 2);

			/* 10 Item Rows with rounded highlight on BOTH left and right ends when selected */
			y_cur = (short)(spin_y1 + 8);
			for (r = 0; r < max_rows && (g_forge.palette_scroll + r) < fcount; r++)
			{
				int idx = g_forge.palette_scroll + r;
				struct forge_catalog_entry const *ce = &g_forge.catalog[filtered[idx]];
				int sel = (idx == g_forge.palette_cursor);
				int placed_cnt = count_active_instances_for_catalog_entry(ce);
				uint32_t row_col = sel ? 0xFFFFFFFFu : 0xFF2896FFu;
				short rx0 = (short)(px0 + 14);
				short ry0 = y_cur;
				short rx1 = (short)(px1 - 14);
				short ry1 = (short)(ry0 + 22);

				if (sel)
					draw_ce_rounded_row_bar(1, rx0, ry0, rx1, ry1);
				forge_hit_add(_forge_hit_palette_row, idx, rx0, ry0, rx1, ry1);

				format_clean_uppercase_item_name(ce->display_name, clean_name, sizeof(clean_name));
				forge_text_fit(0, font_index, clean_name, (short)(rx1 - 60 - (rx0 + 10)));
				draw_ce_menu_str_box(0, font_index, (short)(rx0 + 10), (short)(ry0 + 1), (short)(rx1 - 60), ry1, 0,
					clean_name, row_col);

				/* (a group opens; its count means nothing) */
				snprintf(buf, sizeof(buf),
					ce->group_tag == FORGE_GROUP_AI_MENU || ce->group_tag == FORGE_GROUP_FOLDER ||
					ce->group_tag == FORGE_GROUP_FF_MENU ? ">" : "%d", placed_cnt);
				draw_ce_menu_str_box(0, font_index, (short)(rx1 - 56), (short)(ry0 + 1), (short)(rx1 - 10), ry1, 1 /* right */,
					buf, row_col);

				y_cur = (short)(y_cur + 23);
			}

			/* Scroll indicator arrows using authentic arrow_sm_up / arrow_sm_down bitmaps */
			if (g_forge.palette_scroll > 0)
			{
				draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_up", 1,
					(short)(px1 - 24), (short)(spin_y1 + 9), (short)(px1 - 12), (short)(spin_y1 + 21), 16, 16, 0xFFFFFFFFu);
			}
			if (g_forge.palette_scroll + max_rows < fcount)
			{
				draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_down", 1,
					(short)(px1 - 24), (short)(py1 - 50), (short)(px1 - 12), (short)(py1 - 38), 16, 16, 0xFFFFFFFFu);
			}

			/* Footer Action Buttons using authentic bitmaps/text_button_background */
			{
				{
					forge_hit_add(_forge_hit_palette_place, 0, (short)(px0 + 14), (short)(py1 - 32), (short)(px0 + 142), (short)(py1 - 8));
					forge_hit_add(_forge_hit_palette_done, 0, (short)(px1 - 142), (short)(py1 - 32), (short)(px1 - 14), (short)(py1 - 8));
					draw_ce_text_button(font_index, (short)(px0 + 14), (short)(py1 - 32), 128, 24, 1,
						g_forge.using_controller ? "{A} PLACE" : "[ENTER] PLACE");
					draw_ce_text_button(font_index, (short)(px1 - 142), (short)(py1 - 32), 128, 24, 0,
						g_forge.using_controller ? "{B} DONE" : "[ESC] DONE");
				}
			}
		}

		/* ========================================================================
		 * OBJECT PROPERTIES PANEL (X when holding an object) - BASE GAME MENU STYLING
		 * ======================================================================== */
		if (g_forge.active_modal == _forge_modal_object_properties && g_forge.grabbed_live_index >= 0)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[g_forge.grabbed_live_index];
			int is_ff_spawn = (inst->group_tag == 'ffsp');
			int is_route_point = forge_ff_is_route_point(inst);
			int is_zone = forge_ff_is_dropship_zone(inst);
			int snap_row = forge_props_snap_row(inst);
			int is_ai_spawn = (inst->group_tag == FORGE_GROUP_AI_SPAWN);
			int is_tele = forge_tele_is(inst);
			int test_drop_row = forge_is_drop_area(inst) ? (snap_row ? 6 : 5) : -1;
			int always_row = forge_props_always_spawn_row(inst, snap_row, test_drop_row);
			int scale_row = forge_props_scale_row(inst, snap_row, test_drop_row);
			int damage_row = forge_props_damage_row(inst, scale_row);
			int collision_row = forge_props_collision_row(inst, scale_row, damage_row);
			int is_volume = forge_volume_is(g_forge.grabbed_live_index);
			int is_ai_route = forge_ai_route_is(inst);
			int ai_rows[NUMBER_OF_FORGE_AI_ROWS];
			int ai_row_count = is_ai_spawn ? forge_ai_spawn_rows(g_forge.grabbed_live_index, ai_rows) : 0;
			int route_rows[NUMBER_OF_FORGE_AI_ROUTE_ROWS];
			int route_row_count = is_ai_route ? forge_ai_route_rows(g_forge.grabbed_live_index, route_rows) : 0;
			int zone_rows[FORGE_FF_ZONE_MAX_ROWS];
			int zone_row_count = is_zone ? forge_ff_zone_rows(inst, zone_rows) : 0;
			int volume_rows[NUMBER_OF_FORGE_VOLUME_ROWS];
			int volume_row_count = is_volume ? forge_volume_rows(g_forge.grabbed_live_index, volume_rows) : 0;
			/* (a drop point's FACTIONS TO SPAWN page: its three, then BACK alone) */
			int factions_page = is_zone && s_props_factions_open;
			int row_count = factions_page ? 3 : is_volume ? volume_row_count :
				is_ai_route ? route_row_count :
				is_ai_spawn ? ai_row_count : is_tele ? (snap_row ? 7 : 6) :
				(is_zone ? FORGE_FF_ZONE_FIRST_ROW + zone_row_count :
					(is_route_point ? 7 : (always_row >= 0 ? always_row + 1 : collision_row >= 0 ? collision_row + 1 :
						damage_row >= 0 ? damage_row + 1 : scale_row >= 0 ? scale_row + 1 :
						(test_drop_row >= 0 ? test_drop_row + 1 : ((is_ff_spawn || snap_row) ? 6 : 5)))));
			short box_w = (short)(is_ai_spawn || is_ai_route ? 372 : 312);
			short box_h = (short)(34 + row_count * 28 + 50);
			short px0 = (short)(window_x0 + 16);
			short py0 = (short)(window_y0 + 34);
			short px1 = (short)(px0 + box_w);
			short py1 = (short)(py0 + box_h);
			char labels[NUMBER_OF_FORGE_AI_ROWS][40];
			char values[NUMBER_OF_FORGE_AI_ROWS][48];
			int r;

			set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)px0, (float)py0);

			draw_ce_pause_box(px0, py0, box_w, box_h, 1);

			draw_ce_menu_str(1, font_index, (short)(px0 + 14), (short)(py0 + 4),
				factions_page ? "FACTIONS TO SPAWN" : is_volume ? forge_volume_panel_title(g_forge.grabbed_live_index) :
					is_ai_route ? forge_ai_route_panel_title(g_forge.grabbed_live_index) :
					is_ai_spawn ? forge_ai_spawn_panel_title(g_forge.grabbed_live_index) :
					(is_tele ? "TELEPORTER" :
					(is_route_point ? "DROPSHIP ROUTE" : (is_zone ? "ENEMY DROP POINT" :
					(is_ff_spawn ? forge_ff_spawn_kind_title(FORGE_FF_UNPACK_TYPE(inst->team_designator)) :
						"OBJECT PROPERTIES")))), 0xFFFFFFFFu);

			if (is_volume)
			{
				for (r = 0; r < volume_row_count; r++)
				{
					int row = volume_rows[r];

					labels[r][0] = 0;
					values[r][0] = 0;
					if (row == _forge_volume_row_rot_snap)
					{
						snprintf(labels[r], sizeof(labels[0]), "ROT SNAP:");
						snprintf(values[r], sizeof(values[0]), "%s", snap_names[g_forge.snap_mode]);
					}
					else if (row == _forge_volume_row_floor)
						snprintf(labels[r], sizeof(labels[0]), "DROP TO FLOOR");
					else if (row == _forge_volume_row_delete)
						snprintf(labels[r], sizeof(labels[0]), "%s",
							forge_volume_get(g_forge.grabbed_live_index)->map_index >= 0 ? "TURN OFF" : "DELETE OBJECT");
					else
						forge_volume_row_text(g_forge.grabbed_live_index, row, labels[r], sizeof(labels[r]), values[r],
							sizeof(values[r]));
				}
			}
			else if (is_ai_route)
			{
				for (r = 0; r < route_row_count; r++)
				{
					int row = route_rows[r];

					labels[r][0] = 0;
					values[r][0] = 0;
					if (row == _forge_ai_route_row_rot_snap)
					{
						snprintf(labels[r], sizeof(labels[0]), "ROT SNAP:");
						snprintf(values[r], sizeof(values[0]), "%s", snap_names[g_forge.snap_mode]);
					}
					else if (row == _forge_ai_route_row_floor)
						snprintf(labels[r], sizeof(labels[0]), "DROP TO FLOOR");
					else if (row == _forge_ai_route_row_delete)
						snprintf(labels[r], sizeof(labels[0]), "DELETE POINT");
					else
						forge_ai_route_row_text(g_forge.grabbed_live_index, row, labels[r], sizeof(labels[r]), values[r],
							sizeof(values[r]));
				}
			}
			else if (is_ai_spawn)
			{
				for (r = 0; r < ai_row_count; r++)
				{
					int row = ai_rows[r];

					labels[r][0] = 0;
					values[r][0] = 0;
					if (row == _forge_ai_row_rot_snap)
					{
						snprintf(labels[r], sizeof(labels[0]), "ROT SNAP:");
						snprintf(values[r], sizeof(values[0]), "%s", snap_names[g_forge.snap_mode]);
					}
					else if (row == _forge_ai_row_floor)
						snprintf(labels[r], sizeof(labels[0]), "DROP TO FLOOR");
					else if (row == _forge_ai_row_delete)
						snprintf(labels[r], sizeof(labels[0]), "DELETE OBJECT");
					else
						forge_ai_spawn_row_text(g_forge.grabbed_live_index, row, labels[r], sizeof(labels[r]), values[r],
							sizeof(values[r]));
				}
			}
			else if (is_tele)
			{
				int role = forge_tele_role(inst);
				int ends = forge_tele_other_ends(g_forge.grabbed_live_index, NULL, 0);

				snprintf(labels[0], sizeof(labels[0]), "TYPE:");
				snprintf(values[0], sizeof(values[0]), "%s", forge_tele_role_name(role));
				snprintf(labels[1], sizeof(labels[1]), "CHANNEL:");
				if (role == 0)
					snprintf(values[1], sizeof(values[1]), "#%d", forge_tele_channel(inst));
				else if (ends == 0)
					snprintf(values[1], sizeof(values[1]), "#%d - NOT LINKED", forge_tele_channel(inst));
				else
					snprintf(values[1], sizeof(values[1]), "#%d - %d %s%s", forge_tele_channel(inst), ends,
						role == 1 ? "RECEIVER" : "SENDER", ends == 1 ? "" : "S");
				if (role == 0 || ends == 0)
					snprintf(labels[2], sizeof(labels[2]), "TELEPORT TO OTHER END");
				else
					snprintf(labels[2], sizeof(labels[2]), "TELEPORT TO %s", role == 1 ? "RECEIVER" : "SENDER");
				values[2][0] = 0;
				snprintf(labels[3], sizeof(labels[3]), "ROT SNAP:");
				snprintf(values[3], sizeof(values[3]), "%s", snap_names[g_forge.snap_mode]);
				snprintf(labels[4], sizeof(labels[4]), "DROP TO FLOOR");
				values[4][0] = 0;
				snprintf(labels[5], sizeof(labels[5]), "DELETE OBJECT");
				values[5][0] = 0;
				if (snap_row)
				{
					snprintf(labels[6], sizeof(labels[6]), "SNAP POINTS:");
					forge_snap_tag_layout_text(inst->group_tag, inst->tag_path, values[6], sizeof(values[6]));
				}
			}
			else if (is_route_point)
			{
				int leg = FF_ROUTE_POINT_LEG(inst->team_designator);
				int32_t points[FF_ROUTE_MAX_LEG_POINTS];
				int count = forge_ff_route_leg_points(FF_ROUTE_ID(inst->team_designator), leg, points);
				int rank = 0;

				while (rank < count && points[rank] != g_forge.grabbed_live_index)
					rank++;
				snprintf(labels[0], sizeof(labels[0]), "POINT:");
				snprintf(values[0], sizeof(values[0]), "%s %d OF %d",
					leg == FF_ROUTE_APPROACH ? "APPROACH" : "DEPARTURE", rank + 1, count);
				snprintf(labels[1], sizeof(labels[1]), "ADD POINT BEFORE");
				values[1][0] = 0;
				snprintf(labels[2], sizeof(labels[2]), "ADD POINT AFTER");
				values[2][0] = 0;
				snprintf(labels[3], sizeof(labels[3]), "ROT SNAP:");
				snprintf(values[3], sizeof(values[3]), "%s", snap_names[g_forge.snap_mode]);
				snprintf(labels[4], sizeof(labels[4]), "DROP TO FLOOR");
				values[4][0] = 0;
				snprintf(labels[5], sizeof(labels[5]), "DELETE OBJECT");
				values[5][0] = 0;
				snprintf(labels[6], sizeof(labels[6]), "%s", s_route_test_flying == 1 ? "STOP TEST DROP" : "TEST DROP");
				values[6][0] = 0;
			}
			else if (is_ff_spawn)
			{
				{
					int ff_type = FORGE_FF_UNPACK_TYPE(inst->team_designator);

					/* (vehicle AI's LIGHT or HEAVY; the rest's kind, which
					FIREFIGHT SPAWNS chose) */
					if (forge_ff_spawn_kind(ff_type) == _forge_ff_kind_vehicle)
					{
						snprintf(labels[0], sizeof(labels[0]), "VEHICLE AI:");
						snprintf(values[0], sizeof(values[0]), "%s", ff_type == _forge_ff_spawn_enemy_ghost_only ? "LIGHT" :
							ff_type == _forge_ff_spawn_enemy_heavy_veh ? "HEAVY" : "ANY");
					}
					else
					{
						snprintf(labels[0], sizeof(labels[0]), "SPAWN TYPE:");
						snprintf(values[0], sizeof(values[0]), "%s", forge_ff_spawn_type_label(ff_type));
					}
				}
				snprintf(labels[1], sizeof(labels[1]), "ROUNDS:");
				snprintf(values[1], sizeof(values[1]), "%s",
					forge_ff_round_filter_label(FORGE_FF_UNPACK_ROUND(inst->team_designator)));
				snprintf(labels[2], sizeof(labels[2]), "WAVES:");
				snprintf(values[2], sizeof(values[2]), "%s",
					forge_ff_wave_filter_label(FORGE_FF_UNPACK_WAVE(inst->team_designator)));
				snprintf(labels[3], sizeof(labels[3]), "ROT SNAP:");
				snprintf(values[3], sizeof(values[3]), "%s", snap_names[g_forge.snap_mode]);
				snprintf(labels[4], sizeof(labels[4]), "DROP TO FLOOR");
				values[4][0] = 0;
				snprintf(labels[5], sizeof(labels[5]), "DELETE OBJECT");
				values[5][0] = 0;
				if (is_zone)
				{
					int route_id = FF_ROUTE_ID(inst->team_designator);
					int k;

					for (k = 0; k < zone_row_count; k++)
					{
						int row = FORGE_FF_ZONE_FIRST_ROW + k;

						values[row][0] = 0;
						switch (zone_rows[k])
						{
						case _forge_ff_zone_row_route:
							snprintf(labels[row], sizeof(labels[0]), "FLIGHT ROUTE:");
							if (route_id != 0)
								snprintf(values[row], sizeof(values[0]), "%d POINTS", forge_ff_route_point_count(route_id));
							else
								snprintf(values[row], sizeof(values[0]), "AUTOMATIC");
							break;
						case _forge_ff_zone_row_arrival:
							snprintf(labels[row], sizeof(labels[0]), "ARRIVAL:");
							snprintf(values[row], sizeof(values[0]), "%s",
								ff_route_style_name(forge_ff_route_leg_style(route_id, FF_ROUTE_APPROACH), FF_ROUTE_APPROACH));
							break;
						case _forge_ff_zone_row_departure:
							snprintf(labels[row], sizeof(labels[0]), "DEPARTURE:");
							snprintf(values[row], sizeof(values[0]), "%s",
								ff_route_style_name(forge_ff_route_leg_style(route_id, FF_ROUTE_DEPARTURE), FF_ROUTE_DEPARTURE));
							break;
						case _forge_ff_zone_row_factions:
							snprintf(labels[row], sizeof(labels[0]), "FACTIONS:");
							forge_ff_factions_summary(forge_ff_drop_kinds(inst), values[row], sizeof(values[0]));
							break;
						case _forge_ff_zone_row_test_drop:
							snprintf(labels[row], sizeof(labels[0]), "%s", s_route_test_flying == 1 ? "STOP TEST DROP" : "TEST DROP");
							break;
						case _forge_ff_zone_row_test_flood:
							snprintf(labels[row], sizeof(labels[0]), "%s", s_route_test_flying == 2 ? "STOP TEST FLOOD" : "TEST FLOOD");
							break;
						default:
							labels[row][0] = 0;
							break;
						}
					}
				}
			}
			else
			{
				if (inst->group_tag == 'spwn')
				{
					snprintf(labels[0], sizeof(labels[0]), "TEAM:");
					snprintf(values[0], sizeof(values[0]), "%s",
						(inst->team_designator == 1) ? "BLUE TEAM" : (inst->team_designator == 2 ? "FFA" : "RED TEAM"));
				}
				else if (inst->group_tag == 'flag' &&
					(inst->netgame_flag_type == _netgame_flag_ctf_flag || inst->netgame_flag_type == _netgame_flag_ctf_vehicle))
				{
					snprintf(labels[0], sizeof(labels[0]), "TEAM:");
					snprintf(values[0], sizeof(values[0]), "%s",
						inst->team_designator == 1 ? "BLUE TEAM" : "RED TEAM");
				}
				else if (inst->group_tag == 'flag' &&
					(inst->netgame_flag_type == _netgame_flag_race_track || inst->netgame_flag_type == _netgame_flag_race_vehicle))
				{
					snprintf(labels[0], sizeof(labels[0]), "ORDER:");
					snprintf(values[0], sizeof(values[0]), "#%d", (int)inst->team_designator);
				}
				else if (inst->group_tag == 'effe')
				{
					snprintf(labels[0], sizeof(labels[0]), "REPEAT:");
					snprintf(values[0], sizeof(values[0]), "EVERY %d S", forge_effect_spot_seconds(inst));
				}
				else
				{
					snprintf(labels[0], sizeof(labels[0]), "CHANNEL:");
					snprintf(values[0], sizeof(values[0]), "#%d", (int)inst->team_designator);
				}
				snprintf(labels[1], sizeof(labels[1]), "GAME MODE:");
				snprintf(values[1], sizeof(values[1]), "%s", gametype_flags_label(inst->gametype_flags));
				snprintf(labels[2], sizeof(labels[2]), "ROT SNAP:");
				snprintf(values[2], sizeof(values[2]), "%s", snap_names[g_forge.snap_mode]);
				snprintf(labels[3], sizeof(labels[3]), "DROP TO FLOOR");
				values[3][0] = 0;
				snprintf(labels[4], sizeof(labels[4]), "DELETE OBJECT");
				values[4][0] = 0;
				snprintf(labels[5], sizeof(labels[5]), "SNAP POINTS:");
				values[5][0] = 0;
				if (snap_row)
					forge_snap_tag_layout_text(inst->group_tag, inst->tag_path, values[5], sizeof(values[5]));
				if (test_drop_row >= 0)
				{
					snprintf(labels[test_drop_row], sizeof(labels[0]), "%s", s_test_dropping ? "STOP TEST DROP" : "TEST DROP");
					values[test_drop_row][0] = 0;
				}
				if (always_row >= 0)
				{
					snprintf(labels[always_row], sizeof(labels[0]), "ALWAYS SPAWN:");
					snprintf(values[always_row], sizeof(values[0]), "%s", inst->always_spawn ? "ON" : "OFF");
				}
				if (scale_row >= 0)
				{
					snprintf(labels[scale_row], sizeof(labels[0]), "SCALE:");
					snprintf(values[scale_row], sizeof(values[0]), "%.1fX", (double)forge_inst_scale(inst));
				}
				if (damage_row >= 0)
				{
					snprintf(labels[damage_row], sizeof(labels[0]), "DEAL DAMAGE:");
					snprintf(values[damage_row], sizeof(values[0]), "%s", inst->no_damage ? "OFF" : "ON");
				}
				if (collision_row >= 0)
				{
					snprintf(labels[collision_row], sizeof(labels[0]), "COLLISION:");
					snprintf(values[collision_row], sizeof(values[0]), "%s", inst->no_collision ? "OFF" : "ON");
				}
			}

			if (factions_page)
			{
				unsigned kinds = forge_ff_drop_kinds(inst);

				for (r = 0; r < 3; r++)
				{
					snprintf(labels[r], sizeof(labels[0]), "%s:", ff_drop_kind_name(s_props_faction_kinds[r]));
					snprintf(values[r], sizeof(values[0]), "%s",
						(kinds & (1U << s_props_faction_kinds[r])) ? "ON" : "OFF");
				}
			}

			for (r = 0; r < row_count; r++)
			{
				short rx0 = (short)(px0 + 14);
				short ry0 = (short)(py0 + 34 + r * 28);
				short rx1 = (short)(px1 - 14);
				short ry1 = (short)(ry0 + 25);
				int sel = (g_forge.props_cursor == r);
				uint32_t col = sel ? 0xFFFFFFFFu : 0xFF2896FFu;

				/* (a route point's ADD POINT BEFORE and AFTER: two buttons side
				by side, the one chosen lit while the row is) */
				s_props_row_split[r & 15] = 0;
				if (is_ai_route && r < route_row_count && route_rows[r] == _forge_ai_route_row_add)
				{
					short half = (short)((rx1 - rx0 - 8) / 2);
					int after = forge_ai_route_add_side();

					forge_hit_add(_forge_hit_props_row, r, rx0, ry0, rx1, ry1);
					s_props_row_split[r & 15] = 1;
					s_props_row_has_value[r & 15] = 0;
					draw_ce_text_button(font_index, rx0, ry0, half, 25, sel && !after, labels[r]);
					draw_ce_text_button(font_index, (short)(rx1 - half), ry0, half, 25, sel && after, values[r]);
					continue;
				}
				draw_ce_rounded_row_bar((short)(sel ? 1 : 0), rx0, ry0, rx1, ry1);
				forge_hit_add(_forge_hit_props_row, r, rx0, ry0, rx1, ry1);
				s_props_row_has_value[r & 15] = values[r][0] != 0;

				{
					/* (a page's button: its summary on the right, a press opens it;
					the arrows are for the rows stepped left and right) */
					int page_button = (is_ai_spawn && r < ai_row_count && FORGE_AI_ROW_IS_PAGE_BUTTON(ai_rows[r])) ||
						(is_ai_route && r < route_row_count && route_rows[r] == _forge_ai_route_row_settings) ||
						(is_volume && !factions_page && r < volume_row_count &&
							(volume_rows[r] == _forge_volume_row_effect_page || volume_rows[r] == _forge_volume_row_size_page));
					short label_w = 0;

					/* (a page's button: a click anywhere on it opens it) */
					if (page_button)
						s_props_row_has_value[r & 15] = 0;
					/* (a size KEEP RATIO holds: shown, dimmed) */
					if (is_volume && !factions_page && r < volume_row_count &&
						forge_volume_row_disabled(g_forge.grabbed_live_index, volume_rows[r]))
					{
						col = sel ? 0xFF8C8C8Cu : 0xFF3C5A78u;
					}

					if (values[r][0])
					{
						/* (the label keeps at most 3/5 of the row, the value the rest) */
						forge_text_fit(1, font_index, labels[r], (short)((rx1 - rx0) * 3 / 5));
						label_w = (short)measure_ce_menu_str_pixels(1, font_index, labels[r]);
					}
					draw_ce_menu_str_box(1, font_index, (short)(rx0 + 10), (short)(ry0 + 2), (short)(rx1 - 10), ry1, 0, labels[r], col);
					if (values[r][0] && page_button)
					{
						short vx0 = (short)(rx0 + 10 + label_w + 12);
						short vx1 = (short)(rx1 - 8);

						draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_right", 0, (short)(vx1 - 6), (short)(ry0 + 6), vx1, (short)(ry0 + 18), 6, 12, 0xFFFFFFFFu);
						forge_text_fit(0, font_index, values[r], (short)(vx1 - 10 - vx0));
						draw_ce_menu_str_box(0, font_index, vx0, (short)(ry0 + 3), (short)(vx1 - 10), ry1, 1 /* right */, values[r], col);
					}
					else if (values[r][0])
					{
						short vx0 = (short)(rx0 + 118);
						short vx1 = (short)(rx1 - 8);

						if (vx0 < rx0 + 10 + label_w + 6)
							vx0 = (short)(rx0 + 10 + label_w + 6);
						draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_left", 0, vx0, (short)(ry0 + 6), (short)(vx0 + 6), (short)(ry0 + 18), 6, 12, 0xFFFFFFFFu);
						draw_ce_menu_bitmap_cropped("pc\\bitmaps\\arrow_sm_right", 0, (short)(vx1 - 6), (short)(ry0 + 6), vx1, (short)(ry0 + 18), 6, 12, 0xFFFFFFFFu);
						forge_text_fit(0, font_index, values[r], (short)(vx1 - 10 - (vx0 + 10)));
						draw_ce_menu_str_box(0, font_index, (short)(vx0 + 10), (short)(ry0 + 3), (short)(vx1 - 10), ry1, 2 /* center */, values[r], col);
					}
				}
			}

			/* (reached with the arrows, as the rows are: forge_3d_update) */
			forge_hit_add(_forge_hit_props_row, row_count, (short)(px0 + 14), (short)(py1 - 32), (short)(px0 + 150), (short)(py1 - 8));
			s_props_row_has_value[row_count & 15] = s_props_row_has_value[(row_count + 1) & 15] = 0;
			draw_ce_text_button(font_index, (short)(px0 + 14), (short)(py1 - 32), 136, 24,
				g_forge.props_cursor == row_count, factions_page ? "BACK" : "DONE");
			if (!factions_page)
			{
				forge_hit_add(_forge_hit_props_row, row_count + 1, (short)(px1 - 150), (short)(py1 - 32), (short)(px1 - 14), (short)(py1 - 8));
				draw_ce_text_button(font_index, (short)(px1 - 150), (short)(py1 - 32), 136, 24,
					g_forge.props_cursor == row_count + 1, g_forge.props_cursor == row_count + 1 ? "HOLD TO UNDO" : "UNDO");
			}
			if (s_undo_hold_fraction > 0.0f)
			{
				rectangle2d undo_bar;

				undo_bar.x0 = (short)(px1 - 148);
				undo_bar.y0 = (short)(py1 - 13);
				undo_bar.x1 = (short)(px1 - 16);
				undo_bar.y1 = (short)(py1 - 9);
				draw_quad(&undo_bar, 0xC0061224u);
				undo_bar.x1 = (short)(undo_bar.x0 + (short)(132.0f * s_undo_hold_fraction));
				draw_quad(&undo_bar, 0xFF2896FFu);
			}
		}

		/* MAP SETTINGS (the palette's button held) */
		if (g_forge.active_modal == _forge_modal_map_settings)
			forge_map_settings_menu_draw(window_x0, window_y0, font_index);

		/* ========================================================================
		 * INITIAL SAVE MAP DIALOG BOX (Opened from Esc -> SAVE MAP..)
		 * ======================================================================== */
		if (g_forge.active_modal == _forge_modal_save_copy_as)
		{
			short cx = (short)((window_x0 + window_x1) / 2);
			short cy = (short)((window_y0 + window_y1) / 2);
			short box_w = 416;
			short box_h = 225;
			short px0 = (short)(cx - box_w / 2);
			short py0 = (short)(cy - box_h / 2 - 8);
			short px1 = (short)(px0 + box_w);
			short py1 = (short)(py0 + box_h);
			short in_x0 = (short)(px0 + 28);
			short in_y0 = (short)(py0 + 74);
			short in_x1 = (short)(px1 - 28);
			short in_y1 = (short)(in_y0 + 28);
			size_t name_len = strlen(g_forge.save_map_name);
			char const *visible_input = g_forge.save_map_name;
			char short_name[32];

			set_forge_hud_ui_scale(FORGE_IN_MATCH_HUD_SCALE, (float)cx, (float)cy);

			if (name_len > 22)
				visible_input = g_forge.save_map_name + (name_len - 22);

			if (name_len > 18)
				snprintf(short_name, sizeof(short_name), "%.15s...", g_forge.save_map_name);
			else
				snprintf(short_name, sizeof(short_name), "%s", g_forge.save_map_name);

			draw_ce_pause_box(px0, py0, box_w, box_h, 1);

			draw_ce_menu_str(1, font_index, (short)(px0 + 18), (short)(py0 + 4),
				"SAVE MAP..", 0xFFFFFFFFu);

			draw_ce_menu_str(1, font_index, in_x0, (short)(py0 + 44),
				"ENTER FORGE MAP NAME:", 0xFF2896FFu);

			if (!draw_ce_rounded_row_bar(1, in_x0, in_y0, in_x1, in_y1))
			{
				rectangle2d ib = { in_y0, in_x0, in_y1, in_x1 };
				draw_quad(&ib, 0xE8040C1Cu);
				draw_box_border(in_x0, in_y0, in_x1, in_y1, 1, 0xFF2896FFu);
			}

			snprintf(buf, sizeof(buf), "%s_", visible_input);
			draw_ce_menu_str_box(1, font_index, (short)(in_x0 + 14), (short)(in_y0 + 3), (short)(in_x1 - 14), in_y1, 0,
				buf, 0xFFFFFFFFu);

			draw_ce_blue_line(in_x0, (short)(py0 + 114), in_x1, 2);

			snprintf(buf, sizeof(buf), "Saves as: custom_maps\\%s.map", short_name);
			draw_ce_menu_str_box(0, font_index, in_x0, (short)(py0 + 124), in_x1, (short)(py0 + 144), 0,
				buf, 0xFF2896FFu);

			snprintf(buf, sizeof(buf), "After initial save, press %s in-game for Quick Save.", p_qsave);
			draw_ce_menu_str_box(0, font_index, in_x0, (short)(py0 + 146), in_x1, (short)(py0 + 168), 0,
				buf, 0xFF2896FFu);

			forge_hit_add(_forge_hit_save_ok, 0, (short)(cx - 138), (short)(py1 - 34), (short)(cx - 10), (short)(py1 - 10));
			forge_hit_add(_forge_hit_save_cancel, 0, (short)(cx + 10), (short)(py1 - 34), (short)(cx + 138), (short)(py1 - 10));
			draw_ce_text_button(font_index, (short)(cx - 138), (short)(py1 - 34), 128, 24, 1,
				g_forge.using_controller ? "{A} SAVE" : "[ENTER] SAVE");
			draw_ce_text_button(font_index, (short)(cx + 10), (short)(py1 - 34), 128, 24, 0,
				g_forge.using_controller ? "{B} CANCEL" : "[ESC] CANCEL");
		}

		/* Status Toast Banner dynamically sized to fit text pixel width */
		if (g_forge.status_message[0] && (uint32_t)system_milliseconds() < g_forge.status_message_until_ms)
		{
			draw_sized_bottom_pill_bar(
				font_index, window_x0, window_x1, (short)(bar_top_y - 4),
				g_forge.status_message, NULL, 1, _forge_ui_area_toast_bar, 0xFFFFFFFFu, NULL);
		}
	}

	set_forge_hud_ui_scale(1.0f, 0.0f, 0.0f);
}



