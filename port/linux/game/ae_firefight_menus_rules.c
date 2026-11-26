/*
AE_FIREFIGHT_MENUS_RULES.C

See ae_firefight_menus_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_firefight_menus_rules.h"

/* ---------- keys */

static const char *const wave_keys[AE_FFM_KINDS_WAVE] = {
	"game.firefight_objective_kills", "game.firefight_objective_no_deaths", "game.firefight_objective_shields",
	"game.firefight_objective_kill_first", "game.firefight_objective_dont_use", "game.firefight_objective_vehicle_kills",
	"game.firefight_objective_travel", "game.firefight_objective_weapon_only" };
static const char *const round_keys[AE_FFM_KINDS_ROUND] = {
	"game.firefight_objective_target", "game.firefight_objective_generator", "game.firefight_objective_purge",
	"game.firefight_objective_data_link", "game.firefight_objective_flag", "game.firefight_objective_rescue",
	"game.firefight_objective_oddball", "game.firefight_objective_collect", "game.firefight_objective_bomb" };

int ae_ffm_key_allowed(const char *key)
{
	int each;

	if (!key)
		return 0;
	if (!strcmp(key, "game.campaign_score") || !strcmp(key, "game.firefight_objectives_round") ||
		!strcmp(key, "game.firefight_objectives_round_chance") || !strcmp(key, "game.firefight_objectives_set") ||
		!strcmp(key, "game.firefight_objectives_set_chance"))
		return 1;
	for (each = 0; each < AE_FFM_KINDS_WAVE; each++)
		if (!strcmp(key, wave_keys[each]))
			return 1;
	for (each = 0; each < AE_FFM_KINDS_ROUND; each++)
		if (!strcmp(key, round_keys[each]))
			return 1;
	return 0;
}

/* ---------- OBJECTIVES (the words of Content Evolved's wave and round objectives) */

static struct ae_ffm_objective const wave_rows[AE_FFM_KINDS_WAVE] = {
	{ "game.firefight_objective_kills", "KILLS", "So many kills of a kind: headshots, melee, grenades, splatters or a weapon class." },
	{ "game.firefight_objective_no_deaths", "NO DEATHS", "The wave finished with no player dying. Anyone's death fails it for everyone." },
	{ "game.firefight_objective_shields", "SHIELDS INTACT", "No more than half the players' shields broken: a fall, your own blast, friendly fire count." },
	{ "game.firefight_objective_kill_first", "KILL MARKED FIRST", "An enemy of the wave is marked: kill it before any other." },
	{ "game.firefight_objective_dont_use", "DON'T USE", "A weapon class or grenade most of you carry: one shot or throw fails it for everyone." },
	{ "game.firefight_objective_vehicle_kills", "VEHICLE KILLS", "So many kills from a vehicle of a kind, one given you if the map has none of it there." },
	{ "game.firefight_objective_travel", "TRAVEL", "Reach a marked point of the map, far from you, before the wave is cleared." },
	{ "game.firefight_objective_weapon_only", "USE ONLY", "A weapon is dropped to each of you: once it lands, firing any other fails it." } };
static struct ae_ffm_objective const round_rows[AE_FFM_KINDS_ROUND] = {
	{ "game.firefight_objective_target", "HIGH VALUE TARGET", "An enemy camp round a map objective: kill its VIP before they teleport out." },
	{ "game.firefight_objective_generator", "ENEMY GENERATOR", "An enemy camp round a map objective: destroy its generator before time runs out." },
	{ "game.firefight_objective_purge", "CLEAR ENEMY BASE", "An enemy camp round a map objective: kill every one of them before the round ends." },
	{ "game.firefight_objective_data_link", "DATA LINK", "A hill turns on: hold it with no enemy in it. Needs King of the Hill hills." },
	{ "game.firefight_objective_flag", "FLAG", "A flag turns on: carry it to the other base. Every enemy hunts its carrier. Needs 2 flags." },
	{ "game.firefight_objective_rescue", "RESCUE", "Marines under attack: reach them in time, clear the attackers, and they follow you." },
	{ "game.firefight_objective_oddball", "ODDBALL", "A boss drops a skull: hold it while every enemy hunts you. Needs an oddball spawn." },
	{ "game.firefight_objective_collect", "COLLECT", "Kills drop skulls: carry them to a hill or a flag base. Die and you drop all you carry." },
	{ "game.firefight_objective_bomb", "BOMB", "Carry the bomb to a far target and plant it, then defend it so it isn't disarmed." } };

struct ae_ffm_objective const *ae_ffm_objective(int round, int kind)
{
	if (round == AE_FFM_WAVE)
		return kind >= 0 && kind < AE_FFM_KINDS_WAVE ? &wave_rows[kind] : NULL;
	if (round == AE_FFM_ROUND)
		return kind >= 0 && kind < AE_FFM_KINDS_ROUND ? &round_rows[kind] : NULL;
	return NULL;
}

int ae_ffm_objective_count(int round)
{
	return round == AE_FFM_WAVE ? AE_FFM_KINDS_WAVE : round == AE_FFM_ROUND ? AE_FFM_KINDS_ROUND : 0;
}

const char *ae_ffm_objectives_key(int round)
{
	return round == AE_FFM_ROUND ? "game.firefight_objectives_set" : "game.firefight_objectives_round";
}

const char *ae_ffm_chance_key(int round)
{
	return round == AE_FFM_ROUND ? "game.firefight_objectives_set_chance" : "game.firefight_objectives_round_chance";
}

long ae_ffm_chance_percent(int index)
{
	return index < 0 ? 10 : index >= AE_FFM_CHANCES ? 100 : 10L * (index + 1);
}

int ae_ffm_chance_index(long percent)
{
	long index = (percent + 5) / 10 - 1;

	return index < 0 ? 0 : index >= AE_FFM_CHANCES ? AE_FFM_CHANCES - 1 : (int)index;
}

const char *ae_ffm_objectives_value(int wave_on, int round_on)
{
	return wave_on && round_on ? "BOTH" : wave_on ? "WAVE" : round_on ? "ROUND" : "OFF";
}

const char *ae_ffm_objectives_help(int round, int on)
{
	if (round == AE_FFM_ROUND)
		return on ? "A round can have an objective of those on below, from its 1st or 2nd wave to its end." :
			"No round objectives: Firefight as it is.";
	return on ? "A wave can have an objective of those on below, done before it is cleared." : "No wave objectives: Firefight as it is.";
}

const char *ae_ffm_chance_help(int round)
{
	return round == AE_FFM_ROUND ? "How likely each round is to have an objective." : "How likely each wave is to have an objective.";
}

/* ---------- the lobby's rows */

static const int scales[AE_FFM_SCALES] = { 0, 5, 10, 20, 50 };
static const char *const scale_texts[AE_FFM_SCALES] = { "0%", "5%", "10%", "20%", "50%" };
const char ae_ffm_scale_help[] = "Extra enemies added per player from the 2nd player onwards (non-compounding from the base count).";

int ae_ffm_scale_percent(int index)
{
	return index >= 0 && index < AE_FFM_SCALES ? scales[index] : 0;
}

int ae_ffm_scale_index(int percent)
{
	int index, best = 0, distance = 1000;

	for (index = 0; index < AE_FFM_SCALES; index++)
	{
		int apart = percent > scales[index] ? percent - scales[index] : scales[index] - percent;

		if (apart < distance)
		{
			distance = apart;
			best = index;
		}
	}
	return best;
}

const char *ae_ffm_scale_text(int index)
{
	return index >= 0 && index < AE_FFM_SCALES ? scale_texts[index] : "";
}

static const char *const vehicle_names[AE_FFM_VEHICLE_MODES] = { "ALL AT SPAWN", "ORDNANCE REWARDS", "NONE" };
const char ae_ffm_vehicles_help[] = "The map's vehicles for the players: all from the start, one with each ordnance drop, or none.";

const char *ae_ffm_vehicles_name(int mode)
{
	return mode >= 0 && mode < AE_FFM_VEHICLE_MODES ? vehicle_names[mode] : "";
}

/* ---------- MAP CONTENT */

static const unsigned char kind_custom[AE_FFM_CONTENT_KINDS] = { 0x04, 0x08, 0x80 };
static const unsigned char kind_off[AE_FFM_CONTENT_KINDS] = { 0x10, 0x20, 0x40 };
static const char *const mode_names[AE_FFM_MODES] = { "VANILLA ONLY", "INCLUDE MAP", "ONLY MAP" };
static const char *const choice_names[AE_FFM_CHOICES] = { "ALL", "CUSTOM", "NONE" };
static const char *const kind_rows[AE_FFM_CONTENT_KINDS] = { "MAP ENEMY AI", "MAP WEAPONS", "MAP VEHICLES" };
static const char *const kind_list_rows[AE_FFM_CONTENT_KINDS] = { "CHOOSE ENEMY AI", "CHOOSE WEAPONS", "CHOOSE VEHICLES" };
static const char *const kind_helps[AE_FFM_CONTENT_KINDS] = {
	"The map's own enemy AI in the waves: all of them, those you choose (CUSTOM), or none.",
	"The map's own weapons in the drop pods and the enemies' hands: all, those you choose, or none.",
	"The map's own vehicles, the players' and the enemies': all, those you choose, or none." };
static const char *const kind_list_helps[AE_FFM_CONTENT_KINDS] = {
	"Turn each of the map's enemy AI on or off. Kept for this map for the next games.",
	"Turn each of the map's weapons on or off. Kept for this map for the next games.",
	"Turn each of the map's vehicles on or off. Kept for this map for the next games." };
const char ae_ffm_needs_content[] = "NEEDS USE CONTENT";

int ae_ffm_content_mode(unsigned char content)
{
	switch (content & AE_FFM_BIT_MODE)
	{
	case AE_FFM_MODE_INCLUDE_VALUE: return AE_FFM_MODE_INCLUDE;
	case AE_FFM_MODE_ONLY_VALUE: return AE_FFM_MODE_ONLY;
	}
	return AE_FFM_MODE_VANILLA;
}

unsigned char ae_ffm_content_with_mode(unsigned char content, int mode)
{
	unsigned char bits = mode == AE_FFM_MODE_INCLUDE ? AE_FFM_MODE_INCLUDE_VALUE : mode == AE_FFM_MODE_ONLY ? AE_FFM_MODE_ONLY_VALUE : 0;

	return (unsigned char)((content & ~AE_FFM_BIT_MODE) | bits);
}

int ae_ffm_content_choice(unsigned char content, int kind)
{
	if (kind < 0 || kind >= AE_FFM_CONTENT_KINDS)
		return AE_FFM_CHOICE_ALL;
	return (content & kind_off[kind]) ? AE_FFM_CHOICE_NONE : (content & kind_custom[kind]) ? AE_FFM_CHOICE_CUSTOM : AE_FFM_CHOICE_ALL;
}

unsigned char ae_ffm_content_with_choice(unsigned char content, int kind, int choice)
{
	if (kind < 0 || kind >= AE_FFM_CONTENT_KINDS)
		return content;
	content = (unsigned char)(content & ~(kind_off[kind] | kind_custom[kind]));
	if (choice == AE_FFM_CHOICE_NONE)
		content = (unsigned char)(content | kind_off[kind]);
	else if (choice == AE_FFM_CHOICE_CUSTOM)
		content = (unsigned char)(content | kind_custom[kind]);
	return content;
}

int ae_ffm_has_kind(unsigned char has, int kind)
{
	return kind >= 0 && kind < AE_FFM_CONTENT_KINDS && (has & (1 << kind)) != 0;
}

const char *ae_ffm_mode_name(int mode)
{
	return mode >= 0 && mode < AE_FFM_MODES ? mode_names[mode] : "";
}

const char *ae_ffm_choice_name(int choice)
{
	return choice >= 0 && choice < AE_FFM_CHOICES ? choice_names[choice] : "";
}

const char *ae_ffm_kind_row(int kind)
{
	return kind >= 0 && kind < AE_FFM_CONTENT_KINDS ? kind_rows[kind] : "";
}

const char *ae_ffm_kind_list_row(int kind)
{
	return kind >= 0 && kind < AE_FFM_CONTENT_KINDS ? kind_list_rows[kind] : "";
}

const char *ae_ffm_kind_help(int kind)
{
	return kind >= 0 && kind < AE_FFM_CONTENT_KINDS ? kind_helps[kind] : "";
}

const char *ae_ffm_kind_list_help(int kind)
{
	return kind >= 0 && kind < AE_FFM_CONTENT_KINDS ? kind_list_helps[kind] : "";
}

const char *ae_ffm_mode_help(void)
{
	return "VANILLA ONLY: the game's own enemies and weapons. INCLUDE MAP: the map's beside them. ONLY MAP: the map's in their place.";
}

const char *ae_ffm_loadout_help(void)
{
	return "Spawn with the map's own weapons in place of the game's loadout. Kept for this map.";
}

static const char *const health_rows[AE_FFM_HEALTH_RULES] = { "SHIELDS", "SHIELD REGEN", "HEALTH REGEN" };
static const char *const health_maps_on_off[3] = { "MAP'S", "ON", "OFF" };
static const char *const health_off_on[2] = { "OFF", "ON" };
static const char *const health_helps[AE_FFM_HEALTH_RULES] = {
	"This map's players have their own shield rules. ON gives shields, OFF takes them. Kept for this map.",
	"ON: shields come back as in Halo. OFF: never (only the map's pickups). Kept for this map.",
	"ON: health comes back after 5 seconds unhurt, as a shield does. Kept for this map." };

int ae_ffm_health_choices(int rule)
{
	return rule == AE_FFM_HEALTH_REGEN ? 2 : rule >= 0 && rule < AE_FFM_HEALTH_REGEN ? 3 : 0;
}

const char *ae_ffm_health_row(int rule)
{
	return rule >= 0 && rule < AE_FFM_HEALTH_RULES ? health_rows[rule] : "";
}

const char *ae_ffm_health_choice(int rule, int choice)
{
	if (choice < 0 || choice >= ae_ffm_health_choices(rule))
		return "";
	return rule == AE_FFM_HEALTH_REGEN ? health_off_on[choice] : health_maps_on_off[choice];
}

const char *ae_ffm_health_help(int rule)
{
	return rule >= 0 && rule < AE_FFM_HEALTH_RULES ? health_helps[rule] : "";
}

void ae_ffm_content_summary(unsigned char has, unsigned char content, int health, int health_changed, int loadout_set, char *text,
	size_t size)
{
	static const char *const names[AE_FFM_CONTENT_KINDS] = { "AI", "WEAPONS", "VEHICLES" };
	int mode = ae_ffm_content_mode(content), kind, any = 0;
	size_t used;

	if (!text || size == 0)
		return;
	text[0] = 0;
	if (!has)
	{
		if (health)
			snprintf(text, size, "HEALTH RULES: %s", health_changed ? "CHANGED" : "MAP'S");
		return;
	}
	if (mode == AE_FFM_MODE_VANILLA)
	{
		snprintf(text, size, "VANILLA ONLY%s%s", ae_ffm_has_kind(has, AE_FFM_KIND_WEAPONS) && loadout_set ? ", LOADOUT" : "",
			health && health_changed ? ", HEALTH" : "");
		return;
	}
	snprintf(text, size, "%s:", mode == AE_FFM_MODE_ONLY ? "ONLY MAP" : "INCLUDE MAP");
	for (kind = 0; kind < AE_FFM_CONTENT_KINDS; kind++)
	{
		int choice = ae_ffm_content_choice(content, kind);

		if (!ae_ffm_has_kind(has, kind) || choice == AE_FFM_CHOICE_NONE)
			continue;
		used = strlen(text);
		snprintf(text + used, size - used, "%s%s%s", any ? ", " : " ", names[kind], choice == AE_FFM_CHOICE_CUSTOM ? "*" : "");
		any = 1;
	}
	if (!any)
		snprintf(text, size, "VANILLA ONLY");
	if (ae_ffm_has_kind(has, AE_FFM_KIND_WEAPONS) && loadout_set)
	{
		used = strlen(text);
		snprintf(text + used, size - used, ", LOADOUT");
	}
	if (health && health_changed)
	{
		used = strlen(text);
		snprintf(text + used, size - used, ", HEALTH");
	}
}

void ae_ffm_tag_label(const char *tag_name, char *text, size_t size)
{
	const char *leaf = tag_name ? tag_name : "", *slash;
	size_t at;

	if (!text || size == 0)
		return;
	while ((slash = strchr(leaf, '\\')) != NULL || (slash = strchr(leaf, '/')) != NULL)
		leaf = slash + 1;
	for (at = 0; leaf[at] && at < size - 1; at++)
		text[at] = leaf[at] >= 'a' && leaf[at] <= 'z' ? (char)(leaf[at] - 32) : leaf[at];
	text[at] = 0;
}
