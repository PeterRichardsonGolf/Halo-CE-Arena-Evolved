/*
AE_SETTINGS_ROWS.C

The SETTINGS model (ae_settings_rows.h): the groups, the rows of CONTROLS, VIEW and HUD as rows.md has them (labels
as they are today), what shows when, the sections, the dialogs' texts.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_number_field.h"
#include "ae_settings_rows.h"

#define COUNT(a) ((short)(sizeof(a) / sizeof((a)[0])))

static struct ae_settings_group_info const groups[AE_SETTINGS_PAGES] =
{
	{ "CONTROLS", "Your buttons, sticks, mouse and keys. Every player on this PC has their own.", AE_SETTINGS_PLAYER,
		1 },
	{ "VIDEO", "The screen, the graphics and the frame rate. One set for this PC.", AE_SETTINGS_PC, 1 },
	{ "VIEW", "What you see in first person, and your shields. Every player has their own.", AE_SETTINGS_PLAYER, 1 },
	{ "AUDIO", "Volume, speakers and voice. One set for this PC.", AE_SETTINGS_PC, 1 },
	{ "HUD", "Your HUD, names, scoreboard and training aids. Every player has their own.", AE_SETTINGS_PLAYER, 1 },
	{ "CHAT AND STREAMING", "Text chat and the streamer word filter. One set for this PC.", AE_SETTINGS_PC, 1 },
	{ "ONLINE", "Internet play, map downloads and what this PC shares. One set for this PC.", AE_SETTINGS_PC, 1 },
	{ "ACCESSIBILITY", "Menu size, motion, help and language.", AE_SETTINGS_MIXED, 1 },
	{ "SYSTEM", "The menus, files, campaign rules, recording and this version. One set for this PC.", AE_SETTINGS_PC, 1 },
	{ "CROSSHAIR", "Draw your own crosshair: its shape, colour, lines and motion. Every player has their own.",
		AE_SETTINGS_PLAYER, 1 },
	{ "CROSSHAIR WEAPONS", "Which crosshair each weapon and vehicle gun uses. Every player has their own.",
		AE_SETTINGS_PLAYER, 1 },
};

struct ae_settings_group_info const *ae_settings_group(short group)
{
	return group >= 0 && group < AE_SETTINGS_PAGES ? &groups[group] : NULL;
}

/* ---------- the values */

static const char *const on_off_texts[] = { "ON", "OFF" };
static const char *const on_off_values[] = { "true", "false" };
/* (player_profile.h's numbers from 0.5.0: OpenCE's FISHSTICKS 6 and RECON 7, the Master Chief Collection's Hero, then
Arena Evolved's own from the Master Chief Collection's RECON 8) */
static const char *const layout_texts[] = { "STANDARD", "SWAP TRIGGERS", "SWAP A, L TRIGGER", "SWAP B, L TRIGGER",
	"SWAP B, R STICK", "BUMPER JUMPER", "FISHSTICKS", "HERO", "RECON", "THE DUKE", "ANNIVERSARY", "RECLAIMER (DEFAULT)",
	"ZOOM & SHOOT", "BUMP & JUMP", "GREEN FINGERS", "CUSTOM" };
static const char *const layout_values[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
	"14", "15" };
static const char *const stick_texts[] = { "DEFAULT", "SOUTHPAW", "LEGACY", "LEGACY SOUTHPAW" };
static const char *const look_texts[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
static const char *const zoom_texts[] = { "TOGGLE", "HOLD", "BOTH" };
static const char *const zoom_values[] = { "toggle", "hold", "both" };
static const char *const reset_texts[] = { "FIRST LEVEL", "LAST USED" };
static const char *const reset_values[] = { "first", "last" };
static const char *const paddle_texts[] = { "NONE, NONE" };
static const char *const paddle_values[] = { "none" };
static const char *const strength_texts[] = { "LIGHT", "NORMAL", "STRONG" };
static const char *const strength_values[] = { "light", "normal", "strong" };
static const char *const lightbar_texts[] = { "OFF", "PLAYER COLOR", "SHIELD" };
static const char *const lightbar_values[] = { "off", "player_color", "shield" };
static const char *const aim_texts[] = { "CLASSIC", "MODERN" };
static const char *const aim_values[] = { "classic", "modern" };
static const char *const style_texts[] = { "CLASSIC", "MCC" };
static const char *const style_values[] = { "classic", "mcc" };
static const char *const classic_texts[] = { "0.1", "0.15", "0.25", "0.4", "0.5", "0.75", "1.0", "1.25", "1.5",
	"1.75", "2.0", "2.5", "3.0", "4.0" };
static const char *const classic_values[] = { "0.1", "0.15", "0.25", "0.4", "0.5", "0.75", "1", "1.25", "1.5", "1.75",
	"2", "2.5", "3", "4" };
static const char *const vertical_texts[] = { "SAME", "0.1", "0.15", "0.25", "0.4", "0.5", "0.75", "1.0", "1.25",
	"1.5", "1.75", "2.0", "2.5", "3.0", "4.0" };
static const char *const vertical_values[] = { "0", "0.1", "0.15", "0.25", "0.4", "0.5", "0.75", "1", "1.25", "1.5",
	"1.75", "2", "2.5", "3", "4" };
static const char *const keys_texts[] = { "21 ACTIONS" };
static const char *const keys_values[] = { "" };
static const char *const custom_layout_texts[] = { "NO CHANGES" };
static const char *const shield_texts[] = { "DEFAULT", "WHITE", "BLACK", "RED", "BLUE", "GRAY", "YELLOW", "GREEN",
	"PINK", "PURPLE", "CYAN", "COBALT", "ORANGE", "TEAL", "SAGE", "BROWN", "TAN", "MAROON", "SALMON" };
static const char *const shield_values[] = { "default", "white", "black", "red", "blue", "gray", "yellow", "green",
	"pink", "purple", "cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon" };
static const char *const intensity_texts[] = { "DEFAULT", "LIGHT SHOW" };
static const char *const intensity_values[] = { "default", "light_show" };
/* (the host's shield effects in network games: network_shield_effects.c) */
static const char *const shield_effects_texts[] = { "STOCK", "GLOW", "LIGHT SHOW", "OVERSHIELD", "GLOW + OVERSHIELD",
	"SHOW + OVERSHIELD", "PLAYERS' OWN" };
static const char *const shield_effects_values[] = { "stock", "glow", "light_show", "overshield", "glow_overshield",
	"light_show_overshield", "players" };
/* (0.7.8) VIEWMODEL SWAY: how far the weapon leans as you turn and walk, in percent of the stock lean */
static const char *const sway_texts[] = { "OFF", "25%", "50%", "75%", "STOCK" };
static const char *const sway_values[] = { "0", "25", "50", "75", "100" };
static const char *const hud_style_texts[] = { "AE", "CLASSIC" };
static const char *const hud_style_values[] = { "ae", "classic" };
static const char *const area_texts[] = { "FULL", "16:9", "4:3" };
static const char *const area_values[] = { "full", "16:9", "4:3" };
static const char *const clock_texts[] = { "OFF", "REMAINING", "ELAPSED", "BOTH" };
static const char *const clock_values[] = { "off", "down", "up", "both" };
static const char *const button_texts[] = { "CLASSIC", "GEMS", "DARK GEMS" };
static const char *const button_values[] = { "classic", "gems", "menu_gems" };
static const char *const names_texts[] = { "ALL", "ALLIES", "ENEMIES", "NONE" };
static const char *const names_values[] = { "all", "allies", "enemies", "none" };
/* (0.7.8: in percent like every other size row; the stored values stay the scale) */
static const char *const name_size_texts[] = { "50 %", "75 %", "100 %", "125 %", "150 %", "200 %" };
static const char *const name_size_values[] = { "0.5", "0.75", "1", "1.25", "1.5", "2" };
static const char *const enemy_texts[] = { "CLASSIC", "RED" };
static const char *const enemy_values[] = { "classic", "red" };
static const char *const layout_board_texts[] = { "TEAMS", "BY SCORE" };
static const char *const layout_board_values[] = { "teams", "score" };
static const char *const fade_texts[] = { "INSTANT", "FAST", "NORMAL", "SLOW" };
static const char *const fade_values[] = { "instant", "fast", "normal", "slow" };
static const char *const panel_texts[] = { "DARK", "BLACK", "BLUE", "RED", "GREEN" };
static const char *const panel_values[] = { "16, 16, 16, 150", "0, 0, 0, 200", "16, 32, 64, 160", "64, 16, 16, 160",
	"16, 48, 24, 160" };
static const char *const medal_style_texts[] = { "UNIVERSAL", "MCC" };
static const char *const medal_style_values[] = { "modern", "mcc" };
static const char *const waypoint_texts[] = { "TEXT", "ICONS", "BOTH" };
static const char *const waypoint_values[] = { "text", "icons", "both" };
static const char *const power_texts[] = { "CLOCK", "TOP LEFT" };
static const char *const power_values[] = { "clock", "top_left" };
static const char *const heat_texts[] = { "OFF", "MINE", "ENEMY" };
/* (0.3.7: the way back to the classic menus while AE's lack a screen, CO-OP first; the classic menus' MENUS row
comes back here) */
static const char *const menus_texts[] = { "ARENA EVOLVED", "CLASSIC" };
static const char *const menus_values[] = { "true", "false" };
static const char *const heat_values[] = { "off", "mine", "enemy" };
static const char *const callout_texts[] = { "OFF", "ITEMS", "ITEMS + CLOCK" };
static const char *const callout_values[] = { "off", "items", "items_clock" };
static const char *const detail_texts[] = { "MINIMAL", "STANDARD", "VERBOSE" };
static const char *const detail_values[] = { "minimal", "standard", "verbose" };
/* (0.3.9: the settings only the classic menus had; their texts are the classic menus') */
static const char *const mcc_use_texts[] = { "ASK", "ON", "OFF" };
static const char *const mcc_use_values[] = { "ask", "yes", "no" };
static const char *const health_texts[] = { "HALO CE", "REACH", "HALO 2", "HALO 3" };
static const char *const health_values[] = { "classic", "reach", "halo2", "halo3" };

/* (phase B) VIDEO */
static const char *const display_mode_texts[] = { "FULLSCREEN", "BORDERLESS", "WINDOWED" };
static const char *const display_mode_values[] = { "fullscreen", "borderless", "windowed" };
static const char *const scaling_texts[] = { "NATIVE", "ORIGINAL" };
static const char *const scaling_values[] = { "native", "original" };
/* (owner 2026-10-29: every common monitor and handheld rate; a typed value is 0.5.1's) */
static const char *const fps_texts[] = { "AUTO", "30", "40", "45", "50", "60", "72", "75", "90", "100", "120", "144",
	"165", "180", "200", "240", "280", "300", "360", "NONE" };
static const char *const fps_values[] = { "0", "30", "40", "45", "50", "60", "72", "75", "90", "100", "120", "144",
	"165", "180", "200", "240", "280", "300", "360", "-1" };
/* (0.7.5) the limits' steps: 0 is "the port's own 8192" for particles and "no limit" for lights */
static const char *const particle_texts[] = { "1024", "2048", "4096", "8192" };
static const char *const particle_values[] = { "1024", "2048", "4096", "8192" };
static const char *const light_texts[] = { "4", "8", "16", "NO LIMIT" };
static const char *const light_values[] = { "4", "8", "16", "0" };
/* (structure culling's bit mask: 31 every test, 0 the Xbox's walk; any other number shows as CUSTOM) */
static const char *const culling_texts[] = { "ON", "OFF" };
static const char *const culling_values[] = { "31", "0" };
static const char *const record_quality_texts[] = { "LOW", "MEDIUM", "HIGH" };
static const char *const record_quality_values[] = { "low", "medium", "high" };
static const char *const record_fps_texts[] = { "30", "60" };
static const char *const record_fps_values[] = { "30", "60" };
static const char *const aa_texts[] = { "OFF", "FXAA", "SMAA", "SSAA 2X", "MSAA 2X", "MSAA 4X", "MSAA 8X" };
static const char *const aa_values[] = { "off", "fxaa", "smaa", "ssaa2x", "msaa2x", "msaa4x", "msaa8x" };
static const char *const aniso_texts[] = { "OFF", "2X", "4X", "8X", "16X" };
static const char *const aniso_values[] = { "off", "2x", "4x", "8x", "16x" };
static const char *const shadow_texts[] = { "128", "256", "512", "1024" };
static const char *const model_texts[] = { "STOCK", "NATIVE", "HIGHEST" };
static const char *const model_values[] = { "stock", "native", "highest" };
static const char *const distance_texts[] = { "1024", "2048", "4096", "10000" };
static const char *const performance_texts[] = { "OFF", "FPS", "MINIMAL", "FULL" };
static const char *const performance_values[] = { "off", "fps", "minimal", "full" };
static const char *const swap_texts[] = { "OFF", "IN GAME", "EVERYWHERE" };
static const char *const swap_values[] = { "off", "in_game", "everywhere" };
static const char *const prompts_texts[] = { "AUTO", "XBOX", "PLAYSTATION", "NINTENDO", "UNIVERSAL" };
static const char *const prompts_values[] = { "auto", "xbox", "playstation", "nintendo", "universal" };
static const char *const ping_texts[] = { "OFF", "NUMBER", "NUMBER AND BARS" };
static const char *const ping_values[] = { "off", "number", "number_and_bars" };
static const char *const corner_texts[] = { "TOP LEFT", "TOP RIGHT" };
static const char *const corner_values[] = { "top_left", "top_right" };
static const char *const exposure_texts[] = { "0.25", "0.5", "0.75", "1.0", "1.25", "1.5", "2.0", "2.5", "3.0", "4.0" };
static const char *const exposure_values[] = { "0.25", "0.5", "0.75", "1", "1.25", "1.5", "2", "2.5", "3", "4" };
static const char *const tone_texts[] = { "NEUTRAL", "ACES", "REINHARD" };
static const char *const tone_values[] = { "neutral", "aces", "reinhard" };

/* (phase B) AUDIO */
static const char *const speakers_texts[] = { "STEREO", "5.1", "AUTO" };
static const char *const speakers_values[] = { "stereo", "5.1", "auto" };
static const char *const ragdoll_texts[] = { "NONE", "SPAGHETTI WESTERN", "EXAGGERATED", "MODERN", "MIXED" };
static const char *const ragdoll_values[] = { "none", "spaghetti_western", "exaggerated", "modern", "mixed" };
static const char *const menu_music_texts[] = { "CLASSIC", "MCC SHUFFLE", "MCC IN ORDER", "OFF" };
static const char *const menu_music_values[] = { "classic", "mcc_shuffle", "mcc_order", "off" };
static const char *const voice_chat_texts[] = { "PUSH TO TALK", "OPEN MIC", "OFF" };
static const char *const voice_chat_values[] = { "push_to_talk", "open_mic", "off" };
static const char *const resampler_texts[] = { "SINC", "LINEAR" };
static const char *const resampler_values[] = { "sinc", "linear" };

/* (phase B) ONLINE */
static const char *const yes_no_values[] = { "yes", "no" };
static const char *const ask_texts[] = { "ASK", "ALWAYS", "NEVER" };
static const char *const ask_values[] = { "ask", "always", "never" };
static const char *const upload_texts[] = { "64 KB/S", "128 KB/S", "256 KB/S", "512 KB/S", "1 MB/S", "2 MB/S", "4 MB/S",
	"8 MB/S", "10 MB/S" };
static const char *const upload_values[] = { "64", "128", "256", "512", "1024", "2048", "4096", "8192", "10240" };
static const char *const download_texts[] = { "256 MB", "512 MB", "1 GB", "2 GB", "4 GB", "8 GB", "16 GB", "64 GB" };
static const char *const download_values[] = { "256", "512", "1024", "2048", "4096", "8192", "16384", "65536" };
static const char *const discord_texts[] = { "FULL", "MINIMAL", "OFF" };
static const char *const discord_values[] = { "full", "minimal", "off" };
static const char *const protocol_texts[] = { "AUTO", "OPENCE", "DELTA" };
static const char *const protocol_values[] = { "auto", "opence", "delta" };

/* (phase B) ACCESSIBILITY */
static const char *const menu_size_texts[] = { "90 %", "100 %", "115 %", "130 %" };
static const char *const menu_size_values[] = { "90", "100", "115", "130" };
static const char *const language_texts[] = { "ENGLISH", "DEUTSCH", "FRANCAIS", "ESPANOL", "ITALIANO", "JAPANESE" };
static const char *const language_values[] = { "", "de", "fr", "es", "it", "ja" };
/* (CONTROLS' GYRO: input.gyro_*, this PC's, each pad its own sensor; sensitivity 0.25 to 10 as port_config.c has it) */
static const char *const gyro_texts[] = { "OFF", "ALWAYS", "ZOOMED IN" };
static const char *const gyro_values[] = { "false", "true", "zoom" };
static const char *const gyro_sensitivity_texts[] = { "0.25", "0.5", "0.75", "1", "1.25", "1.5", "2", "2.5", "3", "4",
	"5", "6", "8", "10" };
static const char *const gyro_sensitivity_values[] = { "0.25", "0.5", "0.75", "1.0", "1.25", "1.5", "2.0", "2.5", "3.0",
	"4.0", "5.0", "6.0", "8.0", "10.0" };
/* (the least time the loading screen stays up, seconds: display.loading_screen_*_seconds; multiplayer's at most 5, as
port_config.c has it) */
static const char *const campaign_loading_texts[] = { "NO WAIT", "2 S", "5 S", "10 S", "15 S", "20 S", "30 S" };
static const char *const campaign_loading_values[] = { "0", "2", "5", "10", "15", "20", "30" };
static const char *const multiplayer_loading_texts[] = { "NO WAIT", "1 S", "2 S", "3 S", "5 S" };
static const char *const multiplayer_loading_values[] = { "0", "1", "2", "3", "5" };

/* (phase B) CHAT AND STREAMING */
static const char *const show_texts[] = { "SHOW", "HIDE" };
static const char *const chat_style_texts[] = { "COLORS", "WHITE" };
static const char *const chat_style_values[] = { "colors", "white" };
static const char *const censor_texts[] = { "OFF", "SEVERE", "STRONG AND WORSE", "MILD AND WORSE" };
static const char *const censor_values[] = { "off", "severe", "strong", "mild" };
static const char *const look_acceleration_texts[] = { "OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
static const char *const look_acceleration_values[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
static const char *const vertical_look_texts[] = { "SAME", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
static const char *const vertical_look_values[] = { "same", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
static const char *const radial_deadzone_texts[] = { "SAME", "0 %", "5 %", "10 %", "15 %", "20 %" };
static const char *const radial_deadzone_values[] = { "same", "0", "5", "10", "15", "20" };

/* (the long numeric lists: made once) */
enum { MCC_STEPS = 100, SCALE_STEPS = 20, HOLD_STEPS = 19, FOV_STEPS = 15, DEADZONE_STEPS = 9, WAYPOINT_STEPS = 11,
	VOLUME_STEPS = 11 };
/* (AUDIO's volumes: 0 to 10 as shown, 0 to 1 as config.toml has them, as the classic menus write them) */
static char volume_text[VOLUME_STEPS][16], volume_value[VOLUME_STEPS][16];
static const char *volume_texts[VOLUME_STEPS], *volume_values[VOLUME_STEPS];
static char mcc_text[MCC_STEPS][8], scale_text[SCALE_STEPS][8], hold_text[HOLD_STEPS][8], hold_value[HOLD_STEPS][8];
static char fov_text[FOV_STEPS][16], fov_value[FOV_STEPS][8], deadzone_text[DEADZONE_STEPS][8], deadzone_value[DEADZONE_STEPS][8];
static char waypoint_size_text[WAYPOINT_STEPS][8], waypoint_size_value[WAYPOINT_STEPS][8];
static const char *mcc_texts[MCC_STEPS], *scale_texts[SCALE_STEPS], *hold_texts[HOLD_STEPS], *hold_values[HOLD_STEPS];
static const char *fov_texts[FOV_STEPS], *fov_values[FOV_STEPS], *deadzone_texts[DEADZONE_STEPS],
	*deadzone_values[DEADZONE_STEPS], *waypoint_size_texts[WAYPOINT_STEPS], *waypoint_size_values[WAYPOINT_STEPS];
/* the run-time rows' values (ae_settings_set_values): CALLOUT VOICE's voices, VIDEO's display sizes, AUDIO's devices,
ANNOUNCER's announcers */
enum { RUN_LISTS = 9, RUN_VALUES = 64, RUN_TEXT = 64 };
static const char *const run_keys[RUN_LISTS] = { "game.callout_voice", "display.resolution", "display.window_size",
	"audio.output_device", "audio.input_device", "game.disc", "xh.main", "xw.list", "game.announcer" };
static struct
{
	char value[RUN_VALUES][RUN_TEXT], text[RUN_VALUES][RUN_TEXT];
	const char *values[RUN_VALUES], *texts[RUN_VALUES];
	short count;
} run_lists[RUN_LISTS];

static void xh_lists_build(void);

static void lists_build(void)
{
	static int built;
	int index;

	if (built)
		return;
	built = 1;
	xh_lists_build();
	for (index = 0; index < MCC_STEPS; index++)
	{
		snprintf(mcc_text[index], sizeof(mcc_text[index]), "%.1f", (double)(index + 1) / 10.0);
		mcc_texts[index] = mcc_text[index];
	}
	for (index = 0; index < SCALE_STEPS; index++)
	{
		snprintf(scale_text[index], sizeof(scale_text[index]), "%.1f", (double)(index + 1) / 10.0);
		scale_texts[index] = scale_text[index];
	}
	for (index = 0; index < HOLD_STEPS; index++)
	{
		snprintf(hold_value[index], sizeof(hold_value[index]), "%.2f", 0.10 + 0.05 * (double)index);
		snprintf(hold_text[index], sizeof(hold_text[index]), "%s S", hold_value[index]);
		hold_values[index] = hold_value[index];
		hold_texts[index] = hold_text[index];
	}
	/* (0.7.8) 0 is the game's own view, STOCK; the default is 85, marked in its place */
	fov_texts[0] = "STOCK";
	fov_values[0] = "0";
	for (index = 1; index < FOV_STEPS; index++)
	{
		snprintf(fov_value[index], sizeof(fov_value[index]), "%d", 75 + 5 * index);
		snprintf(fov_text[index], sizeof(fov_text[index]), 75 + 5 * index == 85 ? "%d (DEFAULT)" : "%d", 75 + 5 * index);
		fov_texts[index] = fov_text[index];
		fov_values[index] = fov_value[index];
	}
	deadzone_texts[0] = "DEFAULT";
	deadzone_values[0] = "0";
	for (index = 1; index < DEADZONE_STEPS; index++)
	{
		snprintf(deadzone_value[index], sizeof(deadzone_value[index]), "%d", 5 * index);
		snprintf(deadzone_text[index], sizeof(deadzone_text[index]), "%d %%", 5 * index);
		deadzone_texts[index] = deadzone_text[index];
		deadzone_values[index] = deadzone_value[index];
	}
	for (index = 0; index < VOLUME_STEPS; index++)
	{
		snprintf(volume_text[index], sizeof(volume_text[index]), "%d", index);
		snprintf(volume_value[index], sizeof(volume_value[index]), "%g", (double)index / 10.0);
		volume_texts[index] = volume_text[index];
		volume_values[index] = volume_value[index];
	}
	for (index = 0; index < WAYPOINT_STEPS; index++)
	{
		snprintf(waypoint_size_value[index], sizeof(waypoint_size_value[index]), "%d", 50 + 5 * index);
		snprintf(waypoint_size_text[index], sizeof(waypoint_size_text[index]), "%d %%", 50 + 5 * index);
		waypoint_size_texts[index] = waypoint_size_text[index];
		waypoint_size_values[index] = waypoint_size_value[index];
	}
}

/* ---------- (0.7.7) CROSSHAIR's lists: lengths and offsets in half pixels, thicknesses, opacities, the palette */

enum { XH_HALF = 81, XH_THICK = 20, XH_CENTRE = 16, XH_OUTLINE = 8, XH_OPACITY = 21, XH_OPACITY_MAIN = 19, XH_FRAME = 31,
	XH_RETICLE = 31, XH_PALETTE = 10 };
static char xh_half_text[XH_HALF][16], xh_half_value[XH_HALF][12], xh_thick_text[XH_THICK][16], xh_thick_value[XH_THICK][12],
	xh_centre_text[XH_CENTRE][16], xh_centre_value[XH_CENTRE][12], xh_outline_text[XH_OUTLINE][16],
	xh_outline_value[XH_OUTLINE][12], xh_opacity_text[XH_OPACITY][16], xh_opacity_value[XH_OPACITY][12],
	xh_opacity_main_text[XH_OPACITY_MAIN][16], xh_opacity_main_value[XH_OPACITY_MAIN][12], xh_frame_text[XH_FRAME][16],
	xh_frame_value[XH_FRAME][12], xh_reticle_text[XH_RETICLE][16], xh_reticle_value[XH_RETICLE][12],
	xh_palette_value[XH_PALETTE][12];
static const char *xh_half_texts[XH_HALF], *xh_half_values[XH_HALF], *xh_thick_texts[XH_THICK], *xh_thick_values[XH_THICK],
	*xh_centre_texts[XH_CENTRE], *xh_centre_values[XH_CENTRE], *xh_outline_texts[XH_OUTLINE], *xh_outline_values[XH_OUTLINE],
	*xh_opacity_texts[XH_OPACITY], *xh_opacity_values[XH_OPACITY], *xh_opacity_main_texts[XH_OPACITY_MAIN],
	*xh_opacity_main_values[XH_OPACITY_MAIN], *xh_frame_texts[XH_FRAME], *xh_frame_values[XH_FRAME],
	*xh_reticle_texts[XH_RETICLE], *xh_reticle_values[XH_RETICLE], *xh_palette_texts[XH_PALETTE],
	*xh_palette_values[XH_PALETTE];
static const char *const xh_centre_kind_texts[] = { "NONE", "DOT", "PLUS" };
static const char *const xh_centre_kind_values[] = { "0", "1", "2" };
static const char *const xh_scaling_texts[] = { "FOLLOWS FOV", "FIXED" };
static const char *const xh_scaling_values[] = { "fov", "fixed" };
static const char *const xh_fov_base_texts[] = { "78", "70 (STOCK)" };
static const char *const xh_fov_base_values[] = { "78", "70" };
static const char *const xh_motion_texts[] = { "RIFLES", "ALL WEAPONS", "OFF" };
static const char *const xh_scope_texts[] = { "SAME CROSSHAIR", "WEAPON'S SCOPE" };
static const char *const xh_scope_values[] = { "same", "scope" };
static const char *const xh_motion_values[] = { "rifles", "all", "off" };
static const char *const xh_tint_texts[] = { "ORIGINAL", "SAME AS HUD", "WHITE", "ICE", "CYAN", "PINK", "YELLOW" };
static const char *const xh_tint_values[] = { "original", "hud", "white", "ice", "cyan", "pink", "yellow" };
/* (0.7.8) ENEMY COLOR / ALLY COLOR: the words ae_xh_state_color_of reads */
static const char *const xh_state_texts[] = { "RED", "ORANGE", "YELLOW", "GREEN", "CYAN", "BLUE", "MAGENTA", "WHITE" };
static const char *const xh_state_values[] = { "red", "orange", "yellow", "green", "cyan", "blue", "magenta", "white" };
static const char *const xh_palette_names[XH_PALETTE] = { "WHITE", "ICE", "CYAN", "PINK", "YELLOW", "ORANGE", "VIOLET",
	"BLUE", "GRAY", "BLACK" };

static void xh_lists_build(void)
{
	int index;

	for (index = 0; index < XH_HALF; index++)
	{
		snprintf(xh_half_value[index], sizeof(xh_half_value[index]), "%d", index);
		snprintf(xh_half_text[index], sizeof(xh_half_text[index]), index % 2 ? "%d.5 PX" : "%d PX", index / 2);
		xh_half_values[index] = xh_half_value[index];
		xh_half_texts[index] = xh_half_text[index];
	}
	for (index = 0; index < XH_THICK; index++)
	{
		snprintf(xh_thick_value[index], sizeof(xh_thick_value[index]), "%d", index + 1);
		snprintf(xh_thick_text[index], sizeof(xh_thick_text[index]), (index + 1) % 2 ? "%d.5 PX" : "%d PX", (index + 1) / 2);
		xh_thick_values[index] = xh_thick_value[index];
		xh_thick_texts[index] = xh_thick_text[index];
	}
	for (index = 0; index < XH_CENTRE; index++)
	{
		snprintf(xh_centre_value[index], sizeof(xh_centre_value[index]), "%d", index + 1);
		snprintf(xh_centre_text[index], sizeof(xh_centre_text[index]), (index + 1) % 2 ? "%d.5 PX" : "%d PX", (index + 1) / 2);
		xh_centre_values[index] = xh_centre_value[index];
		xh_centre_texts[index] = xh_centre_text[index];
	}
	for (index = 0; index < XH_OUTLINE; index++)
	{
		snprintf(xh_outline_value[index], sizeof(xh_outline_value[index]), "%d", index + 1);
		snprintf(xh_outline_text[index], sizeof(xh_outline_text[index]), (index + 1) % 2 ? "%d.5 PX" : "%d PX", (index + 1) / 2);
		xh_outline_values[index] = xh_outline_value[index];
		xh_outline_texts[index] = xh_outline_text[index];
	}
	for (index = 0; index < XH_OPACITY; index++)
	{
		snprintf(xh_opacity_value[index], sizeof(xh_opacity_value[index]), "%d", index * 5);
		snprintf(xh_opacity_text[index], sizeof(xh_opacity_text[index]), "%d %%", index * 5);
		xh_opacity_values[index] = xh_opacity_value[index];
		xh_opacity_texts[index] = xh_opacity_text[index];
	}
	for (index = 0; index < XH_OPACITY_MAIN; index++)
	{
		snprintf(xh_opacity_main_value[index], sizeof(xh_opacity_main_value[index]), "%d", 10 + index * 5);
		snprintf(xh_opacity_main_text[index], sizeof(xh_opacity_main_text[index]), "%d %%", 10 + index * 5);
		xh_opacity_main_values[index] = xh_opacity_main_value[index];
		xh_opacity_main_texts[index] = xh_opacity_main_text[index];
	}
	for (index = 0; index < XH_FRAME; index++)
	{
		snprintf(xh_frame_value[index], sizeof(xh_frame_value[index]), "%d", 50 + index * 5);
		snprintf(xh_frame_text[index], sizeof(xh_frame_text[index]), "%d %%", 50 + index * 5);
		xh_frame_values[index] = xh_frame_value[index];
		xh_frame_texts[index] = xh_frame_text[index];
	}
	for (index = 0; index < XH_RETICLE; index++)
	{
		snprintf(xh_reticle_value[index], sizeof(xh_reticle_value[index]), "%d", 50 + index * 5);
		snprintf(xh_reticle_text[index], sizeof(xh_reticle_text[index]), "%d %%", 50 + index * 5);
		xh_reticle_values[index] = xh_reticle_value[index];
		xh_reticle_texts[index] = xh_reticle_text[index];
	}
	for (index = 0; index < XH_PALETTE; index++)
	{
		snprintf(xh_palette_value[index], sizeof(xh_palette_value[index]), "%d", index);
		xh_palette_values[index] = xh_palette_value[index];
		xh_palette_texts[index] = xh_palette_names[index];
	}
}

/* ---------- the rows (rows.md) */

#define VALUES(t, v) t, v, COUNT(t)
#define ON_OFF VALUES(on_off_texts, on_off_values)

static struct ae_settings_row const rows[] =
{
	/* CONTROLS */
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "BUTTON LAYOUT", AE_SETTINGS_SPINNER, "profile.button_preset",
		VALUES(layout_texts, layout_values), AE_SETTINGS_ALWAYS,
		"Which button does what. {A} lists every layout. CUSTOM is your own: set its buttons on CUSTOM LAYOUT, the next "
		"row.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "CUSTOM LAYOUT", AE_SETTINGS_SUBSCREEN, "controls.controller_layout",
		VALUES(custom_layout_texts, keys_values), AE_SETTINGS_ALWAYS,
		"Press the button you want for each action. Your BUTTON LAYOUT becomes CUSTOM.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "A/B SWAP", AE_SETTINGS_SPINNER, "input.ab_swap",
		VALUES(swap_texts, swap_values), AE_SETTINGS_WHEN_NINTENDO,
		"OFF: every button does what its label says, here and in the game. IN GAME: the game goes by an Xbox pad's places "
		"(the bottom button jumps), the menus by labels. EVERYWHERE: the menus by places too.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "BUTTON PROMPTS", AE_SETTINGS_SPINNER, "input.button_prompts",
		VALUES(prompts_texts, prompts_values), AE_SETTINGS_ALWAYS,
		"The pictures of your pad's buttons in the menus, the footer showing the family live. AUTO: your pad's own. "
		"UNIVERSAL: a white disc with the button's place marked.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "STICK LAYOUT", AE_SETTINGS_SPINNER, "profile.joystick_preset",
		stick_texts, layout_values, COUNT(stick_texts), AE_SETTINGS_ALWAYS,
		"SOUTHPAW swaps the sticks. LEGACY moves with the right stick and looks with the left.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "LOOK SENSITIVITY", AE_SETTINGS_SLIDER, "profile.look_sensitivity",
		look_texts, look_texts, COUNT(look_texts), AE_SETTINGS_ALWAYS,
		"How fast the right stick turns you. Halo CE's own scale.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "AIM CONTROL", AE_SETTINGS_SPINNER, "input.aim_control",
		VALUES(aim_texts, aim_values), AE_SETTINGS_ALWAYS,
		"MODERN: a smoother stick curve and a turn that speeds up while you hold the stick all the way. CLASSIC: the "
		"original look.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "LOOK ACCELERATION", AE_SETTINGS_SLIDER, "input.look_acceleration",
		VALUES(look_acceleration_texts, look_acceleration_values), AE_SETTINGS_WHEN_AIM_MODERN,
		"With MODERN: how fast the turn speeds up while the stick is held all the way. OFF: no speed-up.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "VERTICAL LOOK SENSITIVITY", AE_SETTINGS_SLIDER,
		"input.look_sensitivity_vertical", VALUES(vertical_look_texts, vertical_look_values), AE_SETTINGS_WHEN_AIM_MODERN,
		"With MODERN: how fast you look up and down. SAME: LOOK SENSITIVITY.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "LOOK RADIAL DEAD ZONE", AE_SETTINGS_SLIDER, "input.look_radial_deadzone",
		VALUES(radial_deadzone_texts, radial_deadzone_values), AE_SETTINGS_WHEN_AIM_MODERN,
		"With MODERN: the radial step of the look stick dead zone. SAME: RIGHT STICK DEADZONE.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "INVERT LOOK", AE_SETTINGS_TOGGLE, "profile.invert_look", ON_OFF,
		AE_SETTINGS_ALWAYS, "Push up to look down.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "INVERT FLIGHT", AE_SETTINGS_TOGGLE, "profile.flight_inversion", ON_OFF,
		AE_SETTINGS_ALWAYS, "Push up to dive in a Banshee or Ghost, like a flight stick.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "AUTO-CENTER LOOK", AE_SETTINGS_TOGGLE, "profile.autocenter", ON_OFF,
		AE_SETTINGS_ALWAYS, "Your view drifts back to level while you walk and don't look up or down.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "VIBRATION", AE_SETTINGS_TOGGLE, "profile.vibration", ON_OFF,
		AE_SETTINGS_ALWAYS, "The controller rumbles when you fire, get hit or drive.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "LEFT STICK DEADZONE", AE_SETTINGS_SLIDER, "input.left_stick_deadzone",
		deadzone_texts, deadzone_values, DEADZONE_STEPS, AE_SETTINGS_ALWAYS,
		"How far the stick moves before you do. DEFAULT is the game's 27 % (12.5 % with MODERN). Raise it if you drift.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "RIGHT STICK DEADZONE", AE_SETTINGS_SLIDER, "input.right_stick_deadzone",
		deadzone_texts, deadzone_values, DEADZONE_STEPS, AE_SETTINGS_ALWAYS,
		"How far the stick moves before your view turns. DEFAULT is the game's 27 % (12 % with MODERN). Lower is quicker to aim, higher stops drift.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "ZOOM", AE_SETTINGS_SPINNER, "input.zoom_mode",
		VALUES(zoom_texts, zoom_values), AE_SETTINGS_ALWAYS,
		"The controller's zoom. TOGGLE: press to zoom. HOLD: zoom while held. BOTH: a tap toggles, a hold holds.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "ZOOM RESET", AE_SETTINGS_SPINNER, "input.zoom_reset",
		VALUES(reset_texts, reset_values), AE_SETTINGS_ALWAYS,
		"Where the zoom starts the next time you zoom in.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "ZOOM HOLD TIME", AE_SETTINGS_SLIDER, "input.zoom_hold_time",
		hold_texts, hold_values, HOLD_STEPS, AE_SETTINGS_WHEN_ZOOM_BOTH,
		"With BOTH: how long a press must be to count as a hold.", 0 },
	{ AE_SETTINGS_CONTROLS, "CONTROLLER", "BACK PADDLES", AE_SETTINGS_SUBSCREEN, "input.back_left",
		VALUES(paddle_texts, paddle_values), AE_SETTINGS_ALWAYS,
		"Give the back paddles (Elite, Edge, Switch 2 Pro) a button each.", 0 },
	{ AE_SETTINGS_CONTROLS, "DUALSENSE", "ADAPTIVE TRIGGERS", AE_SETTINGS_TOGGLE, "input.adaptive_triggers", ON_OFF,
		AE_SETTINGS_WHEN_DUALSENSE, "The triggers push back like each weapon's trigger.", 0 },
	{ AE_SETTINGS_CONTROLS, "DUALSENSE", "TRIGGER STRENGTH", AE_SETTINGS_SPINNER, "input.trigger_strength",
		VALUES(strength_texts, strength_values), AE_SETTINGS_WHEN_DUALSENSE,
		"How hard the triggers push back. LIGHT is a hair trigger on both.", 0 },
	{ AE_SETTINGS_CONTROLS, "DUALSENSE", "LIGHTBAR", AE_SETTINGS_SPINNER, "input.lightbar",
		VALUES(lightbar_texts, lightbar_values), AE_SETTINGS_WHEN_DUALSENSE,
		"The light on the controller: your armour colour, or your shield level.", 0 },
	{ AE_SETTINGS_CONTROLS, "GYRO", "GYRO AIMING", AE_SETTINGS_SPINNER, "input.gyro_aim", VALUES(gyro_texts, gyro_values),
		AE_SETTINGS_ALWAYS, "Aim by moving a DualSense, DualShock 4, Pro Controller or Joy-Con. ZOOMED IN: only in a scope.",
		AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "GYRO", "TURN SENSITIVITY", AE_SETTINGS_SLIDER, "input.gyro_sensitivity_horizontal",
		VALUES(gyro_sensitivity_texts, gyro_sensitivity_values), AE_SETTINGS_WHEN_GYRO,
		"How far turning the controller left and right turns you. 1 is the controller's own turn.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "GYRO", "TILT SENSITIVITY", AE_SETTINGS_SLIDER, "input.gyro_sensitivity_vertical",
		VALUES(gyro_sensitivity_texts, gyro_sensitivity_values), AE_SETTINGS_WHEN_GYRO,
		"How far tilting the controller up and down looks up and down.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "GYRO", "INVERT TURN", AE_SETTINGS_TOGGLE, "input.gyro_invert_horizontal", ON_OFF,
		AE_SETTINGS_WHEN_GYRO, "Turning the controller right looks left.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "GYRO", "INVERT TILT", AE_SETTINGS_TOGGLE, "input.gyro_invert_vertical", ON_OFF,
		AE_SETTINGS_WHEN_GYRO, "Tilting the controller up looks down.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "MOUSE STYLE", AE_SETTINGS_SPINNER, "input.mouse_style",
		VALUES(style_texts, style_values), AE_SETTINGS_WHEN_KEYBOARD,
		"CLASSIC is Halo PC's mouse. MCC uses one sensitivity with scales for zoom and vehicles.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "HORIZONTAL SENSITIVITY", AE_SETTINGS_SLIDER, "input.mouse_sensitivity",
		VALUES(classic_texts, classic_values), AE_SETTINGS_WHEN_MOUSE_CLASSIC,
		"How fast the mouse turns you left and right.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "VERTICAL SENSITIVITY", AE_SETTINGS_SLIDER, "input.mouse_vertical_sensitivity",
		VALUES(vertical_texts, vertical_values), AE_SETTINGS_WHEN_MOUSE_CLASSIC,
		"How fast the mouse turns you up and down. SAME follows the horizontal one.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "MOUSE SENSITIVITY", AE_SETTINGS_SLIDER, "input.mouse_mcc_sensitivity",
		mcc_texts, mcc_texts, MCC_STEPS, AE_SETTINGS_WHEN_MOUSE_MCC,
		"MCC's mouse sensitivity, one value for both axes.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "ZOOMED SENSITIVITY SCALE", AE_SETTINGS_SLIDER, "input.mouse_zoom_scale",
		scale_texts, scale_texts, SCALE_STEPS, AE_SETTINGS_WHEN_MOUSE_MCC,
		"Your sensitivity while zoomed, as a share of the normal one.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "VEHICLE SENSITIVITY SCALE", AE_SETTINGS_SLIDER, "input.mouse_vehicle_scale",
		scale_texts, scale_texts, SCALE_STEPS, AE_SETTINGS_WHEN_MOUSE_MCC,
		"Your sensitivity in vehicles, as a share of the normal one.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "INVERT VERTICAL AXIS", AE_SETTINGS_TOGGLE, "input.invert_mouse", ON_OFF,
		AE_SETTINGS_WHEN_KEYBOARD, "Move the mouse up to look down.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "AIM ASSIST", AE_SETTINGS_TOGGLE, "input.mouse_aim_assist", ON_OFF,
		AE_SETTINGS_WHEN_KEYBOARD,
		"Use the controller's aim help with the mouse where the gametype allows it. AE gametypes don't.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "ZOOM", AE_SETTINGS_SPINNER, "input.mouse_zoom_mode",
		VALUES(zoom_texts, zoom_values), AE_SETTINGS_WHEN_KEYBOARD,
		"The mouse's zoom, apart from the controller's. TOGGLE, HOLD or BOTH.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "WHEEL ZOOM", AE_SETTINGS_TOGGLE, "input.wheel_zoom", ON_OFF,
		AE_SETTINGS_WHEN_KEYBOARD, "The wheel zooms in and out. It won't switch weapons then, unless one can't zoom.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "ZOOM RESET", AE_SETTINGS_SPINNER, "input.mouse_zoom_reset",
		VALUES(reset_texts, reset_values), AE_SETTINGS_WHEN_KEYBOARD,
		"Where the mouse's zoom starts the next time you zoom in.", 0 },
	{ AE_SETTINGS_CONTROLS, "MOUSE", "ZOOM HOLD TIME", AE_SETTINGS_SLIDER, "input.mouse_zoom_hold_time",
		hold_texts, hold_values, HOLD_STEPS, AE_SETTINGS_WHEN_MOUSE_ZOOM_BOTH,
		"With BOTH: how long a click must be to count as a hold.", 0 },
	{ AE_SETTINGS_CONTROLS, "KEYBOARD", "KEY BINDINGS", AE_SETTINGS_SUBSCREEN, "controls.bindings",
		VALUES(keys_texts, keys_values), AE_SETTINGS_WHEN_KEYBOARD,
		"Two keys for every action. {LB} and {RB} change the page there.", 0 },
	{ AE_SETTINGS_CONTROLS, "KEYBOARD", "KEYBOARD PROMPTS", AE_SETTINGS_TOGGLE, "input.keyboard_prompts", ON_OFF,
		AE_SETTINGS_WHEN_KEYBOARD, "Show your keys, not a controller's buttons, in the game's hints.", 0 },
	/* VIEW */
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "FOV", AE_SETTINGS_SLIDER, "display.fov", fov_texts, fov_values, FOV_STEPS,
		AE_SETTINGS_ALWAYS,
		"Your field of view in degrees. STOCK is Halo CE's 70 for 4:3, wider on wider screens.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "VIEWMODEL FOV", AE_SETTINGS_SLIDER, "display.viewmodel_fov", fov_texts,
		fov_values, FOV_STEPS, AE_SETTINGS_ALWAYS, "How wide your weapon is drawn, apart from the world.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "VIEWMODELS", AE_SETTINGS_TOGGLE, "display.viewmodel_visible", ON_OFF,
		AE_SETTINGS_ALWAYS, "Show your weapon and hands.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "VIEWMODEL SWAY", AE_SETTINGS_SPINNER, "display.viewmodel_sway",
		VALUES(sway_texts, sway_values), AE_SETTINGS_ALWAYS,
		"How far your weapon leans as you turn and walk. OFF keeps it locked to the crosshair.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "LEGS", AE_SETTINGS_TOGGLE, "display.first_person_legs", ON_OFF,
		AE_SETTINGS_ALWAYS, "See your legs when you look down.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "ARM SHIELDS", AE_SETTINGS_TOGGLE, "display.viewmodel_shield", ON_OFF,
		AE_SETTINGS_ALWAYS, "Your shield effect shows on your arms when it is hit.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "ZOOM BLUR", AE_SETTINGS_TOGGLE, "display.zoom_blur", ON_OFF,
		AE_SETTINGS_ALWAYS, "The short blur as you zoom in and out.", 0 },
	{ AE_SETTINGS_VIEW, "FIRST PERSON", "PASSENGER THIRD PERSON", AE_SETTINGS_TOGGLE, "display.passenger_third_person",
		ON_OFF, AE_SETTINGS_ALWAYS, "Third person while you ride in a vehicle you don't drive.", 0 },
	{ AE_SETTINGS_VIEW, "WEAPON COSMETICS", "PISTOL COLOR", AE_SETTINGS_SPINNER, "display.pistol_color",
		VALUES(shield_texts, shield_values), AE_SETTINGS_ALWAYS,
		"Cosmetic, only on your screen: your pistol's body in first person. Others see the stock pistol.", 0 },
	{ AE_SETTINGS_VIEW, "WEAPON COSMETICS", "PISTOL TRIM", AE_SETTINGS_SPINNER, "display.pistol_trim",
		VALUES(shield_texts, shield_values), AE_SETTINGS_ALWAYS,
		"Cosmetic, only on your screen: your pistol's dark parts in first person.", 0 },
	{ AE_SETTINGS_VIEW, "PLAYERS", "BACKPACK WEAPONS", AE_SETTINGS_TOGGLE, "display.backpack_weapons", ON_OFF,
		AE_SETTINGS_ALWAYS, "The weapon a player carries but does not hold shows on their back or thigh.", 0 },
	{ AE_SETTINGS_VIEW, "PLAYERS", "RAGDOLLS: PLAYERS", AE_SETTINGS_SPINNER, "game.ragdoll_players",
		VALUES(ragdoll_texts, ragdoll_values), AE_SETTINGS_ALWAYS,
		"How players' and bots' bodies fall. MIXED plays the death animation, then goes limp; NONE is the original.", 0 },
	{ AE_SETTINGS_VIEW, "PLAYERS", "RAGDOLLS: AI", AE_SETTINGS_SPINNER, "game.ragdoll_ai",
		VALUES(ragdoll_texts, ragdoll_values), AE_SETTINGS_ALWAYS,
		"How the campaign's enemies and allies fall. Only this PC sees it.", 0 },
	{ AE_SETTINGS_VIEW, "SHIELDS", "SHIELD COLOR", AE_SETTINGS_SPINNER, "display.shield_color",
		VALUES(shield_texts, shield_values), AE_SETTINGS_ALWAYS, "The colour your shield flashes when it is hit.", 0 },
	{ AE_SETTINGS_VIEW, "SHIELDS", "SHIELD GLOW", AE_SETTINGS_TOGGLE, "display.shield_glow", ON_OFF,
		AE_SETTINGS_ALWAYS, "Shields glow as they recharge. Online, the host's choice.", 0 },
	{ AE_SETTINGS_VIEW, "SHIELDS", "GLOW INTENSITY", AE_SETTINGS_SPINNER, "display.shield_glow_intensity",
		VALUES(intensity_texts, intensity_values), AE_SETTINGS_WHEN_GLOW,
		"How bright the shield glow is. Online, the host's choice.", 0 },
	{ AE_SETTINGS_VIEW, "SHIELDS", "OVERSHIELD", AE_SETTINGS_TOGGLE, "display.overshield_effect", ON_OFF,
		AE_SETTINGS_ALWAYS, "The overshield's own look on players who carry one. Online, the host's choice.", 0 },
	/* HUD */
	{ AE_SETTINGS_HUD, "HUD", "HUD STYLE", AE_SETTINGS_SPINNER, "display.hud_style",
		VALUES(hud_style_texts, hud_style_values), AE_SETTINGS_ALWAYS,
		"AE's HUD or Halo CE's. Some rows below change with it.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "HUD AREA", AE_SETTINGS_SPINNER, "display.hud_area", VALUES(area_texts, area_values),
		AE_SETTINGS_ALWAYS, "Keep the HUD inside 16:9 or 4:3 on a wide screen.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "COMPACT HUD", AE_SETTINGS_TOGGLE, "display.compact_hud", ON_OFF, AE_SETTINGS_ALWAYS,
		"A smaller HUD.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "MULTIPLAYER GAME TIMER", AE_SETTINGS_SPINNER, "display.match_clock",
		VALUES(clock_texts, clock_values), AE_SETTINGS_ALWAYS, "The match clock on the score strip.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "CLOCK LABELS", AE_SETTINGS_TOGGLE, "display.match_clock_labels", ON_OFF,
		AE_SETTINGS_WHEN_CLOCK, "Words under the clock: LEFT, PLAYED.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "CAMPAIGN TIMER", AE_SETTINGS_TOGGLE, "display.campaign_timer", ON_OFF,
		AE_SETTINGS_ALWAYS, "A level timer in the campaign, for speed runs.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "POWERUP TIMERS", AE_SETTINGS_TOGGLE, "display.powerup_timers", ON_OFF,
		AE_SETTINGS_ALWAYS, "How long your own camo or overshield lasts.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "SHORT MESSAGES", AE_SETTINGS_TOGGLE, "display.short_messages", ON_OFF,
		AE_SETTINGS_ALWAYS, "Kill and pickup messages in fewer words.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "MEDALS", AE_SETTINGS_TOGGLE, "display.medals", ON_OFF,
		AE_SETTINGS_ALWAYS, "Medals you earn shown in your view.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "MEDAL STYLE", AE_SETTINGS_SPINNER, "display.medal_style",
		VALUES(medal_style_texts, medal_style_values), AE_SETTINGS_WHEN_MEDALS,
		"UNIVERSAL: medals by your reticle, with a Killed line. MCC: larger medals on the left.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "KILL FEED", AE_SETTINGS_TOGGLE, "display.kill_feed", ON_OFF,
		AE_SETTINGS_ALWAYS, "Everyone's kills, listed on your HUD, in place of the game's kill messages and pickup text.", 0 },
	{ AE_SETTINGS_HUD, "HUD", "BUTTON STYLE", AE_SETTINGS_SPINNER, "display.hud_button_style",
		VALUES(button_texts, button_values), AE_SETTINGS_WHEN_NOT_AE_HUD, "How button pictures look in the HUD's hints.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "CROSSHAIR", AE_SETTINGS_SPINNER, "xh.main", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"Your own crosshair, drawn in place of every weapon's reticle: pick one you saved, or STOCK (the weapon's own reticle). {A} opens the "
		"editor. In a game whose host turned CUSTOM CROSSHAIRS off, each weapon's own reticle shows.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "IN SCOPE", AE_SETTINGS_SPINNER, "display.crosshair_in_scope",
		VALUES(xh_scope_texts, xh_scope_values), AE_SETTINGS_ALWAYS,
		"Zoomed in: SAME CROSSHAIR keeps your crosshair, at the zoom's size. WEAPON'S SCOPE shows the weapon's own scope "
		"reticle until the zoom ends.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "EDIT CROSSHAIR...", AE_SETTINGS_SUBSCREEN, "action.crosshair_edit", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS, "Draw a crosshair: {A} opens the editor.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "WEAPONS...", AE_SETTINGS_SUBSCREEN, "action.crosshair_weapons", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS, "A different crosshair for a weapon or a vehicle gun. {A} opens the list.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "ALLY RETICLE", AE_SETTINGS_TOGGLE, "display.ally_reticle", ON_OFF,
		AE_SETTINGS_ALWAYS, "Your reticle turns green over an ally.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "ENEMY COLOR", AE_SETTINGS_SPINNER, "display.reticle_enemy_color",
		VALUES(xh_state_texts, xh_state_values), AE_SETTINGS_ALWAYS,
		"The colour your crosshair and a weapon's own reticle take over an enemy in range.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "ALLY COLOR", AE_SETTINGS_SPINNER, "display.reticle_ally_color",
		VALUES(xh_state_texts, xh_state_values), AE_SETTINGS_ALWAYS,
		"The colour over an ally, with ALLY RETICLE on.", 0 },
	{ AE_SETTINGS_HUD, "RETICLE", "MARK PROMPT", AE_SETTINGS_TOGGLE, "display.mark_prompt", ON_OFF,
		AE_SETTINGS_ALWAYS, "The button and word over what a mark would act on.", 0 },
	{ AE_SETTINGS_HUD, "STOCK", "RETICLE SIZE", AE_SETTINGS_SPINNER, "display.reticle_size",
		VALUES(xh_reticle_texts, xh_reticle_values), AE_SETTINGS_ALWAYS,
		"How big a weapon's own reticle is. A crosshair you drew has its own size.", 0 },
	{ AE_SETTINGS_HUD, "STOCK", "RETICLE SCALING", AE_SETTINGS_SPINNER, "display.reticle_scaling",
		VALUES(xh_scaling_texts, xh_scaling_values), AE_SETTINGS_ALWAYS,
		"FOLLOWS FOV: a weapon's own reticle covers the same part of the world as your field of view changes. FIXED: it keeps its size.", 0 },
	{ AE_SETTINGS_HUD, "STOCK", "FOV BASE", AE_SETTINGS_SPINNER, "display.reticle_fov_base",
		VALUES(xh_fov_base_texts, xh_fov_base_values), AE_SETTINGS_ALWAYS,
		"FOLLOWS FOV draws a weapon's own reticle at its stock size at this field of view (degrees across a 4:3 view) and "
		"smaller as your view widens. 78: a little larger than stock at the game's own 70.", 0 },
	{ AE_SETTINGS_HUD, "STOCK", "RETICLE COLOR", AE_SETTINGS_SPINNER, "display.reticle_color",
		VALUES(xh_tint_texts, xh_tint_values), AE_SETTINGS_ALWAYS,
		"The colour of a weapon's own reticle. Red over an enemy and green over an ally stay.", 0 },
	{ AE_SETTINGS_HUD, "STOCK", "RETICLE MOTION", AE_SETTINGS_SPINNER, "display.reticle_motion",
		VALUES(xh_motion_texts, xh_motion_values), AE_SETTINGS_ALWAYS,
		"RIFLES: the AE rifles' reticles grow a little a shot. ALL WEAPONS: every weapon's reticle grows with its real "
		"spread and a shot's kick, as a crosshair you drew moves. OFF: still.", 0 },
	{ AE_SETTINGS_HUD, "NAMES", "PLAYER NAMES", AE_SETTINGS_SPINNER, "display.player_names",
		VALUES(names_texts, names_values), AE_SETTINGS_ALWAYS, "Whose names show over their heads.", 0 },
	{ AE_SETTINGS_HUD, "NAMES", "PLAYER NAME SIZE", AE_SETTINGS_SLIDER, "display.player_name_scale",
		VALUES(name_size_texts, name_size_values), AE_SETTINGS_ALWAYS, "How big the names over heads are.", 0 },
	{ AE_SETTINGS_HUD, "NAMES", "ENEMY NAME COLOR", AE_SETTINGS_SPINNER, "display.enemy_name_color",
		VALUES(enemy_texts, enemy_values), AE_SETTINGS_ALWAYS,
		"RED shows enemies' names in red. HUD STYLE AE always does.", 0 },
	{ AE_SETTINGS_HUD, "NAMES", "DEAD ALLY MARKERS", AE_SETTINGS_TOGGLE, "display.dead_ally_markers", ON_OFF,
		AE_SETTINGS_ALWAYS, "Team games: a cross where a teammate fell, until they respawn.", 0 },
	{ AE_SETTINGS_HUD, "SCOREBOARD", "SCOREBOARD LAYOUT", AE_SETTINGS_SPINNER, "display.scoreboard_team_layout",
		VALUES(layout_board_texts, layout_board_values), AE_SETTINGS_ALWAYS,
		"Team games: players under their team, or everyone by score.", 0 },
	{ AE_SETTINGS_HUD, "SCOREBOARD", "SCOREBOARD PANEL", AE_SETTINGS_TOGGLE, "display.scoreboard_background", ON_OFF,
		AE_SETTINGS_ALWAYS, "A dark panel behind the scoreboard.", 0 },
	{ AE_SETTINGS_HUD, "SCOREBOARD", "SCOREBOARD FADE", AE_SETTINGS_SPINNER, "display.scoreboard_fade",
		VALUES(fade_texts, fade_values), AE_SETTINGS_ALWAYS, "How fast the scoreboard comes and goes.", 0 },
	{ AE_SETTINGS_HUD, "SCOREBOARD", "SHOW PLAYERS WHO QUIT", AE_SETTINGS_TOGGLE, "display.show_quit_players", ON_OFF,
		AE_SETTINGS_ALWAYS, "Players who left stay on the scoreboard, greyed.", 0 },
	{ AE_SETTINGS_HUD, "SCOREBOARD", "PANEL COLOR", AE_SETTINGS_SPINNER, "display.scoreboard_background_color",
		VALUES(panel_texts, panel_values), AE_SETTINGS_WHEN_PANEL, "The scoreboard panel's colour.", 0 },
	{ AE_SETTINGS_HUD, "TRAINING", "WAYPOINTS", AE_SETTINGS_SPINNER, "display.waypoint_style",
		VALUES(waypoint_texts, waypoint_values), AE_SETTINGS_ALWAYS,
		"How item waypoints show when the gametype's TIMERS or TRAINING is on.", 0 },
	{ AE_SETTINGS_HUD, "TRAINING", "WAYPOINT SIZE", AE_SETTINGS_SLIDER, "display.waypoint_size",
		waypoint_size_texts, waypoint_size_values, WAYPOINT_STEPS, AE_SETTINGS_ALWAYS, "How big the waypoints are.", 0 },
	{ AE_SETTINGS_HUD, "TRAINING", "POWER LIST", AE_SETTINGS_SPINNER, "display.power_list",
		VALUES(power_texts, power_values), AE_SETTINGS_ALWAYS,
		"Where the power items' next spawns are listed: over the clock, or top left.", 0 },
	{ AE_SETTINGS_HUD, "TRAINING", "SHOTGUN AS POWER", AE_SETTINGS_TOGGLE, "display.shotgun_power", ON_OFF,
		AE_SETTINGS_ALWAYS, "Count the shotgun as a power weapon in the timers.", 0 },
	{ AE_SETTINGS_HUD, "TRAINING", "SPAWN HEAT", AE_SETTINGS_SPINNER, "display.spawn_heat",
		VALUES(heat_texts, heat_values), AE_SETTINGS_ALWAYS,
		"Colours where players will spawn, when the gametype allows it.", 0 },
	/* (0.7.6) OVERLAYS: the frame rate line, the ping on it and the recording's dot, together; this PC's (a THIS PC badge
	with two players or more) */
	{ AE_SETTINGS_HUD, "OVERLAYS", "PERFORMANCE", AE_SETTINGS_SPINNER, "display.performance",
		VALUES(performance_texts, performance_values), AE_SETTINGS_ALWAYS,
		"Shows the frame rate, and with FULL the frame time and draw count.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_HUD, "OVERLAYS", "POSITION", AE_SETTINGS_SPINNER, "display.performance_position",
		VALUES(corner_texts, corner_values), AE_SETTINGS_WHEN_OVERLAY_LINE, "Where the overlay line sits.",
		AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_HUD, "OVERLAYS", "PING", AE_SETTINGS_SPINNER, "display.ping",
		VALUES(ping_texts, ping_values), AE_SETTINGS_ALWAYS,
		"Your ping to the host in a network game (the host: its slowest player's), on the overlay line at POSITION. "
		"With PERFORMANCE OFF it is the line alone. NUMBER AND BARS adds four bars. Never in the menus.",
		AE_SETTINGS_THIS_PC },
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	{ AE_SETTINGS_HUD, "OVERLAYS", "RECORDING DOT", AE_SETTINGS_TOGGLE, "capture.record_indicator", ON_OFF,
		AE_SETTINGS_ALWAYS, "A red dot at the top right while recording. It is never in the recording.",
		AE_SETTINGS_THIS_PC },
#endif
	/* VIDEO (this PC's; the classic menus' Video Setup writes the same keys) */
	{ AE_SETTINGS_VIDEO, "DISPLAY", "DISPLAY MODE", AE_SETTINGS_SPINNER, "display.mode",
		VALUES(display_mode_texts, display_mode_values), AE_SETTINGS_ALWAYS,
		"How the game fills your screen. A change asks to keep it, else it goes back in 10 seconds.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "RESOLUTION", AE_SETTINGS_SPINNER, "display.resolution", NULL, NULL, 0,
		AE_SETTINGS_WHEN_NOT_WINDOWED,
		"The size the game draws at. A change asks to keep it, else it goes back in 10 seconds.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "WINDOW SIZE", AE_SETTINGS_SPINNER, "display.window_size", NULL, NULL, 0,
		AE_SETTINGS_WHEN_WINDOWED, "The window's size.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "RESOLUTION SCALING", AE_SETTINGS_SPINNER, "display.resolution_scaling",
		VALUES(scaling_texts, scaling_values), AE_SETTINGS_ALWAYS,
		"ORIGINAL draws at Halo PC's size and stretches it, for an older look or a slow PC.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "V-SYNC", AE_SETTINGS_TOGGLE, "display.vsync", ON_OFF, AE_SETTINGS_ALWAYS,
		"Waits for the screen: no tearing, a little more delay.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "FRAME RATE LIMIT", AE_SETTINGS_SPINNER, "display.max_fps",
		VALUES(fps_texts, fps_values), AE_SETTINGS_ALWAYS,
		"With V-SYNC off, the most frames a second. AUTO is twice your screen's refresh rate.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "SMOOTH MOTION", AE_SETTINGS_TOGGLE, "display.interpolation", ON_OFF,
		AE_SETTINGS_ALWAYS, "Draws between the game's 30 ticks a second, so motion is smooth at high frame rates.", 0 },
	{ AE_SETTINGS_VIDEO, "DISPLAY", "INSTANT AIM", AE_SETTINGS_TOGGLE, "display.direct_camera", ON_OFF,
		AE_SETTINGS_ALWAYS, "Your view turns the frame you move, not on the next tick. Less delay when you aim.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "ANTI-ALIASING", AE_SETTINGS_SPINNER, "display.anti_aliasing",
		VALUES(aa_texts, aa_values), AE_SETTINGS_ALWAYS,
		"Smooths jagged edges. SSAA looks best and costs the most.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "ANISOTROPIC FILTERING", AE_SETTINGS_SPINNER, "display.anisotropic",
		VALUES(aniso_texts, aniso_values), AE_SETTINGS_ALWAYS, "Keeps floors and walls sharp at an angle.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "SHADOW RESOLUTION", AE_SETTINGS_SPINNER, "display.shadow_resolution",
		shadow_texts, shadow_texts, COUNT(shadow_texts), AE_SETTINGS_ALWAYS,
		"Sharper shadows under players and vehicles.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "PER-PIXEL LIGHTING", AE_SETTINGS_TOGGLE, "display.per_pixel_lighting", ON_OFF,
		AE_SETTINGS_ALWAYS, "Lighting worked out for each pixel: smoother highlights.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "MODEL DETAIL", AE_SETTINGS_SPINNER, "display.model_detail",
		VALUES(model_texts, model_values), AE_SETTINGS_ALWAYS,
		"Which level of detail far models use.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "DRAW DISTANCE", AE_SETTINGS_SPINNER, "display.far_clip_distance",
		distance_texts, distance_texts, COUNT(distance_texts), AE_SETTINGS_ALWAYS,
		"How far the game draws. Bigger maps can need more.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "HIGH-RES HUD", AE_SETTINGS_TOGGLE, "display.high_res_hud", ON_OFF,
		AE_SETTINGS_ALWAYS, "The HUD in sharp, high resolution art.", 0 },
	{ AE_SETTINGS_VIDEO, "GRAPHICS", "HIGH-RES TEXT", AE_SETTINGS_TOGGLE, "display.high_res_text", ON_OFF,
		AE_SETTINGS_ALWAYS, "The game's text drawn sharp at any size.", 0 },
	{ AE_SETTINGS_VIDEO, "PERFORMANCE", "VERSION WATERMARK", AE_SETTINGS_TOGGLE, "display.version_watermark", ON_OFF,
		AE_SETTINGS_ALWAYS, "The version in a corner, for recordings and bug reports.", 0 },
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	/* (0.7.5) the limits are frame rate tools: they sit with the frame rate counter */
	{ AE_SETTINGS_VIDEO, "PERFORMANCE", "PARTICLE LIMIT", AE_SETTINGS_SPINNER, "display.particle_limit",
		VALUES(particle_texts, particle_values), AE_SETTINGS_ALWAYS,
		"The most sparks, smoke and debris alive at once. Fewer is easier on the PC in a big firefight.", 0 },
	{ AE_SETTINGS_VIDEO, "PERFORMANCE", "DYNAMIC LIGHT LIMIT", AE_SETTINGS_SPINNER, "display.dynamic_light_limit",
		VALUES(light_texts, light_values), AE_SETTINGS_ALWAYS,
		"The most muzzle flashes, plasma and explosions that light the level at once. Fewer is easier on the GPU.", 0 },
#endif
	{ AE_SETTINGS_VIDEO, "ADVANCED", "HDR", AE_SETTINGS_TOGGLE, "display.hdr", ON_OFF, AE_SETTINGS_ALWAYS,
		"Brighter lights and deeper darks.", 0 },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "HDR DISPLAY", AE_SETTINGS_TOGGLE, "display.hdr_output", ON_OFF,
		AE_SETTINGS_WHEN_HDR_DISPLAY, "With HDR, send real HDR to an HDR screen. The game restarts to change it.",
		AE_SETTINGS_RESTART },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "HDR BRIGHTNESS", AE_SETTINGS_SLIDER, "display.hdr_exposure",
		VALUES(exposure_texts, exposure_values), AE_SETTINGS_WHEN_HDR, "The HDR picture's brightness.", 0 },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "TONE MAPPING", AE_SETTINGS_SPINNER, "display.hdr_tone_mapping",
		VALUES(tone_texts, tone_values), AE_SETTINGS_WHEN_HDR,
		"How HDR's bright light is fitted to your screen.", 0 },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "SHOW COORDINATES", AE_SETTINGS_TOGGLE, "display.coordinates", ON_OFF,
		AE_SETTINGS_ALWAYS, "Your position on the map, for map makers and layouts.", 0 },
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	{ AE_SETTINGS_VIDEO, "ADVANCED", "REVERSED DEPTH", AE_SETTINGS_TOGGLE, "display.reversed_depth", ON_OFF,
		AE_SETTINGS_ALWAYS, "Precise depth far away: surfaces close together stop flickering at a distance. The game restarts to change it.",
		AE_SETTINGS_RESTART },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "SHADER CACHE", AE_SETTINGS_TOGGLE, "display.shader_cache", ON_OFF,
		AE_SETTINGS_ALWAYS, "Keeps the graphics programs for next time, so fewer stalls as new ones are needed. OFF builds them again each run.", 0 },
	{ AE_SETTINGS_VIDEO, "ADVANCED", "STRUCTURE CULLING", AE_SETTINGS_SPINNER, "display.structure_culling",
		VALUES(culling_texts, culling_values), AE_SETTINGS_ALWAYS,
		"Skips the level the camera cannot see. OFF draws all of it, as the console did. CUSTOM is a mix set in config.toml.", 0 },
#endif
	/* AUDIO (this PC's; the classic menus' Audio Setup writes the same keys) */
	{ AE_SETTINGS_AUDIO, "VOLUME", "SOUND", AE_SETTINGS_TOGGLE, "audio.enabled", ON_OFF, AE_SETTINGS_ALWAYS,
		"Play any sound at all. OFF is silent; the game restarts to change it.", AE_SETTINGS_RESTART },
	{ AE_SETTINGS_AUDIO, "VOLUME", "MASTER VOLUME", AE_SETTINGS_SLIDER, "audio.volume", volume_texts, volume_values,
		VOLUME_STEPS, AE_SETTINGS_ALWAYS, "Everything at once.", 0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "MUSIC VOLUME", AE_SETTINGS_SLIDER, "audio.music_volume", volume_texts, volume_values,
		VOLUME_STEPS, AE_SETTINGS_ALWAYS, "The music in menus and the campaign.", 0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "MENU MUSIC", AE_SETTINGS_SPINNER, "audio.menu_music",
		VALUES(menu_music_texts, menu_music_values), AE_SETTINGS_ALWAYS,
		"The menus' music. MCC plays the music of your MCC installation, shuffled or in order; CLASSIC is the title theme.",
		0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "EFFECTS VOLUME", AE_SETTINGS_SLIDER, "audio.effects_volume", volume_texts,
		volume_values, VOLUME_STEPS, AE_SETTINGS_ALWAYS, "Weapons, vehicles, voices in the game.", 0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "MENU SOUNDS", AE_SETTINGS_SLIDER, "audio.arena_menus_volume", volume_texts,
		volume_values, VOLUME_STEPS, AE_SETTINGS_ALWAYS, "The AE menus' clicks and moves.", 0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "OUTPUT DEVICE", AE_SETTINGS_SPINNER, "audio.output_device", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS, "Where the game's sound plays. {A} lists every output.", 0 },
	{ AE_SETTINGS_AUDIO, "VOLUME", "SPEAKERS", AE_SETTINGS_SPINNER, "audio.channels",
		VALUES(speakers_texts, speakers_values), AE_SETTINGS_ALWAYS,
		"STEREO for headphones and TVs, 5.1 for surround speakers. The game restarts to change it.",
		AE_SETTINGS_RESTART },
	{ AE_SETTINGS_AUDIO, "VOICE", "VOICE CHAT", AE_SETTINGS_SPINNER, "audio.voice_chat",
		VALUES(voice_chat_texts, voice_chat_values), AE_SETTINGS_ALWAYS,
		"How your microphone sends in network games. The host can turn voice off.", 0 },
	{ AE_SETTINGS_AUDIO, "VOICE", "INPUT DEVICE", AE_SETTINGS_SPINNER, "audio.input_device", NULL, NULL, 0,
		AE_SETTINGS_WHEN_VOICE, "The microphone voice chat uses.", 0 },
	{ AE_SETTINGS_AUDIO, "VOICE", "VOICE VOLUME", AE_SETTINGS_SLIDER, "audio.voice_volume", volume_texts, volume_values,
		VOLUME_STEPS, AE_SETTINGS_ALWAYS, "Other players' voices.", 0 },
	{ AE_SETTINGS_AUDIO, "VOICE", "PUSH TO TALK", AE_SETTINGS_KEY, "controls.push_to_talk", NULL, NULL, 0,
		AE_SETTINGS_WHEN_PUSH_TO_TALK, "The key you hold to talk. {A} changes it in KEY BINDINGS.", 0 },
	/* (CALLOUTS: sound, this PC's; on HUD before 0.3.9, owner 2026-10-23) */
	{ AE_SETTINGS_AUDIO, "CALLOUTS", "CALLOUTS", AE_SETTINGS_SPINNER, "game.callouts",
		VALUES(callout_texts, callout_values), AE_SETTINGS_ALWAYS, "A voice calls power items as they come up.", 0 },
	{ AE_SETTINGS_AUDIO, "CALLOUTS", "CALLOUT DETAIL", AE_SETTINGS_SPINNER, "game.callout_detail",
		VALUES(detail_texts, detail_values), AE_SETTINGS_WHEN_CALLOUTS, "How much each callout says.", 0 },
	{ AE_SETTINGS_AUDIO, "CALLOUTS", "CALLOUT VOICE", AE_SETTINGS_SPINNER, "game.callout_voice", NULL, NULL, 0,
		AE_SETTINGS_WHEN_CALLOUTS, "Whose voice calls them.", 0 },
	{ AE_SETTINGS_AUDIO, "CALLOUTS", "ANNOUNCER", AE_SETTINGS_SPINNER, "game.announcer", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS,
		"Who announces kills, sprees, modes and objectives. MCC ones come from your MCC installation; CLASSIC is the game's own.",
		0 },
	{ AE_SETTINGS_AUDIO, "ADVANCED", "REVERB", AE_SETTINGS_TOGGLE, "audio.reverb", ON_OFF, AE_SETTINGS_ALWAYS,
		"Echo in rooms and caves, as Halo CE has it.", 0 },
	{ AE_SETTINGS_AUDIO, "ADVANCED", "NHE SOUND FILTER", AE_SETTINGS_TOGGLE, "audio.nhe_sound_filter", ON_OFF,
		AE_SETTINGS_ALWAYS,
		"Mutes the plasma pistol's charging hum and the needler's shots on any map, as NHE's maps did.", 0 },
	{ AE_SETTINGS_AUDIO, "ADVANCED", "RESAMPLER", AE_SETTINGS_SPINNER, "audio.resampler",
		VALUES(resampler_texts, resampler_values), AE_SETTINGS_ALWAYS,
		"SINC is cleaner, LINEAR is lighter on a slow PC.", 0 },
	/* ONLINE (this PC's; the classic menus' Network Setup and its MAP SHARING write the same keys) */
	{ AE_SETTINGS_ONLINE, "CONNECTION", "INTERNET PLAY", AE_SETTINGS_TOGGLE, "network.online", ON_OFF,
		AE_SETTINGS_ALWAYS, "Join and host games over the internet. OFF keeps you to LAN and this PC.", 0 },
	{ AE_SETTINGS_ONLINE, "CONNECTION", "COMMUNITY SERVICES", AE_SETTINGS_TOGGLE, "network.community_services",
		on_off_texts, yes_no_values, COUNT(on_off_texts), AE_SETTINGS_ALWAYS,
		"The server browser and invites, through halo.milenko.org. OFF: nothing goes there.", 0 },
	{ AE_SETTINGS_ONLINE, "CONNECTION", "UPNP PORT FORWARDING", AE_SETTINGS_TOGGLE, "network.allow_upnp", ON_OFF,
		AE_SETTINGS_ALWAYS, "Opens the game's port on your router so players can join you.", 0 },
	{ AE_SETTINGS_ONLINE, "CONNECTION", "JOIN FROM CLIPBOARD", AE_SETTINGS_SPINNER, "network.join_from_clipboard",
		VALUES(ask_texts, ask_values), AE_SETTINGS_ALWAYS,
		"Join an invite link you copied, when you switch to the game.", 0 },
	{ AE_SETTINGS_ONLINE, "CONNECTION", "SERVER PINGS", AE_SETTINGS_TOGGLE, "network.browser_pings", ON_OFF,
		AE_SETTINGS_ALWAYS, "Ping every game in the server browser to show its delay. Each host then sees your address.",
		0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "DOWNLOAD MAPS", AE_SETTINGS_TOGGLE, "network.map_download", ON_OFF,
		AE_SETTINGS_ALWAYS, "Get a map you don't have from the host when you join.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "INTERNET DOWNLOADS", AE_SETTINGS_TOGGLE, "network.map_download_internet", ON_OFF,
		AE_SETTINGS_WHEN_MAP_DOWNLOAD, "Also from hosts over the internet, not only on your LAN.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "INTERNET SHARING", AE_SETTINGS_TOGGLE, "network.map_share_internet", ON_OFF,
		AE_SETTINGS_ALWAYS, "Hosting: also send your maps to players who are not on your local network.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "MAP DOWNLOADS", AE_SETTINGS_SPINNER, "network.map_downloads",
		VALUES(ask_texts, ask_values), AE_SETTINGS_ALWAYS,
		"Maps from the community map sites, for games the browser lists.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "HELP SEND MAPS", AE_SETTINGS_TOGGLE, "network.map_seed", ON_OFF, AE_SETTINGS_ALWAYS,
		"Pass maps you have on to other joining players.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "UPLOAD LIMIT", AE_SETTINGS_SPINNER, "network.map_seed_upload",
		VALUES(upload_texts, upload_values), AE_SETTINGS_WHEN_MAP_SEED, "The most this PC sends while helping.", 0 },
	{ AE_SETTINGS_ONLINE, "MAPS", "DOWNLOAD LIMIT", AE_SETTINGS_SPINNER, "network.map_download_limit",
		VALUES(download_texts, download_values), AE_SETTINGS_ALWAYS,
		"The most map data this PC takes from one host.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "LIST HOSTED GAMES", AE_SETTINGS_TOGGLE, "network.list_hosted_games", ON_OFF,
		AE_SETTINGS_ALWAYS, "Games you host show in the server browser.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "HOSTED GAME STATS", AE_SETTINGS_TOGGLE, "network.report_hosted_games", ON_OFF,
		AE_SETTINGS_ALWAYS, "Send your hosted games' scores to the stats site.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "SHARE HOSTED STATS", AE_SETTINGS_TOGGLE, "network.report_events", ON_OFF,
		AE_SETTINGS_ALWAYS, "Also each kill and pickup in your hosted games.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "SHARE GAME STATS", AE_SETTINGS_TOGGLE, "network.report_joined_games", ON_OFF,
		AE_SETTINGS_ALWAYS, "Send the scores of games you join.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "DISCORD STATUS", AE_SETTINGS_SPINNER, "discord.presence",
		VALUES(discord_texts, discord_values), AE_SETTINGS_ALWAYS, "Show what you play on your Discord profile.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "DISCORD GAME NAME", AE_SETTINGS_TOGGLE, "discord.show_server_name", ON_OFF,
		AE_SETTINGS_WHEN_DISCORD, "Include the game's name in your status.", 0 },
	{ AE_SETTINGS_ONLINE, "PRIVACY", "DISCORD JOIN BUTTON", AE_SETTINGS_TOGGLE, "discord.show_join", ON_OFF,
		AE_SETTINGS_WHEN_DISCORD, "Friends can join you from Discord.", 0 },
	{ AE_SETTINGS_ONLINE, "HOSTING", "SHIELD EFFECTS", AE_SETTINGS_SPINNER, "network.shield_effects",
		VALUES(shield_effects_texts, shield_effects_values), AE_SETTINGS_ALWAYS,
		"What everyone sees in games you host; PLAYERS' OWN: each their own settings.", 0 },
	{ AE_SETTINGS_ONLINE, "HOSTING", "MARKS", AE_SETTINGS_TOGGLE, "network.marks", ON_OFF, AE_SETTINGS_ALWAYS,
		"Players can mark and call on the wheel in games you host.", 0 },
	{ AE_SETTINGS_ONLINE, "HOSTING", "BANNED PLAYERS", AE_SETTINGS_SUBSCREEN, "action.banned_players", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS, "Players you banned from games you host. {A} opens the list; unban one there.", 0 },
	{ AE_SETTINGS_ONLINE, "ADVANCED", "PROTOCOL", AE_SETTINGS_SPINNER, "network.protocol",
		VALUES(protocol_texts, protocol_values), AE_SETTINGS_ALWAYS,
		"How this PC talks to other machines. AUTO picks the best both have.", 0 },
	/* ACCESSIBILITY (the menus' rows this PC's, the rest the player's own; the two links are HUD's rows) */
	{ AE_SETTINGS_ACCESSIBILITY, "MENUS", "MENU SIZE", AE_SETTINGS_SPINNER, "display.arena_menus_scale",
		VALUES(menu_size_texts, menu_size_values), AE_SETTINGS_ALWAYS, "The size of the AE menus' text and rows.",
		AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_ACCESSIBILITY, "MENUS", "REDUCE MOTION", AE_SETTINGS_TOGGLE, "display.arena_menus_reduce_motion",
		ON_OFF, AE_SETTINGS_ALWAYS, "Menus cut between screens instead of sliding.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_ACCESSIBILITY, "MENUS", "LANGUAGE", AE_SETTINGS_SPINNER, "game.language",
		VALUES(language_texts, language_values), AE_SETTINGS_ALWAYS,
		"The game's text and voices, when the maps hold that language. The game restarts to change it.",
		AE_SETTINGS_RESTART | AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_ACCESSIBILITY, "IN THE GAME", "IN-GAME HELP", AE_SETTINGS_TOGGLE, "profile.ingame_help", ON_OFF,
		AE_SETTINGS_ALWAYS, "The campaign's hints about buttons.", 0 },
	{ AE_SETTINGS_ACCESSIBILITY, "IN THE GAME", "PLAYER NAME SIZE", AE_SETTINGS_LINK, "display.player_name_scale",
		VALUES(name_size_texts, name_size_values), AE_SETTINGS_ALWAYS, "The same setting as in HUD. {A} goes there.", 0 },
	{ AE_SETTINGS_ACCESSIBILITY, "IN THE GAME", "WAYPOINT SIZE", AE_SETTINGS_LINK, "display.waypoint_size",
		waypoint_size_texts, waypoint_size_values, WAYPOINT_STEPS, AE_SETTINGS_ALWAYS,
		"The same setting as in HUD. {A} goes there.", 0 },
	{ AE_SETTINGS_VIDEO, "LOADING SCREEN", "CAMPAIGN", AE_SETTINGS_SPINNER, "display.loading_screen_campaign_seconds",
		VALUES(campaign_loading_texts, campaign_loading_values), AE_SETTINGS_ALWAYS,
		"The least time a campaign level's loading screen stays up, as the Xbox's disc took.", AE_SETTINGS_THIS_PC },
	{ AE_SETTINGS_VIDEO, "LOADING SCREEN", "MULTIPLAYER", AE_SETTINGS_SPINNER,
		"display.loading_screen_multiplayer_seconds", VALUES(multiplayer_loading_texts, multiplayer_loading_values),
		AE_SETTINGS_ALWAYS, "The same for every other game. The network waits for it.", AE_SETTINGS_THIS_PC },
	/* CHAT AND STREAMING (this PC's: the text chat's two are config.toml's, one for every player here, as the classic
	MULTIPLAYER HUD and STREAMER MODE screens write them) */
	{ AE_SETTINGS_CHAT, "CHAT", "TEXT CHAT", AE_SETTINGS_TOGGLE, "display.text_chat", show_texts, on_off_values,
		COUNT(show_texts), AE_SETTINGS_ALWAYS, "Show other players' chat. HIDE shows none, and its keys open nothing.",
		0 },
	{ AE_SETTINGS_CHAT, "CHAT", "CHAT TEXT", AE_SETTINGS_SPINNER, "display.chat_style",
		VALUES(chat_style_texts, chat_style_values), AE_SETTINGS_ALWAYS,
		"Chat in team and player colours, or all white.", 0 },
	{ AE_SETTINGS_CHAT, "STREAMER MODE", "STREAMER MODE", AE_SETTINGS_TOGGLE, "streamer.enabled", ON_OFF,
		AE_SETTINGS_ALWAYS, "Hides chosen words in names and chat on this PC.", 0 },
	{ AE_SETTINGS_CHAT, "STREAMER MODE", "CENSOR SLURS", AE_SETTINGS_SPINNER, "streamer.slurs",
		VALUES(censor_texts, censor_values), AE_SETTINGS_WHEN_STREAMER,
		"Slurs about race, sexuality, gender and disability.", 0 },
	{ AE_SETTINGS_CHAT, "STREAMER MODE", "CENSOR SWEARING", AE_SETTINGS_SPINNER, "streamer.swearing",
		VALUES(censor_texts, censor_values), AE_SETTINGS_WHEN_STREAMER,
		"Swearing and sexual words. STRONG AND WORSE leaves the mildest showing.", 0 },
	{ AE_SETTINGS_CHAT, "STREAMER MODE", "CENSOR INSULTS", AE_SETTINGS_SPINNER, "streamer.insults",
		VALUES(censor_texts, censor_values), AE_SETTINGS_WHEN_STREAMER, "Name-calling and insults that are not slurs.",
		0 },
	{ AE_SETTINGS_SYSTEM, "MENUS", "MENUS", AE_SETTINGS_SPINNER, "display.arena_menus",
		VALUES(menus_texts, menus_values), AE_SETTINGS_ALWAYS,
		"Arena Evolved's menus, or the classic ones (co-op is there for now). The game restarts to switch.",
		AE_SETTINGS_RESTART },
	{ AE_SETTINGS_SYSTEM, "FILES", "OPEN GAMETYPES FOLDER", AE_SETTINGS_SUBSCREEN, "action.gametypes_folder", NULL,
		NULL, 0, AE_SETTINGS_ALWAYS,
		"Your own game types as files. Copy one to a server's admin/gametypes, or add one: it is listed next start.", 0 },
	{ AE_SETTINGS_SYSTEM, "FILES", "WEAPON PACKS", AE_SETTINGS_TOGGLE, "game.weapon_packs", ON_OFF, AE_SETTINGS_ALWAYS,
		"Read the packs folder's weapons (battle rifle, EVO RIFLE, DMR and more). OFF plays stock weapons and cannot join a pack game.", 0 },
	{ AE_SETTINGS_SYSTEM, "FILES", "MODDED DISC", AE_SETTINGS_SPINNER, "game.disc", NULL, NULL, 0,
		AE_SETTINGS_ALWAYS, "A modded disc's maps replace the stock maps it has. The game restarts to play it.",
		AE_SETTINGS_RESTART },
	{ AE_SETTINGS_SYSTEM, "FILES", "MCC FILES", AE_SETTINGS_SPINNER, "game.mcc_use",
		VALUES(mcc_use_texts, mcc_use_values), AE_SETTINGS_ALWAYS,
		"Play MCC's maps and Workshop maps, and its Custom Edition files: ASK first, always (ON), or never (OFF).", 0 },
/* (Android updates through its store's app, the web build never; macOS opens the release page) */
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	{ AE_SETTINGS_SYSTEM, "STARTUP", "INTRO MOVIES", AE_SETTINGS_TOGGLE, "game.intro_movies", ON_OFF,
		AE_SETTINGS_ALWAYS, "The opening movies before the main menu. Any button skips one; OFF goes straight to the menu.", 0 },
#endif
#if !defined(HALO_CONSOLE_NO_UPDATER) && !defined(HALO_ANDROID) && !defined(HALO_WEB)
	{ AE_SETTINGS_SYSTEM, "STARTUP", "AUTO UPDATE", AE_SETTINGS_TOGGLE, "update.auto", ON_OFF, AE_SETTINGS_ALWAYS,
		"At start, look for a new Arena Evolved and ask before updating. OFF never looks. Release downloads only.", 0 },
#endif
#if !defined(HALO_ANDROID) && !defined(HALO_WEB)
	/* (0.7.5) F9's recordings; the folder is the game's recordings folder (capture.record_directory in config.toml) */
	{ AE_SETTINGS_SYSTEM, "RECORDING", "QUALITY", AE_SETTINGS_SPINNER, "capture.record_quality",
		VALUES(record_quality_texts, record_quality_values), AE_SETTINGS_ALWAYS,
		"F9's recordings: LOW makes small files, HIGH the sharpest. MEDIUM is the middle. They go to the recordings "
		"folder beside config.toml.", 0 },
	{ AE_SETTINGS_SYSTEM, "RECORDING", "FRAME RATE", AE_SETTINGS_SPINNER, "capture.record_fps",
		VALUES(record_fps_texts, record_fps_values), AE_SETTINGS_ALWAYS,
		"The recordings' frames a second, whatever the game's frame rate.", 0 },
#endif
	{ AE_SETTINGS_SYSTEM, "CAMPAIGN", "FALL DAMAGE", AE_SETTINGS_TOGGLE, "game.fall_damage", ON_OFF,
		AE_SETTINGS_ALWAYS, "Campaign only: OFF and landings never hurt. Gametypes have their own in GAME OPTIONS.", 0 },
	{ AE_SETTINGS_SYSTEM, "CAMPAIGN", "HEALTH", AE_SETTINGS_SPINNER, "game.health",
		VALUES(health_texts, health_values), AE_SETTINGS_ALWAYS,
		"Campaign only: how health comes back. HALO CE: health packs. Gametypes have their own.", 0 },
	{ AE_SETTINGS_SYSTEM, "DEV", "DEV MODE", AE_SETTINGS_TOGGLE, "system.dev_mode", ON_OFF, AE_SETTINGS_WHEN_DEV_SWITCH,
		"Tools for testing AE: the dev cameras, hit markers, TEST RUN and the weapon wheel (hold SWITCH WEAPON: every "
		"weapon in the map). This applies to THIS PC only.",
		AE_SETTINGS_THIS_PC | AE_SETTINGS_PILL_ALWAYS },
	{ AE_SETTINGS_SYSTEM, "DEV", "TEST RUN", AE_SETTINGS_SUBSCREEN, "action.test_run", NULL, NULL, 0,
		AE_SETTINGS_DEV_MODE, "Play the test lists on this PC and give your verdicts in the game.", 0 },
	/* (0.7.7) HUD > RETICLE > EDIT CROSSHAIR...: the CROSSHAIR screen's rows (ae_screen_crosshair.c: its tabs show them; their values
	are the saved crosshair's, ae_crosshair_edit.c). NAME, SHAPE, SHARE CODE and DELETE are the screen's own (its strip, its SHAPE
	tab) */
	{ AE_SETTINGS_CROSSHAIR, "COLOR", "COLOR", AE_SETTINGS_SPINNER, "xh.color", VALUES(xh_palette_texts, xh_palette_values), AE_SETTINGS_ALWAYS,
		"The crosshair's colour. Red over an enemy and green over an ally stay, whatever you pick.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "COLOR", "CUSTOM COLOR", AE_SETTINGS_SUBSCREEN, "action.crosshair_hex", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"Any colour as six hex digits, #RRGGBB. {A} types it. Red over an enemy and green over an ally still win.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "COLOR", "OPACITY", AE_SETTINGS_SPINNER, "xh.opacity", VALUES(xh_opacity_main_texts, xh_opacity_main_values), AE_SETTINGS_ALWAYS,
		"How solid the whole crosshair is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "CENTER", "CENTER", AE_SETTINGS_SPINNER, "xh.centre", VALUES(xh_centre_kind_texts, xh_centre_kind_values), AE_SETTINGS_ALWAYS,
		"The middle: nothing, a dot or a plus.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "CENTER", "CENTER SIZE", AE_SETTINGS_SPINNER, "xh.centre_size", VALUES(xh_centre_texts, xh_centre_values), AE_SETTINGS_ALWAYS,
		"The dot's radius, or the plus's thickness.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "INNER LINES", "INNER LINES", AE_SETTINGS_TOGGLE, "xh.inner_on", ON_OFF, AE_SETTINGS_ALWAYS,
		"Four short lines round the middle. Inner sit nearer.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "INNER LINES", "INNER LENGTH", AE_SETTINGS_SPINNER, "xh.inner_length", VALUES(xh_half_texts, xh_half_values), AE_SETTINGS_ALWAYS,
		"How long each line is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "INNER LINES", "INNER THICKNESS", AE_SETTINGS_SPINNER, "xh.inner_thickness", VALUES(xh_thick_texts, xh_thick_values), AE_SETTINGS_ALWAYS,
		"How thick each line is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "INNER LINES", "INNER GAP", AE_SETTINGS_SPINNER, "xh.inner_offset", VALUES(xh_half_texts, xh_half_values), AE_SETTINGS_ALWAYS,
		"How far each line starts from the middle.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "INNER LINES", "INNER OPACITY", AE_SETTINGS_SPINNER, "xh.inner_opacity", VALUES(xh_opacity_texts, xh_opacity_values), AE_SETTINGS_ALWAYS,
		"How solid these lines are.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTER LINES", "OUTER LINES", AE_SETTINGS_TOGGLE, "xh.outer_on", ON_OFF, AE_SETTINGS_ALWAYS,
		"Four short lines round the middle. Outer sit farther.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTER LINES", "OUTER LENGTH", AE_SETTINGS_SPINNER, "xh.outer_length", VALUES(xh_half_texts, xh_half_values), AE_SETTINGS_ALWAYS,
		"How long each line is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTER LINES", "OUTER THICKNESS", AE_SETTINGS_SPINNER, "xh.outer_thickness", VALUES(xh_thick_texts, xh_thick_values), AE_SETTINGS_ALWAYS,
		"How thick each line is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTER LINES", "OUTER GAP", AE_SETTINGS_SPINNER, "xh.outer_offset", VALUES(xh_half_texts, xh_half_values), AE_SETTINGS_ALWAYS,
		"How far each line starts from the middle.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTER LINES", "OUTER OPACITY", AE_SETTINGS_SPINNER, "xh.outer_opacity", VALUES(xh_opacity_texts, xh_opacity_values), AE_SETTINGS_ALWAYS,
		"How solid these lines are.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "FRAME", "FRAME", AE_SETTINGS_TOGGLE, "xh.frame_on", ON_OFF, AE_SETTINGS_ALWAYS,
		"The shape's rings, arcs, brackets and arrows.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "FRAME", "FRAME SIZE", AE_SETTINGS_SPINNER, "xh.frame_size", VALUES(xh_frame_texts, xh_frame_values), AE_SETTINGS_ALWAYS,
		"How big the frame is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "FRAME", "FRAME THICKNESS", AE_SETTINGS_SPINNER, "xh.frame_thickness", VALUES(xh_thick_texts, xh_thick_values), AE_SETTINGS_ALWAYS,
		"How thick the frame's strokes are.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "FRAME", "FRAME OPACITY", AE_SETTINGS_SPINNER, "xh.frame_opacity", VALUES(xh_opacity_texts, xh_opacity_values), AE_SETTINGS_ALWAYS,
		"How solid the frame is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTLINE", "OUTLINE", AE_SETTINGS_TOGGLE, "xh.outline_on", ON_OFF, AE_SETTINGS_ALWAYS,
		"A dark edge round every stroke, so the crosshair shows on bright ground.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTLINE", "OUTLINE THICKNESS", AE_SETTINGS_SPINNER, "xh.outline_thickness", VALUES(xh_outline_texts, xh_outline_values), AE_SETTINGS_ALWAYS,
		"How wide the dark edge is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "OUTLINE", "OUTLINE OPACITY", AE_SETTINGS_SPINNER, "xh.outline_opacity", VALUES(xh_opacity_texts, xh_opacity_values), AE_SETTINGS_ALWAYS,
		"How dark the edge is.", 0 },
	{ AE_SETTINGS_CROSSHAIR, "MOTION", "MOVEMENT SPREAD", AE_SETTINGS_TOGGLE, "xh.movement", ON_OFF, AE_SETTINGS_ALWAYS,
		"The lines open a little while you move (a feel cue: movement does not change your aim).", 0 },
	{ AE_SETTINGS_CROSSHAIR, "MOTION", "FIRING SPREAD", AE_SETTINGS_TOGGLE, "xh.firing", ON_OFF, AE_SETTINGS_ALWAYS,
		"The lines open with the weapon's real spread and kick when you fire.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "PISTOL", AE_SETTINGS_SPINNER, "xw.pistol", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "ASSAULT RIFLE", AE_SETTINGS_SPINNER, "xw.assault_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "SHOTGUN", AE_SETTINGS_SPINNER, "xw.shotgun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "SNIPER RIFLE", AE_SETTINGS_SPINNER, "xw.sniper_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "ROCKET LAUNCHER", AE_SETTINGS_SPINNER, "xw.rocket_launcher", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "PLASMA PISTOL", AE_SETTINGS_SPINNER, "xw.plasma_pistol", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "PLASMA RIFLE", AE_SETTINGS_SPINNER, "xw.plasma_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "NEEDLER", AE_SETTINGS_SPINNER, "xw.needler", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "FLAMETHROWER", AE_SETTINGS_SPINNER, "xw.flamethrower", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "STOCK WEAPONS", "GRAVITY RIFLE", AE_SETTINGS_SPINNER, "xw.gravity_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "WARTHOG GUN", AE_SETTINGS_SPINNER, "xw.warthog_gun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "SCORPION CANNON", AE_SETTINGS_SPINNER, "xw.scorpion_cannon", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "GHOST GUN", AE_SETTINGS_SPINNER, "xw.ghost_gun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "ROCKET WARTHOG GUN", AE_SETTINGS_SPINNER, "xw.rwarthog_gun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "BANSHEE GUN", AE_SETTINGS_SPINNER, "xw.banshee_gun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "VEHICLE GUNS", "GUN TURRET", AE_SETTINGS_SPINNER, "xw.gun_turret_gun", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "BATTLE RIFLE", AE_SETTINGS_SPINNER, "xw.battle_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "SMG", AE_SETTINGS_SPINNER, "xw.smg", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "SUPPRESSED SMG", AE_SETTINGS_SPINNER, "xw.smg_silenced", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "CARBINE", AE_SETTINGS_SPINNER, "xw.carbine", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "SUPPRESSED AR", AE_SETTINGS_SPINNER, "xw.ar_silenced", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "DMR", AE_SETTINGS_SPINNER, "xw.dmr", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "EVO RIFLE", AE_SETTINGS_SPINNER, "xw.evo_rifle", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "BOLT SNIPER", AE_SETTINGS_SPINNER, "xw.sniper_bolt", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "HJ MAGNUM", AE_SETTINGS_SPINNER, "xw.magnum_scoped", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "BRUTE SHOT", AE_SETTINGS_SPINNER, "xw.brute_shot", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "COMBAT AR", AE_SETTINGS_SPINNER, "xw.ar_modern", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "SIDEARM", AE_SETTINGS_SPINNER, "xw.sidearm", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
	{ AE_SETTINGS_CROSSHAIR_WEAPONS, "AE WEAPONS", "ENERGY SWORD", AE_SETTINGS_SPINNER, "xw.energy_sword", NULL, NULL, 0, AE_SETTINGS_ALWAYS,
		"USE MAIN: the crosshair HUD > RETICLE > CROSSHAIR names. STOCK: this weapon's own reticle. Or one of your saved crosshairs.", 0 },
};

short ae_settings_row_count(void)
{
	return COUNT(rows);
}

struct ae_settings_row const *ae_settings_row(short index)
{
	lists_build();
	return index >= 0 && index < COUNT(rows) ? &rows[index] : NULL;
}

short ae_settings_row_of(const char *key)
{
	short index;

	for (index = 0; key && index < COUNT(rows); index++)
	{
		if (!strcmp(rows[index].key, key))
			return index;
	}
	return -1;
}

static short run_list_of(const char *key)
{
	short index;

	/* (every weapon's row of HUD > RETICLE > WEAPONS shares one list of choices) */
	if (key && !strncmp(key, "xw.", 3) && strcmp(key, "xw.list"))
		key = "xw.list";
	for (index = 0; key && index < RUN_LISTS; index++)
	{
		if (run_keys[index] && !strcmp(run_keys[index], key))
			return index;
	}
	return -1;
}

int ae_settings_set_values(const char *key, const char *const *texts, const char *const *values, short count)
{
	short list = run_list_of(key), index;
	int at;

	if (list < 0)
		return 0;
	run_lists[list].count = 0;
	for (index = 0; index < count && run_lists[list].count < RUN_VALUES; index++)
	{
		short place = run_lists[list].count;
		const char *text = texts ? texts[index] : values[index];

		if (!values[index] || strlen(values[index]) >= RUN_TEXT || !text)
			continue;
		snprintf(run_lists[list].value[place], RUN_TEXT, "%s", values[index]);
		if (texts)
			snprintf(run_lists[list].text[place], RUN_TEXT, "%s", text);
		else
		{
			for (at = 0; text[at] && at < RUN_TEXT - 1; at++)
				run_lists[list].text[place][at] = (char)toupper((unsigned char)text[at]);
			run_lists[list].text[place][at] = 0;
		}
		run_lists[list].values[place] = run_lists[list].value[place];
		run_lists[list].texts[place] = run_lists[list].text[place];
		run_lists[list].count++;
	}
	return 1;
}

void ae_settings_set_voices(const char *const *names, short count)
{
	const char *kept[RUN_VALUES];
	short index, found = 0;

	/* (folder names too long for a value are left out, as before) */
	for (index = 0; index < count && found < RUN_VALUES; index++)
	{
		if (names[index] && names[index][0] && strlen(names[index]) < 48)
			kept[found++] = names[index];
	}
	if (found > 32)
		found = 32;
	ae_settings_set_values("game.callout_voice", NULL, kept, found);
}

short ae_settings_value_count(struct ae_settings_row const *row)
{
	short list;

	if (!row)
		return 0;
	lists_build();
	if (row->count)
		return row->count;
	list = run_list_of(row->key);
	return list >= 0 ? run_lists[list].count : 0;
}

const char *ae_settings_value_text(struct ae_settings_row const *row, short index)
{
	if (index < 0 || index >= ae_settings_value_count(row))
		return "";
	return row->count ? row->texts[index] : run_lists[run_list_of(row->key)].texts[index];
}

const char *ae_settings_value_of(struct ae_settings_row const *row, short index)
{
	if (index < 0 || index >= ae_settings_value_count(row))
		return "";
	return row->count ? row->values[index] : run_lists[run_list_of(row->key)].values[index];
}

int ae_settings_row_takes_empty(struct ae_settings_row const *row)
{
	short index, count = ae_settings_value_count(row);

	for (index = 0; index < count; index++)
	{
		if (!ae_settings_value_of(row, index)[0])
			return 1;
	}
	return 0;
}

/* ---------- what shows */

static int value_is(struct ae_settings_facts const *facts, const char *key, const char *value)
{
	const char *now = facts && facts->value ? facts->value(facts->context, key) : NULL;

	return now && !strcmp(now, value);
}

int ae_settings_row_shown(struct ae_settings_row const *row, struct ae_settings_facts const *facts)
{
	if (!row)
		return 0;
	switch (row->when)
	{
	case AE_SETTINGS_WHEN_DUALSENSE: return facts && facts->dualsense;
	case AE_SETTINGS_WHEN_KEYBOARD: return facts && facts->keyboard;
	case AE_SETTINGS_WHEN_MOUSE_CLASSIC: return facts && facts->keyboard && !value_is(facts, "input.mouse_style", "mcc");
	case AE_SETTINGS_WHEN_MOUSE_MCC: return facts && facts->keyboard && value_is(facts, "input.mouse_style", "mcc");
	case AE_SETTINGS_WHEN_ZOOM_BOTH: return value_is(facts, "input.zoom_mode", "both");
	case AE_SETTINGS_WHEN_MOUSE_ZOOM_BOTH:
		return facts && facts->keyboard && value_is(facts, "input.mouse_zoom_mode", "both");
	case AE_SETTINGS_WHEN_MEDALS: return !value_is(facts, "display.medals", "false");
	case AE_SETTINGS_WHEN_NOT_AE_HUD: return !value_is(facts, "display.hud_style", "ae");
	case AE_SETTINGS_WHEN_CLOCK: return !value_is(facts, "display.match_clock", "off");
	case AE_SETTINGS_WHEN_GLOW: return value_is(facts, "display.shield_glow", "true");
	case AE_SETTINGS_WHEN_PANEL: return value_is(facts, "display.scoreboard_background", "true");
	case AE_SETTINGS_WHEN_CALLOUTS: return !value_is(facts, "game.callouts", "off");
	case AE_SETTINGS_WHEN_WINDOWED: return value_is(facts, "display.mode", "windowed");
	case AE_SETTINGS_WHEN_NOT_WINDOWED: return !value_is(facts, "display.mode", "windowed");
	case AE_SETTINGS_WHEN_PERFORMANCE: return !value_is(facts, "display.performance", "off");
	case AE_SETTINGS_WHEN_NINTENDO: return facts && facts->nintendo;
	case AE_SETTINGS_WHEN_OVERLAY_LINE:
		return !value_is(facts, "display.performance", "off") || !value_is(facts, "display.ping", "off");
	case AE_SETTINGS_WHEN_HDR: return value_is(facts, "display.hdr", "true");
	case AE_SETTINGS_WHEN_HDR_DISPLAY: return facts && facts->hdr_display;
	case AE_SETTINGS_WHEN_VOICE: return !value_is(facts, "audio.voice_chat", "off");
	case AE_SETTINGS_WHEN_PUSH_TO_TALK: return value_is(facts, "audio.voice_chat", "push_to_talk");
	case AE_SETTINGS_WHEN_MAP_DOWNLOAD: return value_is(facts, "network.map_download", "true");
	case AE_SETTINGS_WHEN_MAP_SEED: return value_is(facts, "network.map_seed", "true");
	case AE_SETTINGS_WHEN_DISCORD: return !value_is(facts, "discord.presence", "off");
	case AE_SETTINGS_WHEN_STREAMER: return value_is(facts, "streamer.enabled", "true");
	case AE_SETTINGS_WHEN_DEV_SWITCH: return !(facts && facts->release_build) || value_is(facts, "system.dev_mode", "true");
	case AE_SETTINGS_DEV_MODE: return value_is(facts, "system.dev_mode", "true");
	case AE_SETTINGS_WHEN_AIM_MODERN: return value_is(facts, "input.aim_control", "modern");
	case AE_SETTINGS_WHEN_GYRO: return value_is(facts, "input.gyro_aim", "true") || value_is(facts, "input.gyro_aim", "zoom");
	default: return 1;
	}
}

short ae_settings_entries(short group, struct ae_settings_facts const *facts, short *entries, short maximum)
{
	const char *section = NULL;
	short index, count = 0;

	lists_build();
	for (index = 0; index < COUNT(rows) && count < maximum; index++)
	{
		if (rows[index].group != group || !ae_settings_row_shown(&rows[index], facts))
			continue;
		if (!section || strcmp(section, rows[index].section))
		{
			if (count + 1 >= maximum)
				break;
			section = rows[index].section;
			entries[count++] = (short)(AE_SETTINGS_HEADER | index);
		}
		entries[count++] = index;
	}
	return count;
}

int ae_settings_entry_is_header(short entry)
{
	return (entry & AE_SETTINGS_HEADER) != 0;
}

const char *ae_settings_entry_section(short entry)
{
	struct ae_settings_row const *row = ae_settings_row((short)(entry & ~AE_SETTINGS_HEADER));

	return row ? row->section : "";
}

short ae_settings_sections(short const *entries, short count, struct ae_settings_section *sections, short maximum)
{
	short place, found = 0;

	for (place = 0; place < count; place++)
	{
		if (ae_settings_entry_is_header(entries[place]))
		{
			if (found >= maximum)
				break;
			sections[found].name = ae_settings_entry_section(entries[place]);
			sections[found].header = place;
			sections[found].rows = 0;
			found++;
		}
		else if (found)
			sections[found - 1].rows++;
	}
	return found;
}

short ae_settings_section_at(struct ae_settings_section const *sections, short count, short place)
{
	short index, at = -1;

	for (index = 0; index < count; index++)
	{
		if (sections[index].header <= place)
			at = index;
	}
	return at;
}

short ae_settings_row_step(short const *entries, short count, short place, short direction)
{
	short next = place;

	do
		next = (short)(next + direction);
	while (next >= 0 && next < count && ae_settings_entry_is_header(entries[next]));
	return next >= 0 && next < count ? next : place;
}

/* ---------- values */

static int number(const char *text, double *value)
{
	char *end;

	if (!text || !*text)
		return 0;
	*value = strtod(text, &end);
	return end != text && !*end;
}

short ae_settings_value_index(struct ae_settings_row const *row, const char *text)
{
	short count = ae_settings_value_count(row), index, best = -1;
	double wanted, distance = 0.0;

	if (!text)
		return -1;
	/* (0.7.5, PARTICLE LIMIT: 0 is the port's own 8192, its biggest step) */
	if (!strcmp(text, "0") && row->key && !strcmp(row->key, "display.particle_limit"))
		text = "8192";
	for (index = 0; index < count; index++)
	{
		if (!strcmp(ae_settings_value_of(row, index), text))
			return index;
	}
	/* (a number written another way, or between the steps: the nearest) */
	if (!number(text, &wanted))
		return -1;
	for (index = 0; index < count; index++)
	{
		double value;

		if (number(ae_settings_value_of(row, index), &value) && (best < 0 || fabs(value - wanted) < distance))
		{
			best = index;
			distance = fabs(value - wanted);
		}
	}
	return best;
}

float ae_settings_slider(struct ae_settings_row const *row, short index)
{
	short count = ae_settings_value_count(row);

	if (count < 2 || index < 0)
		return 0.0f;
	return (float)index / (float)(count - 1);
}

/* ---------- (0.3.9) number rows */

/* (each a slider's key; its range is its steps' first and last but the special) */
static struct { const char *key; double minimum, maximum, special; int has_special; } const numbers[] =
{
	{ "input.mouse_sensitivity", 0.1, 4.0, 0.0, 0 },
	{ "input.mouse_vertical_sensitivity", 0.1, 4.0, 0.0, 1 },
	{ "input.mouse_mcc_sensitivity", 0.1, 10.0, 0.0, 0 },
	{ "input.mouse_zoom_scale", 0.1, 2.0, 0.0, 0 },
	{ "input.mouse_vehicle_scale", 0.1, 2.0, 0.0, 0 },
};

int ae_settings_row_number(struct ae_settings_row const *row, struct ae_settings_number *number)
{
	short index;

	if (!row || row->kind != AE_SETTINGS_SLIDER || !row->key)
		return 0;
	for (index = 0; index < COUNT(numbers); index++)
	{
		if (strcmp(row->key, numbers[index].key))
			continue;
		if (number)
		{
			number->minimum = numbers[index].minimum;
			number->maximum = numbers[index].maximum;
			number->special = numbers[index].special;
			number->has_special = numbers[index].has_special;
		}
		return 1;
	}
	return 0;
}

/* (the numbers are counted in hundredths: 0.01 steps never drift) */
static long hundredths(double value)
{
	return (long)floor(value * 100.0 + 0.5);
}

static void hundredths_text(long value, char *text, size_t size)
{
	if (value < 0)
		value = 0;
	if (value % 10)
		snprintf(text, size, "%ld.%02ld", value / 100, value % 100);
	else
		snprintf(text, size, "%ld.%ld", value / 100, value % 100 / 10);
}

void ae_settings_number_text(double value, char *text, size_t size)
{
	hundredths_text(hundredths(value), text, size);
}

/* the step whose value is this number, -1 */
static short step_of(struct ae_settings_row const *row, long value)
{
	short count = ae_settings_value_count(row), index;

	for (index = 0; index < count; index++)
	{
		double step;

		if (number(ae_settings_value_of(row, index), &step) && hundredths(step) == value)
			return index;
	}
	return -1;
}

/* a number as the setting's text: a step's own text when it is a step's ("1", "0" for SAME), else its hundredths */
static void setting_text(struct ae_settings_row const *row, long value, char *text, size_t size)
{
	short index = step_of(row, value);

	if (index >= 0)
		snprintf(text, size, "%s", ae_settings_value_of(row, index));
	else
		hundredths_text(value, text, size);
}

void ae_settings_number_shown(struct ae_settings_row const *row, const char *value, char *text, size_t size)
{
	struct ae_settings_number range;
	double wanted;
	short index;

	if (!ae_settings_row_number(row, &range) || !number(value, &wanted))
	{
		index = ae_settings_value_index(row, value);
		snprintf(text, size, "%s", index >= 0 ? ae_settings_value_text(row, index) : value ? value : "");
		return;
	}
	index = step_of(row, hundredths(wanted));
	/* (a value no step has and outside the range, as MCC SENSITIVITY's 0.0 before its seed: its nearest step, as
	before) */
	if (index < 0 && (hundredths(wanted) < hundredths(range.minimum) || hundredths(wanted) > hundredths(range.maximum)))
		index = ae_settings_value_index(row, value);
	if (index >= 0)
		snprintf(text, size, "%s", ae_settings_value_text(row, index));
	else
		ae_settings_number_text(wanted, text, size);
}

int ae_settings_number_typing_ok(const char *typed)
{
	int dots = 0, decimals = 0, length = 0;

	if (!typed)
		return 0;
	for (; *typed; typed++, length++)
	{
		if (*typed == '.')
		{
			if (dots++)
				return 0;
		}
		else if (*typed >= '0' && *typed <= '9')
		{
			if (dots && ++decimals > 2)
				return 0;
		}
		else
			return 0;
	}
	return length <= AE_SETTINGS_NUMBER_TYPED;
}

int ae_settings_number_typed(struct ae_settings_row const *row, const char *typed, char *text, size_t size)
{
	struct ae_settings_number range;
	long value, low, high;

	if (!ae_settings_row_number(row, &range) || !ae_settings_number_typing_ok(typed) || !strpbrk(typed, "0123456789"))
		return 0;
	value = hundredths(strtod(typed, NULL));
	low = hundredths(range.minimum);
	high = hundredths(range.maximum);
	if (!(range.has_special && value == hundredths(range.special)))
		value = value < low ? low : value > high ? high : value;
	setting_text(row, value, text, size);
	return 1;
}

int ae_settings_number_step(struct ae_settings_row const *row, const char *now, short direction, int fine, int wrap,
	char *text, size_t size)
{
	struct ae_settings_number range;
	short count = ae_settings_value_count(row), index, best = -1;
	long value, low, high, best_value = 0;
	double parsed;

	if (!ae_settings_row_number(row, &range) || !direction)
		return 0;
	low = hundredths(range.minimum);
	high = hundredths(range.maximum);
	value = number(now, &parsed) ? hundredths(parsed) : low;
	if (fine)
	{
		/* (from outside the range, the special too: its end on that side, else held) */
		if (value < low || value > high)
		{
			if ((value < low) != (direction > 0))
				return 0;
			value = direction > 0 ? low : high;
		}
		else
		{
			value += direction > 0 ? 1 : -1;
			if (value < low || value > high)
			{
				/* (a fresh press goes round to the other end, as the coarse step does; a held repeat stops) */
				if (!wrap)
					return 0;
				value = value < low ? high : low;
			}
		}
		setting_text(row, value, text, size);
		return 1;
	}
	/* (the nearest step past the value on that side; none: a fresh press rounds to the far end's, a held repeat stops) */
	for (index = 0; index < count; index++)
	{
		double step;
		long at;

		if (!number(ae_settings_value_of(row, index), &step))
			continue;
		at = hundredths(step);
		if (direction > 0 ? at > value && (best < 0 || at < best_value) : at < value && (best < 0 || at > best_value))
		{
			best = index;
			best_value = at;
		}
	}
	if (best < 0 && wrap)
	{
		for (index = 0; index < count; index++)
		{
			double step;
			long at;

			if (!number(ae_settings_value_of(row, index), &step))
				continue;
			at = hundredths(step);
			if (direction > 0 ? at < value && (best < 0 || at < best_value) :
				at > value && (best < 0 || at > best_value))
			{
				best = index;
				best_value = at;
			}
		}
	}
	if (best < 0)
		return 0;
	snprintf(text, size, "%s", ae_settings_value_of(row, best));
	return 1;
}

float ae_settings_slider_at(struct ae_settings_row const *row, const char *text)
{
	short count = ae_settings_value_count(row), index;
	double value, a, b;

	if (count < 2)
		return 0.0f;
	if (!number(text, &value))
		return ae_settings_slider(row, ae_settings_value_index(row, text));
	for (index = 0; index + 1 < count; index++)
	{
		if (!number(ae_settings_value_of(row, index), &a) || !number(ae_settings_value_of(row, index + 1), &b) || b <= a)
			return ae_settings_slider(row, ae_settings_value_index(row, text));
		if (index == 0 && value <= a)
			return 0.0f;
		if (value <= b)
			return ((float)index + (float)((value - a) / (b - a))) / (float)(count - 1);
	}
	return 1.0f;
}

/* ---------- the dialogs' texts */

void ae_settings_keep_body(const char *group, const char *who, const char *const *labels, short count, char *text,
	size_t size)
{
	char list[200];
	short index, named = count > 3 ? 3 : count;
	size_t length = 0;

	list[0] = 0;
	for (index = 0; index < named; index++)
	{
		const char *joint = index == 0 ? "" : count <= 3 && index == named - 1 ? " and " : ", ";

		length += (size_t)snprintf(list + length, length < sizeof(list) ? sizeof(list) - length : 0, "%s%s", joint,
			labels[index]);
		if (length >= sizeof(list))
			break;
	}
	if (count > 3)
		snprintf(list + strlen(list), sizeof(list) - strlen(list), " and %d more", count - 3);
	if (who)
		snprintf(text, size, "%d %s setting%s changed for %s: %s. They are in use already.", count, group,
			count == 1 ? "" : "s", who, list);
	else
		snprintf(text, size, "%d %s setting%s changed on this PC: %s. They are in use already.", count, group,
			count == 1 ? "" : "s", list);
}

short ae_settings_restore_body(const char *group, const char *who, short changes, char *text, size_t size)
{
	if (changes <= 0)
		snprintf(text, size, "%s is already at its defaults.", group);
	else if (who)
		snprintf(text, size, "Every %s setting for %s goes back to its default: %d setting%s change%s. Other players "
			"and this PC's settings stay as they are. You can still undo it as you leave.", group, who, changes,
			changes == 1 ? "" : "s", changes == 1 ? "s" : "");
	else
		snprintf(text, size, "Every %s setting on this PC goes back to its default: %d setting%s change%s. Players' "
			"own settings stay as they are.", group, changes, changes == 1 ? "" : "s", changes == 1 ? "s" : "");
	return changes > 0 ? changes : 0;
}

/* ---------- (0.7.0) whole-number rows: a list's values and any whole number of a range besides */

enum { WHOLE_AUTO_ORDER = -2, WHOLE_NONE_ORDER = 1000000 };

/* (0.7.6) the rows that take a typed whole number besides their list's values, by setting: the range in the unit the row
shows (ae_number_field.h's range; the stored text is the number itself, "97", or "97.0" in a real). A 0 typed is held up to the least; the list's DEFAULT / AUTO is reached from the list.
Refused (the lists are all they need): the volumes (stored 0 to 1 in tenths), the mouse numbers (their own decimals) */
static struct { const char *key; struct ae_number_range range; const char *suffix; } const wholes[] =
{
	{ "display.max_fps", { 30, 1000, NULL, NULL, NULL }, NULL },
	{ "display.fov", { 60, 150, NULL, NULL, NULL }, NULL },
	{ "display.viewmodel_fov", { 60, 150, NULL, NULL, NULL }, NULL },
	{ "input.left_stick_deadzone", { 1, 50, NULL, NULL, NULL }, " %" },
	{ "input.right_stick_deadzone", { 1, 50, NULL, NULL, NULL }, " %" },
	{ "display.waypoint_size", { 50, 100, NULL, NULL, NULL }, " %" },
	{ "display.reticle_size", { 50, 200, NULL, NULL, NULL }, " %" },
};

int ae_settings_row_whole(struct ae_settings_row const *row, struct ae_settings_whole *whole)
{
	short index;

	if (!row || (row->kind != AE_SETTINGS_SPINNER && row->kind != AE_SETTINGS_SLIDER) || !row->key)
		return 0;
	for (index = 0; index < COUNT(wholes); index++)
	{
		if (strcmp(row->key, wholes[index].key))
			continue;
		if (whole)
		{
			whole->minimum = wholes[index].range.minimum;
			whole->maximum = wholes[index].range.maximum;
			whole->suffix = wholes[index].suffix;
		}
		return 1;
	}
	return 0;
}

/* a list value's place among the numbers: AUTO (0) below them all, NONE (-1) above them all */
static long whole_order(const char *value)
{
	long n = strtol(value, NULL, 10);

	return n == 0 ? WHOLE_AUTO_ORDER : n < 0 ? WHOLE_NONE_ORDER : n;
}

/* a text's whole number: digits only (no sign), 1 to 6 of them, and a real's ".0" after them; -1 when it is not */
static long whole_of(const char *text)
{
	long value = 0;
	int length = 0;

	if (!text || !*text)
		return -1;
	for (; *text && *text != '.'; text++, length++)
	{
		if (*text < '0' || *text > '9' || length >= 6)
			return -1;
		value = value * 10 + (*text - '0');
	}
	if (!length)
		return -1;
	if (*text == '.')
		for (text++; *text; text++)
			if (*text != '0')
				return -1;
	return value;
}

void ae_settings_whole_shown(struct ae_settings_row const *row, const char *value, char *text, size_t size)
{
	short count = ae_settings_value_count(row), index;
	long n = whole_of(value);

	for (index = 0; index < count; index++)
		if (value && (!strcmp(ae_settings_value_of(row, index), value) ||
			(n > 0 && whole_of(ae_settings_value_of(row, index)) == n)))
		{
			snprintf(text, size, "%s", ae_settings_value_text(row, index));
			return;
		}
	if (n > 0)
	{
		struct ae_settings_whole range;

		range.suffix = NULL;
		ae_settings_row_whole(row, &range);
		snprintf(text, size, "%ld%s", n, range.suffix ? range.suffix : "");
	}
	else
	{
		index = ae_settings_value_index(row, value);
		snprintf(text, size, "%s", index >= 0 ? ae_settings_value_text(row, index) : value ? value : "");
	}
}

int ae_settings_whole_typing_ok(const char *typed)
{
	return ae_number_typing_ok(typed, AE_SETTINGS_WHOLE_TYPED);
}

int ae_settings_whole_typed(struct ae_settings_row const *row, const char *typed, char *text, size_t size)
{
	struct ae_settings_whole range;
	struct ae_number_range held;
	long value;

	if (!ae_settings_row_whole(row, &range) || !ae_settings_whole_typing_ok(typed) || !typed[0])
		return 0;
	memset(&held, 0, sizeof(held));
	held.minimum = range.minimum;
	held.maximum = range.maximum;
	ae_number_parse(typed, &value);
	value = ae_number_clamp(&held, value);
	snprintf(text, size, "%ld", value);
	return 1;
}

int ae_settings_whole_step(struct ae_settings_row const *row, const char *now, short direction, int wrap, char *text,
	size_t size)
{
	short count = ae_settings_value_count(row), index, best = -1;
	long order, best_order = 0, current;

	if (!direction || !ae_settings_row_whole(row, NULL))
		return 0;
	current = whole_of(now);
	/* (a value in no list: AUTO and NONE are not numbers typed; a number's place among the list's) */
	order = current > 0 ? current : now ? whole_order(now) : WHOLE_AUTO_ORDER;
	for (index = 0; index < count; index++)
	{
		long at = whole_order(ae_settings_value_of(row, index));

		if (direction > 0 ? at > order && (best < 0 || at < best_order) : at < order && (best < 0 || at > best_order))
		{
			best = index;
			best_order = at;
		}
	}
	/* (past the last: a fresh press goes round to the other end (FRAME RATE LIMIT: AUTO and NONE); a held repeat stops) */
	if (best < 0 && wrap)
		for (index = 0; index < count; index++)
		{
			long at = whole_order(ae_settings_value_of(row, index));

			if (best < 0 || (direction > 0 ? at < best_order : at > best_order))
			{
				best = index;
				best_order = at;
			}
		}
	if (best < 0)
		return 0;
	snprintf(text, size, "%s", ae_settings_value_of(row, best));
	return 1;
}

short ae_settings_whole_values(struct ae_settings_row const *row, const char *now, struct ae_settings_whole_value *values,
	short maximum)
{
	short count = ae_settings_value_count(row), index, total = 0, inserted = 0;
	long typed = whole_of(now);
	struct ae_settings_whole range;
	const char *suffix;

	if (!ae_settings_row_whole(row, &range))
		return 0;
	suffix = range.suffix ? range.suffix : "";
	/* a value that no list entry has (237) comes in its place among them */
	for (index = 0; index < count; index++)
		if (typed > 0 && whole_of(ae_settings_value_of(row, index)) == typed)
			inserted = 1;
	for (index = 0; index < count && total < maximum - 1; index++)
	{
		if (typed > 0 && !inserted && whole_order(ae_settings_value_of(row, index)) > typed)
		{
			snprintf(values[total].text, sizeof(values[total].text), "%ld%s", typed, suffix);
			snprintf(values[total].value, sizeof(values[total].value), "%ld", typed);
			values[total++].typed = 1;
			inserted = 1;
		}
		snprintf(values[total].text, sizeof(values[total].text), "%s", ae_settings_value_text(row, index));
		snprintf(values[total].value, sizeof(values[total].value), "%s", ae_settings_value_of(row, index));
		values[total++].typed = 0;
	}
	if (typed > 0 && !inserted && total < maximum - 1)
	{
		snprintf(values[total].text, sizeof(values[total].text), "%ld%s", typed, suffix);
		snprintf(values[total].value, sizeof(values[total].value), "%ld", typed);
		values[total++].typed = 1;
	}
	snprintf(values[total].text, sizeof(values[total].text), "TYPE A NUMBER...");
	values[total].value[0] = 0;
	values[total++].typed = 2;
	return total;
}
