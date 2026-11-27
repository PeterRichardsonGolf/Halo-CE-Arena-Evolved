/*
AE_FIREFIGHT_MENUS_RULES.H

The words and rules behind Firefight's lobby rows and its two screens, MAP CONTENT and OBJECTIVES (ae_screen_firefight_content.c), as pure
data (no engine includes; tests/ae_firefight_menus_rules_test.c). The words are Content Evolved's own (SutlabSol's Firefight, its Server
Setup rows) with the product names taken out.

Every setting these screens write goes through ae_ffm_key_allowed: Firefight's objective keys and the campaign score's. A multiplayer
game's settings are never among them, and the Firefight keys are read by a Firefight game alone.
*/

#ifndef __AE_FIREFIGHT_MENUS_RULES_H
#define __AE_FIREFIGHT_MENUS_RULES_H

#include <stddef.h>

/* ---------- the settings these screens may write */

/* 1 when key is one of Firefight's menu settings: its objectives' switches, chances and kinds, and game.campaign_score */
int ae_ffm_key_allowed(const char *key);

/* ---------- OBJECTIVES */

enum { AE_FFM_WAVE = 0, AE_FFM_ROUND = 1, AE_FFM_KINDS_WAVE = 8, AE_FFM_KINDS_ROUND = 9, AE_FFM_CHANCES = 10 };

struct ae_ffm_objective
{
	const char *key;      /* the setting (game.firefight_objective_kills) */
	const char *label;
	const char *help;
};

/* the wave's (kind 0 to 7) or the round's (0 to 8) objective; NULL out of range */
struct ae_ffm_objective const *ae_ffm_objective(int round, int kind);
int ae_ffm_objective_count(int round);
/* the switch's key ("game.firefight_objectives_round" / "_set") and the chance's */
const char *ae_ffm_objectives_key(int round);
const char *ae_ffm_chance_key(int round);
/* the chance's steps: 10 to 100 percent; the index of a percent (the nearest) */
long ae_ffm_chance_percent(int index);
int ae_ffm_chance_index(long percent);
/* the lobby's OBJECTIVES value: OFF, WAVE, ROUND or BOTH (the two switches) */
const char *ae_ffm_objectives_value(int wave_on, int round_on);
/* the screen's help for a section's switch and chance */
const char *ae_ffm_objectives_help(int round, int on);
const char *ae_ffm_chance_help(int round);

/* ---------- the lobby's rows */

/* SCALE WITH PLAYERS: 0, 5, 10, 20 and 50 percent; the index of a percent (the nearest), its text */
enum { AE_FFM_SCALES = 5 };
int ae_ffm_scale_percent(int index);
int ae_ffm_scale_index(int percent);
const char *ae_ffm_scale_text(int index);
extern const char ae_ffm_scale_help[];
/* VEHICLES (the options' firefight_vehicle_spawns): ALL AT SPAWN, ORDNANCE REWARDS, NONE */
enum { AE_FFM_VEHICLE_MODES = 3 };
const char *ae_ffm_vehicles_name(int mode);
extern const char ae_ffm_vehicles_help[];

/* ---------- MAP CONTENT (the options' firefight_map_content byte, halo_firefight.h's FIREFIGHT_MAP_CONTENT_*) */

enum { AE_FFM_MODE_VANILLA, AE_FFM_MODE_INCLUDE, AE_FFM_MODE_ONLY, AE_FFM_MODES };
enum { AE_FFM_KIND_ENEMIES, AE_FFM_KIND_WEAPONS, AE_FFM_KIND_VEHICLES, AE_FFM_CONTENT_KINDS };
enum { AE_FFM_CHOICE_ALL, AE_FFM_CHOICE_CUSTOM, AE_FFM_CHOICE_NONE, AE_FFM_CHOICES };
/* the content byte's bits (the engine's own values: the glue checks them) */
enum { AE_FFM_BIT_MODE = 0x03, AE_FFM_MODE_INCLUDE_VALUE = 2, AE_FFM_MODE_ONLY_VALUE = 3 };

/* the byte's mode (VANILLA for the byte's 0 and 1) and the byte with a mode set (the kinds' bits kept) */
int ae_ffm_content_mode(unsigned char content);
unsigned char ae_ffm_content_with_mode(unsigned char content, int mode);
/* a kind's choice and the byte with one set (the mode's bits kept); the bits are per kind: enemies 0x04 / 0x10, weapons 0x08 / 0x20,
vehicles 0x80 / 0x40 (custom / off) */
int ae_ffm_content_choice(unsigned char content, int kind);
unsigned char ae_ffm_content_with_choice(unsigned char content, int kind, int choice);
/* what a map has of its own, as the engine says (1 enemies, 2 weapons, 4 vehicles): whether it has a kind */
int ae_ffm_has_kind(unsigned char has, int kind);
const char *ae_ffm_mode_name(int mode);
const char *ae_ffm_choice_name(int choice);
const char *ae_ffm_kind_row(int kind);      /* MAP ENEMY AI, MAP WEAPONS, MAP VEHICLES */
const char *ae_ffm_kind_list_row(int kind); /* CHOOSE ENEMY AI, CHOOSE WEAPONS, CHOOSE VEHICLES */
const char *ae_ffm_kind_help(int kind);
const char *ae_ffm_kind_list_help(int kind);
const char *ae_ffm_mode_help(void);
const char *ae_ffm_loadout_help(void);
/* the row's A words and the dimmed rows' reason while the mode is VANILLA ONLY */
extern const char ae_ffm_needs_content[];

/* HEALTH RULES: SHIELDS and SHIELD REGEN (MAP'S, ON, OFF), HEALTH REGEN (OFF, ON): the rule, its choices and words */
enum { AE_FFM_HEALTH_SHIELDS, AE_FFM_HEALTH_SHIELD_REGEN, AE_FFM_HEALTH_REGEN, AE_FFM_HEALTH_RULES };
int ae_ffm_health_choices(int rule);
const char *ae_ffm_health_row(int rule);
const char *ae_ffm_health_choice(int rule, int choice);
const char *ae_ffm_health_help(int rule);

/* the lobby's MAP CONTENT value: what is taken of the map ("INCLUDE MAP: AI, WEAPONS*, VEHICLES", "VANILLA ONLY", with ", LOADOUT" and ", HEALTH"
when set; "HEALTH RULES: MAP'S" on a map with only health rules). has: the map's kinds; health: its player has health rules;
health_changed, loadout_set: those rules or weapons set for the map; "" when the map has none of it */
void ae_ffm_content_summary(unsigned char has, unsigned char content, int health, int health_changed, int loadout_set,
	char *text, size_t size);

/* a tag's name as a row shows it: its last part, in capitals ("weapons\\sniper\\sniper rifle" -> "SNIPER RIFLE") */
void ae_ffm_tag_label(const char *tag_name, char *text, size_t size);

#endif
