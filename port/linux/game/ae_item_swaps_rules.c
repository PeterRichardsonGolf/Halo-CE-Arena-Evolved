/*
AE_ITEM_SWAPS_RULES.C

Campaign swaps and the campaign lobby's ITEM SETS picker, decided without drawing (ae_item_swaps_rules.h). The values a swap's rows step
through are the keys Content Evolved's campaign profile file keeps, so a swap made here reads there and the other way round.
*/

#include <stdio.h>
#include <string.h>
#include "ae_item_swaps_rules.h"

#define DOT "\xE2\x80\xA2"

static void copy(char *out, int size, char const *text)
{
	snprintf(out, (size_t)size, "%s", text);
}

/* ---------- limits */

char const *ae_is_limit_word(int what, long count)
{
	if (what == 0)
		return count >= AE_IS_LIMIT_SETS ? "64 SETS: THE MOST" : "";
	if (what == 1)
		return count >= AE_IS_LIMIT_ITEMS ? "256 ITEMS: THE MOST" : "";
	return count >= AE_IS_LIMIT_SWAPS ? "64 SWAPS: THE MOST" : "";
}

/* ---------- a swap in the list */

int ae_is_swap_state(int in_use, int set_found, long items_on)
{
	if (!set_found)
		return AE_IS_SWAP_SET_MISSING;
	if (items_on <= 0)
		return AE_IS_SWAP_NOTHING_ON;
	return in_use ? AE_IS_SWAP_IN_USE : AE_IS_SWAP_OK;
}

char const *ae_is_swap_state_word(int state)
{
	switch (state)
	{
	case AE_IS_SWAP_IN_USE: return "IN USE";
	case AE_IS_SWAP_SET_MISSING: return "SET MISSING";
	case AE_IS_SWAP_NOTHING_ON: return "NOTHING ON";
	}
	return "";
}

int ae_is_swap_state_tone(int state)
{
	return state == AE_IS_SWAP_IN_USE ? 2 : state == AE_IS_SWAP_OK ? 0 : 3;
}

void ae_is_swap_sub(int set_found, char const *set_name, char const *weapon_replace, char const *enemy_replace, char *out, int size)
{
	if (!set_found)
	{
		copy(out, size, "Its set is gone");
		return;
	}
	snprintf(out, (size_t)size, "%s " DOT " weapons %s " DOT " enemies %s", set_name, weapon_replace && !strcmp(weapon_replace, "manual") ?
		"by weapon" : "at random", enemy_replace && !strcmp(enemy_replace, "manual") ? "by enemy" : "at random");
}

char const *ae_is_swaps_count_right(void)
{
	return "PICK ONE IN THE CAMPAIGN LOBBY";
}

char const *ae_is_swaps_empty_words(void)
{
	return "A swap plays one of your sets in the campaign's missions in the randomizer's place. Make one to choose what the set's items take the place of.";
}

/* ---------- a swap's page */

int ae_is_swap_rows(int by_weapon, int by_enemy, int has_biped, struct ae_is_swap_row rows[AE_IS_SWAP_ROWS_MOST])
{
	int count = 0, each;

	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_NAME, 0 };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_SET, 0 };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_WEAPON_ROT, 0 };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_WEAPON_PICK, 0 };
	for (each = 0; by_weapon && each < AE_IS_SWAP_WEAPON_SLOTS; each++)
		rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_WEAPON_SLOT, each };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_ENEMY_ROT, 0 };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_ENEMY_PICK, 0 };
	for (each = 0; by_enemy && each < AE_IS_SWAP_ENEMY_SLOTS; each++)
		rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_ENEMY_SLOT, each };
	rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_BIPED, 0 };
	if (has_biped)
	{
		rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_HANDS, 0 };
		rows[count++] = (struct ae_is_swap_row){ AE_IS_SW_MOVEMENT, 0 };
	}
	return count;
}

int ae_is_swap_row_section(int row)
{
	switch (row)
	{
	case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_WEAPON_PICK: case AE_IS_SW_WEAPON_SLOT: return AE_IS_SWS_WEAPONS;
	case AE_IS_SW_ENEMY_ROT: case AE_IS_SW_ENEMY_PICK: case AE_IS_SW_ENEMY_SLOT: return AE_IS_SWS_ENEMIES;
	case AE_IS_SW_BIPED: case AE_IS_SW_HANDS: case AE_IS_SW_MOVEMENT: return AE_IS_SWS_BODY;
	}
	return AE_IS_SWS_NONE;
}

char const *ae_is_swap_section_name(int section)
{
	static char const *const names[4] = { "", "WEAPONS", "ENEMIES", "YOUR BODY" };

	return section >= 0 && section < 4 ? names[section] : "";
}

char const *ae_is_swap_row_label(int row)
{
	static char const *const labels[AE_IS_SW_ROWS] = { "NAME", "ITEM SET", "THE MISSION'S WEAPONS", "HOW THEY'RE PICKED", "",
		"THE MISSION'S ENEMIES", "HOW THEY'RE PICKED", "", "BIPED", "HANDS", "MOVEMENT" };

	return row >= 0 && row < AE_IS_SW_ROWS ? labels[row] : "";
}

/* the rotations and the pickings, with the keys of Content Evolved's campaign profile ("add" / "only", "random" / "manual") */
static char const *const rotation_words[2] = { "MIXED IN", "ALL SWAPPED" };
static char const *const rotation_keys[2] = { "add", "only" };
static char const *const weapon_pick_words[2] = { "AT RANDOM", "BY WEAPON" };
static char const *const enemy_pick_words[2] = { "AT RANDOM", "BY ENEMY" };
static char const *const pick_keys[2] = { "random", "manual" };
static char const *const movement_words[2] = { "SPARTAN", "ITS OWN" };
static char const *const movement_keys[2] = { "", "own" };

int ae_is_swap_value_count(int row)
{
	switch (row)
	{
	case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_WEAPON_PICK: case AE_IS_SW_ENEMY_ROT: case AE_IS_SW_ENEMY_PICK: case AE_IS_SW_MOVEMENT:
		return 2;
	}
	return 0;
}

char const *ae_is_swap_value_word(int row, int index)
{
	if (index < 0 || index > 1)
		return "";
	switch (row)
	{
	case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_ENEMY_ROT: return rotation_words[index];
	case AE_IS_SW_WEAPON_PICK: return weapon_pick_words[index];
	case AE_IS_SW_ENEMY_PICK: return enemy_pick_words[index];
	case AE_IS_SW_MOVEMENT: return movement_words[index];
	}
	return "";
}

char const *ae_is_swap_value_key(int row, int index)
{
	if (index < 0 || index > 1)
		return "";
	switch (row)
	{
	case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_ENEMY_ROT: return rotation_keys[index];
	case AE_IS_SW_WEAPON_PICK: case AE_IS_SW_ENEMY_PICK: return pick_keys[index];
	case AE_IS_SW_MOVEMENT: return movement_keys[index];
	}
	return "";
}

int ae_is_swap_value_index(int row, char const *key)
{
	int each;

	for (each = 1; key && key[0] && each < ae_is_swap_value_count(row); each++)
		if (!strcmp(key, ae_is_swap_value_key(row, each)))
			return each;
	return 0;
}

char const *ae_is_swap_row_file_key(int row)
{
	switch (row)
	{
	case AE_IS_SW_NAME: return "name";
	case AE_IS_SW_SET: return "profile";
	case AE_IS_SW_WEAPON_ROT: return "weapon_rotation";
	case AE_IS_SW_WEAPON_PICK: return "weapon_replace";
	case AE_IS_SW_ENEMY_ROT: return "enemy_rotation";
	case AE_IS_SW_ENEMY_PICK: return "enemy_replace";
	case AE_IS_SW_BIPED: return "player_biped";
	case AE_IS_SW_HANDS: return "player_hands";
	case AE_IS_SW_MOVEMENT: return "player_movement";
	}
	return "";
}

char const *ae_is_swap_slot_section(int row, int loadout)
{
	if (row == AE_IS_SW_WEAPON_SLOT)
		return "weapons";
	if (row == AE_IS_SW_ENEMY_SLOT)
		return loadout ? "loadout" : "enemies";
	return "";
}

int ae_is_swap_row_change(int row)
{
	switch (row)
	{
	case AE_IS_SW_NAME: return 2;
	case AE_IS_SW_WEAPON_SLOT: case AE_IS_SW_ENEMY_SLOT: return 3;
	case AE_IS_SW_SET: case AE_IS_SW_BIPED: case AE_IS_SW_HANDS: case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_WEAPON_PICK:
	case AE_IS_SW_ENEMY_ROT: case AE_IS_SW_ENEMY_PICK: case AE_IS_SW_MOVEMENT: return 1;
	}
	return 0;
}

char const *ae_is_swap_row_card_text(int row)
{
	switch (row)
	{
	case AE_IS_SW_NAME: return "What the swap is called in the lobby and in this list.";
	case AE_IS_SW_SET: return "The set whose weapons and enemies the swap plays. Only sets with an item on are offered.";
	case AE_IS_SW_WEAPON_ROT: return "MIXED IN keeps the mission's own weapon among the ones picked for it. ALL SWAPPED always uses a picked one.";
	case AE_IS_SW_WEAPON_PICK: return "AT RANDOM gives every weapon in a mission one of the set's weapons. BY WEAPON lets you pick, for each of the campaign's weapons, what it becomes.";
	case AE_IS_SW_ENEMY_ROT: return "MIXED IN keeps the mission's own enemies among the ones picked for them. ALL SWAPPED always uses a picked one.";
	case AE_IS_SW_ENEMY_PICK: return "AT RANDOM swaps the mission's enemies for the set's. BY ENEMY lets you pick what each of the campaign's enemies becomes and what it carries.";
	case AE_IS_SW_BIPED: return "The body you and split-screen players start every mission as while the swap plays. It wins over the set's own PLAY AS IT.";
	case AE_IS_SW_HANDS: return "The first-person hands you see with that body.";
	case AE_IS_SW_MOVEMENT: return "SPARTAN moves as the campaign's player does. ITS OWN moves as the body's own movement.";
	case AE_IS_SW_WEAPON_SLOT: return "What this weapon of the mission becomes.";
	case AE_IS_SW_ENEMY_SLOT: return "What this enemy of the mission becomes, and what it carries.";
	}
	return "";
}

char const *ae_is_swap_row_help(int row)
{
	switch (row)
	{
	case AE_IS_SW_NAME: return "A types the swap's name.";
	case AE_IS_SW_SET: return "Left or right picks the set the swap plays.";
	case AE_IS_SW_WEAPON_ROT: case AE_IS_SW_ENEMY_ROT: return "Left or right changes it. MIXED IN keeps the mission's own among the picks.";
	case AE_IS_SW_WEAPON_PICK: return "Left or right changes it. BY WEAPON shows a row for each of the campaign's weapons.";
	case AE_IS_SW_ENEMY_PICK: return "Left or right changes it. BY ENEMY shows a row for each of the campaign's enemies.";
	case AE_IS_SW_BIPED: return "Left or right picks your body from the set's bipeds that fit a player.";
	case AE_IS_SW_HANDS: return "Left or right picks the hands. ITS OWN takes the set's hands whose path is nearest the body's.";
	case AE_IS_SW_MOVEMENT: return "Left or right changes it.";
	case AE_IS_SW_WEAPON_SLOT: return "A picks the weapons it becomes. AS IT IS leaves the mission's own.";
	case AE_IS_SW_ENEMY_SLOT: return "A picks what it becomes and what it carries. AS IT IS leaves the mission's own.";
	}
	return "";
}

/* ---------- a slot */

void ae_is_slot_value(int picked, char *out, int size)
{
	if (picked <= 0)
		copy(out, size, "AS IT IS");
	else
		snprintf(out, (size_t)size, "%d PICKED", picked);
}

void ae_is_slot_count_line(int picked, char *out, int size)
{
	if (picked <= 0)
		copy(out, size, "AS IT IS");
	else if (picked == 1)
		copy(out, size, "1 PICKED");
	else
		snprintf(out, (size_t)size, "%d PICKED " DOT " ONE AT RANDOM EACH TIME", picked);
}

char const *ae_is_slot_replaced_head(int weapon)
{
	return weapon ? "BECOMES" : "REPLACED BY";
}

char const *ae_is_slot_armed_head(void)
{
	return "ARMED WITH";
}

char const *ae_is_slot_own_word(int section)
{
	return section == 0 ? "THE MISSION'S OWN" : "THEIR OWN";
}

char const *ae_is_slot_card_text(int weapon, char const *name, int mixed_in)
{
	static char text[240];

	if (weapon)
		snprintf(text, sizeof(text), "Each %s lying in a mission, or given to an AI, becomes one of the picks.%s", name,
			mixed_in ? " MIXED IN keeps the original among them." : "");
	else
		snprintf(text, sizeof(text), "Each %s in a mission becomes one of the picks, and carries one of the weapons picked for it.%s", name,
			mixed_in ? " MIXED IN keeps the mission's own among them." : "");
	return text;
}

char const *ae_is_slot_help(int weapon)
{
	return weapon ? "A or left / right turns a pick on or off. Nothing picked leaves the weapon as it is." :
		"A or left / right turns a pick on or off. Nothing picked leaves the enemy as it is.";
}

char const *ae_is_slot_unarmed_words(void)
{
	return "It holds no weapons, so there is nothing to arm it with.";
}

/* ---------- the lobby's picker and row */

void ae_is_pick_writes(int pick, struct ae_is_pick_writes *writes)
{
	memset(writes, 0, sizeof(*writes));
	if (pick == AE_IS_PICK_SWAP)
	{
		/* (the swap's file is written; the randomizer's set stays as it was, which a playing swap passes over) */
		writes->swap_file = 1;
		writes->injection = 1;
	}
	else if (pick == AE_IS_PICK_SET)
	{
		writes->clear_swap = 1;
		writes->set_folder = 1;
		writes->injection = 1;
	}
	else
	{
		writes->clear_swap = 1;
		writes->clear_set = 1;
	}
}

char const *ae_is_pick_swap_wait(int state)
{
	return state == AE_IS_SWAP_SET_MISSING ? "SET MISSING" : state == AE_IS_SWAP_NOTHING_ON ? "NOTHING ON" : "";
}

char const *ae_is_pick_set_wait(long items_on, int randomizer_on, int mcc_mission)
{
	if (items_on <= 0)
		return "NO ITEMS ON";
	if (mcc_mission)
		return "NOT ON MCC MISSIONS";
	if (!randomizer_on)
		return "NEEDS RANDOMIZER";
	return "";
}

void ae_is_pick_swap_value(int state, char const *set_name, char *out, int size)
{
	if (state == AE_IS_SWAP_IN_USE)
		copy(out, size, "IN USE");
	else if (state == AE_IS_SWAP_OK)
		copy(out, size, set_name);
	else
		copy(out, size, ae_is_pick_swap_wait(state));
}

void ae_is_pick_set_value(int in_use, long items_on, char *out, int size)
{
	if (in_use)
		copy(out, size, "IN USE");
	else
		snprintf(out, (size_t)size, "%ld ITEMS ON", items_on);
}

char const *ae_is_pick_none_value(int in_use)
{
	return in_use ? "IN USE" : "THE CAMPAIGN'S OWN";
}

char const *ae_is_pick_none_help(void)
{
	return "{A} plays the mission with its own enemies and weapons.";
}

void ae_is_pick_swap_help(char const *swap_name, char const *set_name, char *out, int size)
{
	snprintf(out, (size_t)size, "{A} plays %s: %s's weapons and enemies take the mission's places, and the randomizer is passed over.",
		swap_name, set_name);
}

void ae_is_pick_set_help(char const *set_name, char *out, int size)
{
	snprintf(out, (size_t)size, "{A} plays the mission with %s's items: its weapons and actors join the randomizer's pools, and the pools get an ITEM SET choice.",
		set_name);
}

int ae_is_lobby_row_shown(int has_set_on, int daily, int online_coop)
{
	return has_set_on && !daily && !online_coop;
}

void ae_is_lobby_value(int pick, char const *name, char *out, int size)
{
	char upper[64];
	int each;

	for (each = 0; each < (int)sizeof(upper) - 1 && name && name[each]; each++)
		upper[each] = name[each] >= 'a' && name[each] <= 'z' ? (char)(name[each] - 32) : name[each];
	upper[each] = 0;
	if (pick == AE_IS_PICK_SWAP)
		copy(out, size, upper);
	else if (pick == AE_IS_PICK_SET)
		snprintf(out, (size_t)size, "%s " DOT " RANDOMIZER", upper);
	else
		copy(out, size, "NONE");
}

char const *ae_is_randomizer_wait(int swap_picked)
{
	return swap_picked ? "SWAP PLAYS INSTEAD" : "";
}

char const *ae_is_randomizer_wait_help(void)
{
	return "A campaign swap plays in the randomizer's place. Pick NONE or a set under ITEM SETS to use the randomizer.";
}

char const *ae_is_lobby_row_help(void)
{
	return "Play the mission with an item set, or a campaign swap of one: its weapons and enemies. {A} opens the list.";
}

/* ---------- the firing range row */

char const *ae_is_range_wait(long items_on, int range_map_ok)
{
	if (items_on <= 0)
		return "NOTHING ON";
	if (!range_map_ok)
		return "NEEDS BLOOD GULCH";
	return "";
}
