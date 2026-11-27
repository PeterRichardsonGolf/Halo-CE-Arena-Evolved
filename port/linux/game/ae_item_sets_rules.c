/*
AE_ITEM_SETS_RULES.C

ITEM SETS' rules (ae_item_sets_rules.h). Plain C.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ae_item_sets_rules.h"

#define SEPARATOR "\xE2\x80\xBA"
#define DOT "\xE2\x80\xA2"
#define ELLIPSIS "\xE2\x80\xA6"
#define DOT_SEP "\xE2\x80\xA2"

static char const *const kind_names[AE_IS_KINDS] = { "WEAPONS", "AI", "EQUIPMENT", "VEHICLES", "BIPEDS", "SCENERY",
	"DOORS & DEVICES", "SKIES", "FIRST PERSON HANDS" };
static char const *const kind_blurbs[AE_IS_KINDS] = { "What players and AI carry", "Enemies and allies, and how they fight",
	"Grenades, power-ups and pickups", "Driven, flown or ridden", "The bodies players and AI move in",
	"Rocks, crates, trees and props", "Doors, lifts, switches and machines", "The sky around the map", "The arms a player sees" };
static char const *const kind_words[AE_IS_KINDS] = { "WEAPON", "AI", "EQUIPMENT", "VEHICLE", "BIPED", "SCENERY", "DEVICE", "SKY",
	"HANDS" };

static int same(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

static void copy(char *to, int size, char const *from)
{
	if (size < 1)
		return;
	snprintf(to, (size_t)size, "%s", from ? from : "");
}

char const *ae_is_kind_name(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_names[kind] : "";
}

char const *ae_is_kind_blurb(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_blurbs[kind] : "";
}

char const *ae_is_kind_word(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_words[kind] : "";
}

static char const *last_slash(char const *path)
{
	char const *last = NULL;

	for (; path && *path; path++)
		if (*path == '\\' || *path == '/')
			last = path;
	return last;
}

void ae_is_item_name(char const *path, char *name, int size)
{
	char const *last = last_slash(path);

	copy(name, size, last ? last + 1 : path);
}

int ae_is_folder_count(char const *path)
{
	int count = 0;

	for (; path && *path; path++)
		if (*path == '\\' || *path == '/')
			count++;
	return count;
}

/* the nth folder of a path (0 first): its start and length */
static int folder_at(char const *path, int n, char const **start, int *length)
{
	char const *each = path, *from = path;
	int at = 0;

	for (; *each; each++)
	{
		if (*each == '\\' || *each == '/')
		{
			if (at == n)
			{
				*start = from;
				*length = (int)(each - from);
				return 1;
			}
			at++;
			from = each + 1;
		}
	}
	return 0;
}

static void append(char *to, int size, char const *text, int length)
{
	int used = (int)strlen(to);

	if (used + length >= size)
		length = size - used - 1;
	if (length > 0)
	{
		memcpy(to + used, text, (size_t)length);
		to[used + length] = 0;
	}
}

void ae_is_item_folders(char const *path, int keep, char *folders, int size)
{
	int count = ae_is_folder_count(path), each, first;
	char const *start;
	int length;

	if (size < 1)
		return;
	folders[0] = 0;
	if (count == 0)
		return;
	if (keep < 0 || keep >= count - 1)
	{
		for (each = 0; each < count; each++)
		{
			if (each)
				append(folders, size, " " SEPARATOR " ", 5);
			if (folder_at(path, each, &start, &length))
				append(folders, size, start, length);
		}
		return;
	}
	if (folder_at(path, 0, &start, &length))
		append(folders, size, start, length);
	append(folders, size, " " SEPARATOR " " ELLIPSIS, 9);
	first = count - keep;
	for (each = first; each < count; each++)
	{
		append(folders, size, " " SEPARATOR " ", 5);
		if (folder_at(path, each, &start, &length))
			append(folders, size, start, length);
	}
}

/* the character count of UTF-8 text */
static int glyphs(char const *text)
{
	int count = 0;

	for (; *text; text++)
		if (((unsigned char)*text & 0xC0) != 0x80)
			count++;
	return count;
}

void ae_is_middle_cut(char const *name, int chars, char *cut, int size)
{
	int length = (int)strlen(name), head, tail;

	if (glyphs(name) <= chars || chars < 5 || length >= 200)
	{
		copy(cut, size, name);
		return;
	}
	/* (names are ASCII: the engine's paths) */
	head = (chars - 1) / 2 + (chars - 1) % 2;
	tail = chars - 1 - head;
	if (size < head + tail + 4)
	{
		copy(cut, size, name);
		return;
	}
	memcpy(cut, name, (size_t)head);
	cut[head] = 0;
	append(cut, size, ELLIPSIS, 3);
	append(cut, size, name + length - tail, tail);
}

int ae_is_path_lines(char const *path, int chars, char lines[][96], int most)
{
	int count = ae_is_folder_count(path), each, used = 0, line = 0;
	char const *start;
	int length;

	if (most < 1)
		return 0;
	lines[0][0] = 0;
	/* (the folders then the item's own name as the last part) */
	for (each = 0; each <= count; each++)
	{
		char part[96];
		int piece;

		if (each < count && folder_at(path, each, &start, &length))
		{
			if (length > 90)
				length = 90;
			memcpy(part, start, (size_t)length);
			part[length] = 0;
		}
		else
			ae_is_item_name(path, part, sizeof(part));
		piece = glyphs(part) + (used ? 3 : 0);
		if (used && used + piece + 2 > chars && line + 1 < most)
		{
			append(lines[line], 96, " " SEPARATOR, 4);
			line++;
			lines[line][0] = 0;
			used = 0;
			piece = glyphs(part);
		}
		if (used)
			append(lines[line], 96, " " SEPARATOR " ", 5);
		append(lines[line], 96, part, (int)strlen(part));
		used += piece;
	}
	return line + 1;
}

int ae_is_set_name_ok(char const *name)
{
	int letters = 0;

	if (!name)
		return 0;
	for (; *name; name++)
	{
		unsigned char c = (unsigned char)*name;

		if (isalnum(c))
			letters++;
		else if (c != ' ' && c != '-' && c != '_')
			return 0;
	}
	return letters > 0;
}

static void size_text(unsigned long long bytes, char *text, int size)
{
	if (bytes >= 1024ull * 1024ull * 1024ull)
		snprintf(text, (size_t)size, "%llu.%llu GB", bytes / (1024ull * 1024ull * 1024ull),
			(bytes * 10ull / (1024ull * 1024ull * 1024ull)) % 10ull);
	else if (bytes >= 1024ull * 1024ull)
		snprintf(text, (size_t)size, "%llu MB", (bytes + 512ull * 1024ull) / (1024ull * 1024ull));
	else
		snprintf(text, (size_t)size, "%llu KB", (bytes + 512ull) / 1024ull);
}

void ae_is_set_line(long items, unsigned long long bytes, long maps, char *text, int size)
{
	char megabytes[24];

	if (items <= 0)
	{
		copy(text, size, "No items yet");
		return;
	}
	size_text(bytes, megabytes, sizeof(megabytes));
	snprintf(text, (size_t)size, "%ld item%s " DOT " %s " DOT " from %ld map%s", items, items == 1 ? "" : "s", megabytes, maps,
		maps == 1 ? "" : "s");
}

void ae_is_set_state(int playing, long items, unsigned long long bytes, char *text, int size)
{
	char megabytes[24];

	size_text(bytes, megabytes, sizeof(megabytes));
	snprintf(text, (size_t)size, "%s " DOT " %ld ITEM%s " DOT " %s", playing ? "PLAYING" : "OFF", items, items == 1 ? "" : "S",
		megabytes);
}

static int starts(char const *text, char const *prefix)
{
	return !strncmp(text, prefix, strlen(prefix));
}

void ae_is_reason_words(char const *engine, char *text, int size)
{
	if (!engine || !engine[0])
		copy(text, size, "");
	else if (starts(engine, "It has no first-person"))
		copy(text, size, "It has no first-person model or animations: its map never lets a player hold it, so it can't be added.");
	else if (starts(engine, "It names a tag this build cannot take out: "))
		snprintf(text, (size_t)size, "It uses a part this version can't take out: %s.", engine + strlen("It names a tag this build cannot take out: "));
	else if (starts(engine, "Make a profile"))
		copy(text, size, "Make a set first.");
	else if (starts(engine, "Out of memory"))
		copy(text, size, "Not enough memory.");
	else if (starts(engine, "The map cannot be read again"))
		copy(text, size, "The map can't be read again: open it again.");
	else if (starts(engine, "The profile cannot be written"))
		copy(text, size, "The set can't be written: the log says why.");
	else
		snprintf(text, (size_t)size, "It can't be added: %s", engine);
}

void ae_is_failed_words(char const *failed, char *text, int size)
{
	if (!failed || !failed[0])
		copy(text, size, "");
	else if (same(failed, "too large"))
		copy(text, size, "The last level couldn't play it, so it was turned off: with the items before it, it was more than a level can add.");
	else if (same(failed, "missing"))
		copy(text, size, "Its file is missing from the set's folder, so it was turned off.");
	else if (same(failed, "unreadable"))
		copy(text, size, "Its file couldn't be read, so it was turned off.");
	else if (same(failed, "unplayable"))
		copy(text, size, "This version can't play it, so it was turned off.");
	else if (same(failed, "no memory"))
		copy(text, size, "The last level had no memory for it, so it was turned off.");
	else if (same(failed, "not holdable"))
		copy(text, size, "A weapon it adds has no first-person model or animations: no player can hold it. It was turned off.");
	else if (same(failed, "bitmaps"))
		copy(text, size, "A picture it uses can't be drawn by this version, so it was turned off.");
	else if (same(failed, "models"))
		copy(text, size, "A model it uses can't be made for this version, so it was turned off.");
	else
		copy(text, size, "The last level couldn't play it, so it was turned off.");
}

void ae_is_failed_todo(char const *failed, char *text, int size)
{
	if (failed && same(failed, "too large"))
		copy(text, size, "Turn off items this set can do without, then A turns it on to try again. Y removes it.");
	else
		copy(text, size, "A turns it on to try again. Y removes it from the set.");
}

int ae_is_plays_in(int game, int randomizer_on)
{
	return game == AE_IS_GAME_CAMPAIGN_ALONE && randomizer_on != 0;
}

char const *ae_is_plays_words(int randomizer_on)
{
	return randomizer_on ? "While it plays, the randomizer can use its items in the campaign levels you play alone. Changes play from the next level." :
		"It plays through the randomizer, which is off: turn it on in the campaign's setup. Changes play from the next level.";
}

char const *ae_is_others_words(void)
{
	return "Multiplayer, online and co-op games never use it.";
}

/* ---------- an item's settings page */

static char const *const row_labels[AE_IS_ROWS] = { "NAME", "ON", "FACTION", "WEAPON CLASS", "COUNTS AS", "GROUP NAME", "HUD DRAWN AS",
	"TAKES THE CAMPAIGN'S PLACE", "PLAY AS IT", "HANDS", "MOVEMENT", "REMOVE FROM THE SET" };

int ae_is_setting_rows(int kind, int replace_known, int play_as_on, int rows[AE_IS_ROWS])
{
	int count = 0;

	rows[count++] = AE_IS_ROW_NAME;
	rows[count++] = AE_IS_ROW_ON;
	if (kind == 0)
	{
		rows[count++] = AE_IS_ROW_FACTION;
		rows[count++] = AE_IS_ROW_CLASS;
		rows[count++] = AE_IS_ROW_GROUP;
		rows[count++] = AE_IS_ROW_HUD;
	}
	else if (kind == 1)
	{
		rows[count++] = AE_IS_ROW_FACTION;
		rows[count++] = AE_IS_ROW_COUNTS_AS;
		rows[count++] = AE_IS_ROW_GROUP;
	}
	else if (kind == 4)
	{
		rows[count++] = AE_IS_ROW_PLAY_AS;
		if (play_as_on)
		{
			rows[count++] = AE_IS_ROW_HANDS;
			rows[count++] = AE_IS_ROW_MOVEMENT;
		}
	}
	if (replace_known)
		rows[count++] = AE_IS_ROW_REPLACE;
	rows[count++] = AE_IS_ROW_REMOVE;
	return count;
}

int ae_is_row_section(int row)
{
	switch (row)
	{
	case AE_IS_ROW_FACTION: case AE_IS_ROW_CLASS: case AE_IS_ROW_COUNTS_AS: case AE_IS_ROW_GROUP: return AE_IS_SECTION_RANDOMIZER;
	case AE_IS_ROW_HUD: case AE_IS_ROW_REPLACE: return AE_IS_SECTION_LEVEL;
	case AE_IS_ROW_PLAY_AS: case AE_IS_ROW_HANDS: case AE_IS_ROW_MOVEMENT: return AE_IS_SECTION_BODY;
	case AE_IS_ROW_REMOVE: return AE_IS_SECTION_SET;
	}
	return AE_IS_SECTION_NONE;
}

char const *ae_is_row_label(int row)
{
	return row >= 0 && row < AE_IS_ROWS ? row_labels[row] : "";
}

struct value_list { char const *const *words; char const *const *keys; int count; };

static char const *const faction_weapon_words[] = { "AUTO", "COVENANT", "HUMAN" };
static char const *const faction_weapon_keys[] = { "", "covenant", "human" };
static char const *const faction_ai_words[] = { "AUTO", "COVENANT", "FLOOD", "HUMAN", "ANY POOL ONLY" };
static char const *const faction_ai_keys[] = { "", "covenant", "flood", "human", "any" };
static char const *const class_words[] = { "AUTO", "PRECISION", "AUTOMATIC", "POWER", "NONE" };
static char const *const class_keys[] = { "", "precision", "automatic", "power", "none" };
static char const *const counts_words[] = { "AUTO", "GRUNT", "ELITE", "JACKAL", "HUNTER", "FLOOD COMBAT", "FLOOD CARRIER", "MARINE",
	"ENGINEER", "SENTINEL", "NONE" };
static char const *const counts_keys[] = { "", "grunt", "elite", "jackal", "hunter", "flood_combat", "flood_carrier", "marine",
	"engineer", "sentinel", "none" };
/* (its own first: a weapon with no setting keeps its HUD; AUTO picks a campaign weapon's by the words of its name) */
static char const *const hud_words[] = { "ITS OWN", "AUTO", "ASSAULT RIFLE", "PISTOL", "SNIPER RIFLE", "ROCKET LAUNCHER", "SHOTGUN",
	"FLAMETHROWER", "PLASMA RIFLE", "PLASMA PISTOL", "NEEDLER" };
static char const *const hud_keys[] = { "", "auto", "assault_rifle", "pistol", "sniper_rifle", "rocket_launcher", "shotgun",
	"flamethrower", "plasma_rifle", "plasma_pistol", "needler" };
static char const *const off_on_words[] = { "OFF", "ON" };
static char const *const off_on_keys[] = { "", "true" };
static char const *const movement_words[] = { "SPARTAN", "ITS OWN" };
static char const *const movement_keys[] = { "", "own" };

#define LIST(words, keys) { words, keys, (int)(sizeof(words) / sizeof((words)[0])) }

static int value_list(int row, int kind, struct value_list *list)
{
	static struct value_list const faction_weapon = LIST(faction_weapon_words, faction_weapon_keys),
		faction_ai = LIST(faction_ai_words, faction_ai_keys), classes = LIST(class_words, class_keys),
		counts = LIST(counts_words, counts_keys), huds = LIST(hud_words, hud_keys), off_on = LIST(off_on_words, off_on_keys),
		movement = LIST(movement_words, movement_keys);

	switch (row)
	{
	case AE_IS_ROW_FACTION: *list = kind == 1 ? faction_ai : faction_weapon; return 1;
	case AE_IS_ROW_CLASS: *list = classes; return 1;
	case AE_IS_ROW_COUNTS_AS: *list = counts; return 1;
	case AE_IS_ROW_HUD: *list = huds; return 1;
	case AE_IS_ROW_REPLACE: case AE_IS_ROW_PLAY_AS: *list = off_on; return 1;
	case AE_IS_ROW_MOVEMENT: *list = movement; return 1;
	}
	return 0;
}

int ae_is_value_count(int row, int kind)
{
	struct value_list list;

	return value_list(row, kind, &list) ? list.count : 0;
}

char const *ae_is_value_word(int row, int kind, int index)
{
	struct value_list list;

	return value_list(row, kind, &list) && index >= 0 && index < list.count ? list.words[index] : "";
}

char const *ae_is_value_key(int row, int kind, int index)
{
	struct value_list list;

	return value_list(row, kind, &list) && index >= 0 && index < list.count ? list.keys[index] : "";
}

int ae_is_value_index(int row, int kind, char const *key)
{
	struct value_list list;
	int each;

	if (!value_list(row, kind, &list) || !key)
		return 0;
	for (each = 0; each < list.count; each++)
		if (same(key, list.keys[each]))
			return each;
	/* (a setting that reads "true" or "false" in other words: the replace's) */
	return row == AE_IS_ROW_REPLACE && same(key, "on") ? 1 : 0;
}

int ae_is_value_step(int count, int index, int direction)
{
	if (count < 1)
		return 0;
	return ((index + direction) % count + count) % count;
}

int ae_is_typed_ok(char const *text)
{
	size_t length = 0;

	for (; text && *text; text++, length++)
	{
		unsigned char c = (unsigned char)*text;

		if (!isalnum(c) && c != ' ' && c != '-' && c != '_')
			return 0;
	}
	return length <= 31;
}

int ae_is_row_typed(int row)
{
	return row == AE_IS_ROW_NAME || row == AE_IS_ROW_GROUP;
}

char const *ae_is_row_card_text(int row)
{
	switch (row)
	{
	case AE_IS_ROW_NAME: return "The name the menus show it by. Empty: its own name from the map.";
	case AE_IS_ROW_ON: return "ON plays it. OFF keeps it in the set and leaves it out.";
	case AE_IS_ROW_FACTION: return "Which of the randomizer's side pools it joins. AUTO reads its name and folders as the randomizer "
		"reads the campaign's: \"pistol\" is human.";
	case AE_IS_ROW_CLASS: return "Which of the PRECISION, AUTOMATIC and POWER pools it joins.";
	case AE_IS_ROW_COUNTS_AS: return "Which of the campaign's enemy kinds it is among when a pool picks ONE TYPE.";
	case AE_IS_ROW_GROUP: return "Items with the same group name are played together when a pool's ONE TYPE picks ITEM SET.";
	case AE_IS_ROW_HUD: return "The crosshair, ammo meters and zoom it is drawn with, whoever carries it. It still fires and zooms as "
		"it did: only what is drawn changes.";
	case AE_IS_ROW_REPLACE: return "ON: wherever a mission has the item of this path, this one plays instead.";
	case AE_IS_ROW_PLAY_AS: return "ON: you and split-screen players start every mission as it, while the set plays. One body per set: "
		"turning it on here turns the set's other body off.";
	case AE_IS_ROW_HANDS: return "The arms you see in first person. ITS OWN picks the hands whose path shares the most folders with the biped's.";
	case AE_IS_ROW_MOVEMENT: return "SPARTAN moves as the campaign's player does. ITS OWN moves as the biped's own animations do.";
	case AE_IS_ROW_REMOVE: return "Takes it out of the set. The map it came from still has it.";
	}
	return "";
}

char const *ae_is_hands_word(int index)
{
	return index == 0 ? "ITS OWN" : index == 1 ? "THE CAMPAIGN'S OWN" : "";
}

void ae_is_hands_key(char const *path, char const *map, char *key, int size)
{
	snprintf(key, size > 0 ? (size_t)size : 0, "%s#%s", path ? path : "", map ? map : "");
}

/* ---------- the body */

int ae_is_body_source(int game, int randomizer_on, int set_playing, int set_has_body, int swap_playing, int swap_has_body)
{
	if (game != AE_IS_GAME_CAMPAIGN_ALONE)
		return AE_IS_BODY_NONE;
	if (swap_playing)
		return swap_has_body ? AE_IS_BODY_SWAP : AE_IS_BODY_NONE;
	return set_playing && randomizer_on && set_has_body ? AE_IS_BODY_SET : AE_IS_BODY_NONE;
}

int ae_is_hands_choice(char const *kept)
{
	if (!kept || !kept[0])
		return AE_IS_HANDS_OWN;
	return !strcmp(kept, "campaign") ? AE_IS_HANDS_CAMPAIGN : AE_IS_HANDS_CHOSEN;
}

int ae_is_movement_own(char const *kept)
{
	return kept && !strcmp(kept, "own");
}

int ae_is_body_layer(int source, int biped_in_level, int fits)
{
	if (source == AE_IS_BODY_NONE)
		return AE_IS_LAYER_LEAVE;
	return biped_in_level && fits ? AE_IS_LAYER_BODY : AE_IS_LAYER_CLEAR;
}

int ae_is_body_is(char const *kept, char const *path)
{
	if (!kept || !kept[0] || !path || !path[0])
		return 0;
	for (; *kept && *path; kept++, path++)
	{
		char a = *kept == '/' ? '\\' : *kept, b = *path == '/' ? '\\' : *path;

		if (tolower((unsigned char)a) != tolower((unsigned char)b))
			return 0;
	}
	return *kept == *path;
}

int ae_is_fit_from_facts(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, struct ae_is_fit *fit)
{
	memset(fit, 0, sizeof(*fit));
	fit->known = stock != NULL && biped != NULL;
	if (!fit->known)
		return 0;
	fit->fits = ae_biped_pack_fitness(stock, biped, fit->reason, (int)sizeof(fit->reason));
	fit->size_percent = stock->standing_height > 0.0f ? (int)(biped->standing_height / stock->standing_height * 100.0f + 0.5f) : 0;
	fit->physics = biped->uses_player_physics != 0;
	fit->no_graph = !biped->has_animation_graph;
	fit->measured = 1;
	fit->weapons_total = biped->labels_checked;
	fit->weapons_held = biped->labels_checked - biped->labels_missing;
	fit->seats_total = biped->seats_stock;
	fit->seats_have = biped->seats_stock - biped->seats_missing;
	return fit->fits;
}

int ae_is_fit_reason_lines(char const *reason, char lines[][160], int most)
{
	int count = 0;

	while (reason && *reason && count < most)
	{
		char const *end = strchr(reason, ';');
		size_t length = end ? (size_t)(end - reason) : strlen(reason);

		while (length && *reason == ' ')
		{
			reason++;
			length--;
		}
		if (length)
		{
			if (length > 156)
				length = 156;
			memcpy(lines[count], reason, length);
			lines[count][length] = 0;
			lines[count][0] = (char)toupper((unsigned char)lines[count][0]);
			strcat(lines[count], ".");
			count++;
		}
		reason = end ? end + 1 : reason + strlen(reason);
	}
	return count;
}

/* ---------- AUTO's answers */

/* (the randomizer's words, kept the same: tools/test_ae_units.py reads both) */
static char const *const covenant_words[] = { "covenant", "plasma", "needle", "carbine", "fuel rod", "sword", "brute", "spike", "elite", "beam", NULL };
static char const *const precision_words[] = { "sniper", "pistol", "carbine", "battle rifle", "battlerifle", "dmr", "magnum", "beam rifle", NULL };
static char const *const power_words[] = { "rocket", "fuel", "sword", "shotgun", "launcher", "cannon", "hammer", "flame", "laser", "grenade", "mortar", NULL };
static char const *const automatic_words[] = { "assault", "smg", "rifle", "needler", "spiker", "machine", "repeater", "mauler", NULL };
/* (the first word found chooses a stock HUD) */
static struct { char const *word, *value, *shown; } const auto_hud_words[] = {
	{ "sniper", "sniper_rifle", "THE SNIPER RIFLE'S" }, { "beam", "sniper_rifle", "THE SNIPER RIFLE'S" }, { "needle", "needler", "THE NEEDLER'S" },
	{ "rocket", "rocket_launcher", "THE ROCKET LAUNCHER'S" }, { "launcher", "rocket_launcher", "THE ROCKET LAUNCHER'S" },
	{ "fuel rod", "rocket_launcher", "THE ROCKET LAUNCHER'S" }, { "shotgun", "shotgun", "THE SHOTGUN'S" }, { "mauler", "shotgun", "THE SHOTGUN'S" },
	{ "flame", "flamethrower", "THE FLAMETHROWER'S" }, { "plasma pistol", "plasma_pistol", "THE PLASMA PISTOL'S" },
	{ "plasma rifle", "plasma_rifle", "THE PLASMA RIFLE'S" }, { "pistol", "pistol", "THE PISTOL'S" }, { "magnum", "pistol", "THE PISTOL'S" } };
/* (the campaign's kinds of enemy, by words of a name: randomizer.c's classify, in its order) */
enum { AI_NONE, AI_FLOOD_COMBAT, AI_FLOOD_CARRIER, AI_FLOOD_INFECTION, AI_GRUNT, AI_JACKAL, AI_HUNTER, AI_ELITE, AI_SENTINEL, AI_MARINE, AI_ENGINEER };

static void lower_into(char *out, int size, char const *path, char const *name)
{
	int used = 0;
	char const *part;
	int pass;

	for (pass = 0; pass < 2; pass++)
	{
		for (part = pass ? name : path; part && *part && used + 1 < size; part++)
			out[used++] = (char)tolower((unsigned char)*part);
		if (!pass && used + 1 < size)
			out[used++] = ' ';
	}
	out[used] = 0;
}

static int has_word(char const *lower, char const *const *words)
{
	for (; *words; words++)
		if (strstr(lower, *words))
			return 1;
	return 0;
}

static int ai_kind(char const *lower)
{
	if (strstr(lower, "floodcombat") || strstr(lower, "flood_combat"))
		return AI_FLOOD_COMBAT;
	if (strstr(lower, "floodcarrier") || strstr(lower, "flood_carrier"))
		return AI_FLOOD_CARRIER;
	if (strstr(lower, "flood"))
		return AI_FLOOD_INFECTION;
	if (strstr(lower, "grunt"))
		return AI_GRUNT;
	if (strstr(lower, "jackal"))
		return AI_JACKAL;
	if (strstr(lower, "hunter"))
		return AI_HUNTER;
	if (strstr(lower, "elite"))
		return AI_ELITE;
	if (strstr(lower, "sentinel"))
		return AI_SENTINEL;
	if (strstr(lower, "marine") || strstr(lower, "crew"))
		return AI_MARINE;
	if (strstr(lower, "bugger") || strstr(lower, "engineer"))
		return AI_ENGINEER;
	return AI_NONE;
}

static char const *ai_faction_word(int ai)
{
	switch (ai)
	{
	case AI_GRUNT: case AI_ELITE: case AI_JACKAL: case AI_HUNTER: case AI_ENGINEER: return "COVENANT";
	case AI_FLOOD_COMBAT: case AI_FLOOD_CARRIER: case AI_FLOOD_INFECTION: return "FLOOD";
	case AI_MARINE: return "HUMAN";
	}
	return "ANY POOL ONLY";
}

static char const *ai_type_word(int ai)
{
	static char const *const words[] = { "NONE", "FLOOD COMBAT", "FLOOD CARRIER", "FLOOD COMBAT", "GRUNT", "JACKAL", "HUNTER", "ELITE", "SENTINEL",
		"MARINE", "ENGINEER" };

	return words[ai];
}

int ae_is_auto_answer(int row, int kind, char const *path, char const *name, char *out, int size)
{
	char lower[600], hud[600];
	int each;

	if (size < 1)
		return 0;
	out[0] = 0;
	lower_into(lower, (int)sizeof(lower), path, name);
	if (row == AE_IS_ROW_FACTION && kind == 0)
		snprintf(out, (size_t)size, "%s", has_word(lower, covenant_words) ? "COVENANT" : "HUMAN");
	else if (row == AE_IS_ROW_FACTION && kind == 1)
		snprintf(out, (size_t)size, "%s", ai_faction_word(ai_kind(lower)));
	else if (row == AE_IS_ROW_CLASS && kind == 0)
		snprintf(out, (size_t)size, "%s", has_word(lower, precision_words) ? "PRECISION" : has_word(lower, power_words) ? "POWER" :
			has_word(lower, automatic_words) ? "AUTOMATIC" : "NONE");
	else if (row == AE_IS_ROW_COUNTS_AS && kind == 1)
		snprintf(out, (size_t)size, "%s", ai_type_word(ai_kind(lower)));
	else if (row == AE_IS_ROW_HUD && kind == 0)
	{
		/* (its path's underscores read as spaces) */
		snprintf(hud, sizeof(hud), "%s", lower);
		for (each = 0; hud[each]; each++)
			if (hud[each] == '_')
				hud[each] = ' ';
		snprintf(out, (size_t)size, "ITS PARTS'");
		for (each = 0; each < (int)(sizeof(auto_hud_words) / sizeof(auto_hud_words[0])); each++)
			if (strstr(hud, auto_hud_words[each].word))
			{
				snprintf(out, (size_t)size, "%s", auto_hud_words[each].shown);
				break;
			}
	}
	else
		return 0;
	return 1;
}

static void sentence_case(char *text)
{
	int each;

	for (each = 1; text[each]; each++)
		text[each] = (char)tolower((unsigned char)text[each]);
}

void ae_is_pools_words(int kind, char const *path, char const *name, char const *faction, char const *klass, char const *type, int short_form,
	char *out, int size)
{
	char side[40], class_word[40], type_word[40], parts[3][48];
	int count = 0, automatic = 0, row, each, both_auto;

	if (size < 1)
		return;
	out[0] = 0;
	if (kind != 0 && kind != 1)
		return;
	side[0] = class_word[0] = type_word[0] = 0;
	/* (a setting kept: its word; else what AUTO finds) */
	row = ae_is_value_index(AE_IS_ROW_FACTION, kind, faction);
	if (row)
		snprintf(side, sizeof(side), "%s", ae_is_value_word(AE_IS_ROW_FACTION, kind, row));
	else
	{
		ae_is_auto_answer(AE_IS_ROW_FACTION, kind, path, name, side, (int)sizeof(side));
		automatic = 1;
	}
	if (kind == 0)
	{
		row = ae_is_value_index(AE_IS_ROW_CLASS, 0, klass);
		if (row)
			snprintf(class_word, sizeof(class_word), "%s", ae_is_value_word(AE_IS_ROW_CLASS, 0, row));
		else
		{
			ae_is_auto_answer(AE_IS_ROW_CLASS, 0, path, name, class_word, (int)sizeof(class_word));
			automatic = 1;
		}
	}
	else
	{
		row = ae_is_value_index(AE_IS_ROW_COUNTS_AS, 1, type);
		if (row)
			snprintf(type_word, sizeof(type_word), "%s", ae_is_value_word(AE_IS_ROW_COUNTS_AS, 1, row));
		else
		{
			ae_is_auto_answer(AE_IS_ROW_COUNTS_AS, 1, path, name, type_word, (int)sizeof(type_word));
			automatic = 1;
		}
	}
	both_auto = automatic;
	if (!short_form)
		snprintf(parts[count++], sizeof(parts[0]), "ANY");
	snprintf(parts[count++], sizeof(parts[0]), "%s", side);
	snprintf(parts[count++], sizeof(parts[0]), "%s", kind == 0 ? class_word : type_word);
	for (each = 0; each < count; each++)
	{
		char piece[48];

		if (!parts[each][0] || !strcmp(parts[each], "NONE"))
			continue;
		snprintf(piece, sizeof(piece), "%s", parts[each]);
		if (short_form)
			sentence_case(piece);
		if (out[0])
			strncat(out, " " DOT_SEP " ", (size_t)size - strlen(out) - 1);
		strncat(out, piece, (size_t)size - strlen(out) - 1);
	}
	if (short_form && both_auto)
		strncat(out, " (auto)", (size_t)size - strlen(out) - 1);
}

char const *ae_is_row_help(int row)
{
	switch (row)
	{
	case AE_IS_ROW_ON: case AE_IS_ROW_PLAY_AS: return "Changes play from the next mission you load.";
	case AE_IS_ROW_FACTION: return "AUTO finds it from the item's name, as the randomizer finds the campaign's.";
	case AE_IS_ROW_CLASS: return "AUTO reads its name for the PRECISION, POWER or AUTOMATIC words.";
	case AE_IS_ROW_COUNTS_AS: return "AUTO reads its name for the campaign's kind of enemy.";
	case AE_IS_ROW_HUD: return "ITS OWN keeps its crosshair; AUTO picks a campaign weapon's from its name.";
	case AE_IS_ROW_REPLACE: return "ON: it plays wherever a mission has an item of its path.";
	case AE_IS_ROW_HANDS: case AE_IS_ROW_MOVEMENT: return "Stored with the set; the next build plays it.";
	}
	return "";
}

int ae_is_body_rows_live(void)
{
	return 0;
}

int ae_is_row_change(int row, int play_as_blocked)
{
	if (ae_is_row_typed(row))
		return 2;
	switch (row)
	{
	case AE_IS_ROW_REMOVE: return 0;
	case AE_IS_ROW_PLAY_AS: return play_as_blocked ? 0 : 1;
	case AE_IS_ROW_HANDS: case AE_IS_ROW_MOVEMENT: return ae_is_body_rows_live() ? 1 : 0;
	}
	return row >= 0 && row < AE_IS_ROWS ? 1 : 0;
}

/* ---------- why a biped does not fit */

static void clause_add(char clauses[][160], int *count, char const *format, int a, int b)
{
	if (*count < 6)
		snprintf(clauses[(*count)++], 160, format, a, b);
}

static void fit_clauses(struct ae_is_fit const *fit, char clauses[][160], int *count)
{
	*count = 0;
	if (!fit->known || fit->fits || !fit->measured)
		return;
	if (fit->no_graph)
		clause_add(clauses, count, "it has no animations", 0, 0);
	if (!fit->physics)
		clause_add(clauses, count, "it has no player physics", 0, 0);
	if (fit->seats_total > 0 && fit->seats_have < fit->seats_total)
		clause_add(clauses, count, "it has no seat for %d of the stock player's %d vehicle seats", fit->seats_total - fit->seats_have, fit->seats_total);
	if (fit->weapons_total > 0 && fit->weapons_held < fit->weapons_total)
		clause_add(clauses, count, "it can't hold %d of the campaign's %d weapons", fit->weapons_total - fit->weapons_held, fit->weapons_total);
	if (fit->size_percent > 0 && fit->size_percent < (int)(AE_BIPED_SIZE_RATIO_MINIMUM * 100.0f))
		clause_add(clauses, count, "it is too small: %d %% of the stock player's size (at least %d %%)", fit->size_percent,
			(int)(AE_BIPED_SIZE_RATIO_MINIMUM * 100.0f));
	else if (fit->size_percent > (int)(AE_BIPED_SIZE_RATIO_MAXIMUM * 100.0f))
		clause_add(clauses, count, "it is too big: %d %% of the stock player's size (at most %d %%)", fit->size_percent,
			(int)(AE_BIPED_SIZE_RATIO_MAXIMUM * 100.0f));
}

void ae_is_fit_words(struct ae_is_fit const *fit, char *text, int size)
{
	char clauses[6][160];
	int count, each;

	if (size < 1)
		return;
	text[0] = 0;
	fit_clauses(fit, clauses, &count);
	if (!fit->known || fit->fits)
		return;
	if (!count)
	{
		snprintf(text, (size_t)size, "A level that loaded it found it does not fit a player.");
		return;
	}
	for (each = 0; each < count; each++)
	{
		char piece[200];

		snprintf(piece, sizeof(piece), "%s%s", each == 0 ? "" : each + 1 == count ? ", and " : ", ", clauses[each]);
		if (each == 0)
			piece[0] = (char)toupper((unsigned char)piece[0]);
		strncat(text, piece, (size_t)size - strlen(text) - 1);
	}
	strncat(text, ".", (size_t)size - strlen(text) - 1);
}

void ae_is_fit_short(struct ae_is_fit const *fit, char *text, int size)
{
	char clauses[6][160];
	int count, each;

	if (size < 1)
		return;
	text[0] = 0;
	fit_clauses(fit, clauses, &count);
	if (!fit->known || fit->fits)
		return;
	if (!count)
	{
		snprintf(text, (size_t)size, "a level found it does not fit");
		return;
	}
	for (each = 0; each < count && each < 2; each++)
	{
		if (each)
			strncat(text, ", and ", (size_t)size - strlen(text) - 1);
		strncat(text, clauses[each], (size_t)size - strlen(text) - 1);
	}
}

static void hint(struct ae_is_hint *hints, int *count, int which, char const *label)
{
	if (*count < AE_IS_HINTS_MOST)
	{
		hints[*count].hint = which;
		hints[*count].label = label;
		(*count)++;
	}
}

int ae_is_hints(int step, int playing, int has_items, struct ae_is_hint hints[AE_IS_HINTS_MOST])
{
	int count = 0;

	switch (step)
	{
	case AE_IS_STEP_SETS:
		/* (-2: the CAMPAIGN SWAPS door, which A opens; -3: a row that waits, which A does not) */
		if (has_items == -2)
			hint(hints, &count, AE_IS_HINT_OPEN, "Open");
		if (has_items >= 0)
		{
			hint(hints, &count, AE_IS_HINT_OPTIONS, "Options");
			hint(hints, &count, playing ? AE_IS_HINT_STOP : AE_IS_HINT_PLAY, playing ? "Stop playing" : "Play this set");
		}
		hint(hints, &count, AE_IS_HINT_NEW, "New set");
		break;
	case AE_IS_STEP_SWAPS:
		/* (1: a swap is focused; 0: NEW SWAP, or no swap yet) */
		hint(hints, &count, AE_IS_HINT_OPEN, has_items > 0 ? "Open" : "New swap");
		if (has_items > 0)
		{
			hint(hints, &count, AE_IS_HINT_NEW, "New swap");
			hint(hints, &count, AE_IS_HINT_REMOVE, "Delete");
		}
		break;
	case AE_IS_STEP_SWAP:
		/* (ae_is_swap_row_change's answer for the focused row) */
		if (has_items == 1)
			hint(hints, &count, AE_IS_HINT_CHANGE, "Change");
		else if (has_items == 2)
			hint(hints, &count, AE_IS_HINT_OPEN, "Type");
		else if (has_items == 3)
			hint(hints, &count, AE_IS_HINT_OPEN, "Open");
		hint(hints, &count, AE_IS_HINT_REMOVE, "Delete swap");
		break;
	case AE_IS_STEP_RANKS:
		hint(hints, &count, AE_IS_HINT_OPEN, "Open");
		break;
	case AE_IS_STEP_SLOT:
		hint(hints, &count, AE_IS_HINT_OPEN, "On / off");
		break;
	case AE_IS_STEP_CONTENTS:
		/* (Content Evolved's keys: A opens the item's settings, left and right turn it on or off) */
		if (has_items > 0)
		{
			hint(hints, &count, AE_IS_HINT_SETTINGS, "Settings");
			hint(hints, &count, AE_IS_HINT_ON_OFF, "On / off");
			hint(hints, &count, AE_IS_HINT_REMOVE, "Remove");
		}
		/* (2: the firing range row, which A starts) */
		if (has_items == 2)
			hint(hints, &count, AE_IS_HINT_OPEN, "Try it");
		hint(hints, &count, AE_IS_HINT_ADD_ITEMS, "Add items");
		break;
	case AE_IS_STEP_SETTINGS:
		/* (a row that cannot be changed has no Change; a typed one is typed with A) */
		if (has_items == 1)
			hint(hints, &count, AE_IS_HINT_CHANGE, "Change");
		else if (has_items == 2)
			hint(hints, &count, AE_IS_HINT_OPEN, "Type");
		hint(hints, &count, AE_IS_HINT_REMOVE, "Remove");
		break;
	case AE_IS_STEP_MAPS:
		hint(hints, &count, AE_IS_HINT_OPEN_MAP, "Open the map");
		hint(hints, &count, AE_IS_HINT_SEARCH, "Search");
		hint(hints, &count, AE_IS_HINT_PAGE, "Page");
		break;
	case AE_IS_STEP_KINDS:
		hint(hints, &count, AE_IS_HINT_OPEN, "Open");
		hint(hints, &count, AE_IS_HINT_CLOSE_MAP, "Close the map");
		return count;
	case AE_IS_STEP_ITEMS:
		if (has_items > 0)
			hint(hints, &count, AE_IS_HINT_REMOVE, "Remove");
		else
			hint(hints, &count, AE_IS_HINT_ADD, "Add to set");
		hint(hints, &count, AE_IS_HINT_SEARCH, "Search");
		hint(hints, &count, AE_IS_HINT_PAGE, "Page");
		break;
	}
	hint(hints, &count, AE_IS_HINT_BACK, "Back");
	return count;
}
