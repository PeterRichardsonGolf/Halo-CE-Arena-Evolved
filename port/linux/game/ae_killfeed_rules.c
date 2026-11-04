/*
AE_KILLFEED_RULES.C

The kill feed's pure rules (ae_killfeed_rules.h).
*/

#include <string.h>
#include <wchar.h>

#include "ae_killfeed_rules.h"
#include "ae_marker_rules.h"

/* ---------- weapons */

/* a path in lower case with backslashes only, clipped to the buffer */
static void normalise(char const *path, char *out, int size)
{
	int index = 0;

	if (size < 1)
		return;
	for (; path && *path && index < size - 1; path++, index++)
	{
		char c = *path;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		else if (c == '/')
			c = '\\';
		out[index] = c;
	}
	out[index] = 0;
}

/* "evo" as a word of its own or the start of "evo_rifle" / "evo rifle", never inside "evolved" */
static boolean evo_word(char const *text)
{
	char const *at;

	for (at = strstr(text, "evo"); at; at = strstr(at + 1, "evo"))
	{
		boolean starts = at == text || at[-1] == '\\' || at[-1] == ' ' || at[-1] == '_';
		char next = at[3];

		if (starts && (next == 0 || next == '\\' || next == ' ' || next == '_'))
			return TRUE;
	}
	return FALSE;
}

int ae_kf_weapon_of_path(char const *path)
{
	char text[256];

	if (!path)
		return AE_KF_WEAPON_UNKNOWN;
	normalise(path, text, sizeof(text));
	if (strstr(text, "melee"))
		return AE_KF_WEAPON_MELEE;
	if (strstr(text, "frag grenade") || strstr(text, "frag_grenade"))
		return AE_KF_WEAPON_FRAG;
	if (strstr(text, "plasma grenade") || strstr(text, "plasma_grenade"))
		return AE_KF_WEAPON_PLASMA_GRENADE;
	if (strstr(text, "vehicles\\") || strstr(text, "collision"))
		return AE_KF_WEAPON_VEHICLE;
	if (strstr(text, "falling") || strstr(text, "distance"))
		return AE_KF_WEAPON_FALL;
	if (strstr(text, "sword"))
		return AE_KF_WEAPON_SWORD;
	if (strstr(text, "hammer"))
		return AE_KF_WEAPON_HAMMER;
	if (strstr(text, "bomb"))
		return AE_KF_WEAPON_BOMB;
	if (strstr(text, "plasma pistol") || strstr(text, "plasma_pistol"))
		return AE_KF_WEAPON_PLASMA_PISTOL;
	if (strstr(text, "plasma rifle") || strstr(text, "plasma_rifle"))
		return AE_KF_WEAPON_PLASMA_RIFLE;
	if (strstr(text, "plasma cannon") || strstr(text, "plasma_cannon") || strstr(text, "fuel rod") || strstr(text, "fuel_rod"))
		return AE_KF_WEAPON_FUEL_ROD;
	if (strstr(text, "needler"))
		return AE_KF_WEAPON_NEEDLER;
	if (strstr(text, "rocket"))
		return AE_KF_WEAPON_ROCKET;
	if (strstr(text, "sniper"))
		return AE_KF_WEAPON_SNIPER;
	if (strstr(text, "shotgun"))
		return AE_KF_WEAPON_SHOTGUN;
	if (strstr(text, "flamethrower") || strstr(text, "flame"))
		return AE_KF_WEAPON_FLAME;
	if (strstr(text, "assault rifle") || strstr(text, "assault_rifle"))
		return AE_KF_WEAPON_ASSAULT_RIFLE;
	if (strstr(text, "battle rifle") || strstr(text, "battle_rifle"))
		return AE_KF_WEAPON_BATTLE_RIFLE;
	/* (the Evo rifle's folder, not any path with "evolved" in it: the community packs' folders are "evolved") */
	if (evo_word(text))
		return AE_KF_WEAPON_EVO;
	if (strstr(text, "dmr"))
		return AE_KF_WEAPON_DMR;
	if (strstr(text, "carbine"))
		return AE_KF_WEAPON_CARBINE;
	if (strstr(text, "smg"))
		return AE_KF_WEAPON_SMG;
	if (strstr(text, "pistol") || strstr(text, "magnum"))
		return AE_KF_WEAPON_PISTOL;
	return AE_KF_WEAPON_UNKNOWN;
}

int ae_kf_weapon_of_slot(int slot)
{
	switch (slot)
	{
	case 1: return AE_KF_WEAPON_BATTLE_RIFLE;
	case 2: return AE_KF_WEAPON_SMG;
	case 3: return AE_KF_WEAPON_SMG_SILENCED;
	case 4: return AE_KF_WEAPON_CARBINE;
	case 5: return AE_KF_WEAPON_AR_SILENCED;
	case 6: return AE_KF_WEAPON_DMR;
	case 7: return AE_KF_WEAPON_EVO;
	case 8: return AE_KF_WEAPON_NEEDLER;
	case 9: return AE_KF_WEAPON_SNIPER_BOLT;
	case 10: return AE_KF_WEAPON_MAGNUM_SCOPED;
	case 11: return AE_KF_WEAPON_BRUTE_SHOT;
	case 12: return AE_KF_WEAPON_AR_MODERN;
	case 13: return AE_KF_WEAPON_SIDEARM;
	case 14: return AE_KF_WEAPON_SWORD;
	default: return AE_KF_WEAPON_UNKNOWN;
	}
}

static char const *const weapon_words[AE_KF_WEAPON_COUNT] = {
	"",				/* unknown */
	"PISTOL",
	"AR",
	"SNIPER",
	"SHOTGUN",
	"ROCKETS",
	"PLASMA PISTOL",
	"PLASMA RIFLE",
	"NEEDLER",
	"FUEL ROD",
	"FLAME",
	"BR",
	"DMR",
	"EVO",
	"CARBINE",
	"SMG",
	"SMG",			/* silenced */
	"AR",			/* silenced */
	"SWORD",
	"HAMMER",
	"BOMB",
	"GRENADE",		/* frag */
	"GRENADE",		/* plasma */
	"MELEE",
	"VEHICLE",
	"FALL",
	"SNIPER",		/* the bolt-action sniper rifle */
	"HJ MAGNUM",	/* the HJ magnum */
	"BRUTE SHOT",
	"AR",			/* the combat assault rifle */
	"SIDEARM"
};

char const *ae_kf_weapon_word(int weapon)
{
	return weapon >= 0 && weapon < AE_KF_WEAPON_COUNT ? weapon_words[weapon] : "";
}

char const *ae_kf_weapon_tag(int weapon)
{
	if (weapon == AE_KF_WEAPON_PLASMA_GRENADE)
		return "PLASMA";
	if (weapon <= AE_KF_WEAPON_UNKNOWN || weapon >= AE_KF_WEAPON_COUNT)
		return "?";
	return weapon_words[weapon];
}

boolean ae_kf_weapon_tag_path(char const *damage_path, char *out, int size)
{
	char text[256];
	char *folder, *end;
	int length;

	if (!damage_path || !out || size < 1)
		return FALSE;
	out[0] = 0;
	normalise(damage_path, text, sizeof(text));
	if (strncmp(text, "weapons\\", 8) != 0)
		return FALSE;
	folder = text + 8;
	end = strchr(folder, '\\');
	if (!end || end == folder)
		return FALSE;
	*end = 0;
	length = (int)strlen(folder);
	/* "weapons\" + folder + "\" + folder */
	if (8 + length + 1 + length + 1 > size)
		return FALSE;
	memcpy(out, "weapons\\", 8);
	memcpy(out + 8, folder, (size_t)length);
	out[8 + length] = '\\';
	memcpy(out + 8 + length + 1, folder, (size_t)length);
	out[8 + length + 1 + length] = 0;
	return TRUE;
}

boolean ae_kf_weapon_has_no_picture(int weapon)
{
	return weapon == AE_KF_WEAPON_MELEE || weapon == AE_KF_WEAPON_VEHICLE || weapon == AE_KF_WEAPON_FALL ||
		weapon == AE_KF_WEAPON_FRAG || weapon == AE_KF_WEAPON_PLASMA_GRENADE || weapon == AE_KF_WEAPON_BOMB ||
		weapon == AE_KF_WEAPON_UNKNOWN;
}

/* ---------- a list of rows */

static boolean counted(struct ae_kf_row const *row, long now)
{
	return row->used && row->gone_at < 0 && now < row->life_end + AE_KF_FADE_TICKS;
}

void ae_kf_list_reset(struct ae_kf_list *list)
{
	memset(list, 0, sizeof(*list));
	list->last = -1;
}

static real clamp01(real value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

static real slot_at(struct ae_kf_row const *row, long now)
{
	real t = clamp01((real)(now - row->moved_at) / (real)AE_KF_CLOSE_TICKS);

	return row->from + (row->to - row->from) * t;
}

/* each row that holds a slot takes the one the live rows newer than it leave; a row whose slot changes starts moving from where it
is */
static void retarget(struct ae_kf_list *list, long now)
{
	int index, other;

	for (index = 0; index < AE_KF_ROWS; index++)
	{
		struct ae_kf_row *row = &list->rows[index];
		int newer = 0;

		if (!counted(row, now))
			continue;
		for (other = index + 1; other < AE_KF_ROWS; other++)
			if (counted(&list->rows[other], now))
				newer++;
		if ((real)newer != row->to)
		{
			row->from = slot_at(row, now);
			row->to = (real)newer;
			row->moved_at = now;
		}
	}
}

/* the rows kept oldest first, without gaps */
static void compact(struct ae_kf_list *list, long now)
{
	int from, to = 0;

	for (from = 0; from < AE_KF_ROWS; from++)
	{
		struct ae_kf_row const *row = &list->rows[from];
		boolean alive = row->used && now < row->life_end + AE_KF_FADE_TICKS && (row->gone_at < 0 || now < row->gone_at + AE_KF_GONE_TICKS);

		if (!alive)
			continue;
		if (to != from)
			list->rows[to] = *row;
		to++;
	}
	for (; to < AE_KF_ROWS; to++)
		memset(&list->rows[to], 0, sizeof(list->rows[to]));
}

/* a full list: the oldest row, if it is not leaving already, is given the time it has to go */
static void squeeze(struct ae_kf_list *list, long now)
{
	int index, live = 0, oldest = -1;

	for (index = 0; index < AE_KF_ROWS; index++)
	{
		if (!counted(&list->rows[index], now))
			continue;
		live++;
		if (oldest < 0)
			oldest = index;
	}
	if (live >= AE_KF_MOST && oldest >= 0 && now < list->rows[oldest].life_end)
	{
		long leave = list->rows[oldest].born + AE_KF_FULL_LEAVE_TICKS;

		if (leave < now)
			leave = now;
		if (leave < list->rows[oldest].life_end)
			list->rows[oldest].life_end = leave;
	}
}

void ae_kf_list_step(struct ae_kf_list *list, long now)
{
	if (list->last >= 0 && now < list->last)
		ae_kf_list_reset(list);
	list->last = now;
	compact(list, now);
	squeeze(list, now);
	compact(list, now);
	retarget(list, now);
}

int ae_kf_list_add(struct ae_kf_list *list, struct ae_kf_row const *row, long now)
{
	int index, live = 0;
	struct ae_kf_row *added;

	ae_kf_list_step(list, now);
	for (index = 0; index < AE_KF_ROWS; index++)
		if (counted(&list->rows[index], now))
			live++;
	if (live >= AE_KF_MOST)
	{
		/* (no queue: the oldest goes at once) */
		for (index = 0; index < AE_KF_ROWS; index++)
			if (counted(&list->rows[index], now))
			{
				list->rows[index].gone_at = now;
				break;
			}
	}
	/* (no room: the oldest row of all goes at once) */
	if (list->rows[AE_KF_ROWS - 1].used)
	{
		memmove(&list->rows[0], &list->rows[1], sizeof(list->rows[0]) * (AE_KF_ROWS - 1));
		memset(&list->rows[AE_KF_ROWS - 1], 0, sizeof(list->rows[0]));
	}
	for (index = 0; index < AE_KF_ROWS && list->rows[index].used; index++)
		;
	added = &list->rows[index];
	*added = *row;
	added->used = TRUE;
	added->born = now;
	added->life_end = now + AE_KF_LIFE;
	added->gone_at = -1;
	added->from = 0.0f;
	added->to = 0.0f;
	added->moved_at = now;
	squeeze(list, now);
	retarget(list, now);
	return live;
}

int ae_kf_list_count(struct ae_kf_list const *list)
{
	int index, count = 0;

	for (index = 0; index < AE_KF_ROWS; index++)
		if (list->rows[index].used)
			count++;
	return count;
}

int ae_kf_list_live(struct ae_kf_list const *list, long now)
{
	int index, count = 0;

	for (index = 0; index < AE_KF_ROWS; index++)
		if (counted(&list->rows[index], now))
			count++;
	return count;
}

boolean ae_kf_row_pose(struct ae_kf_row const *row, long now, real *slot, real *alpha, real *slide)
{
	real enter, a;

	if (!row->used || now < row->born || now >= row->life_end + AE_KF_FADE_TICKS ||
		(row->gone_at >= 0 && now >= row->gone_at + AE_KF_GONE_TICKS))
		return FALSE;
	enter = clamp01((real)(now - row->born) / (real)AE_KF_ENTER_TICKS);
	a = enter;
	if (now >= row->life_end)
	{
		real fade = 1.0f - (real)(now - row->life_end) / (real)AE_KF_FADE_TICKS;

		a = a < fade ? a : fade;
	}
	if (row->gone_at >= 0)
	{
		real fade = 1.0f - (real)(now - row->gone_at) / (real)AE_KF_GONE_TICKS;

		a = a < fade ? a : fade;
	}
	*slot = slot_at(row, now);
	*alpha = clamp01(a);
	*slide = AE_KF_SLIDE * (1.0f - enter);
	return TRUE;
}

/* ---------- colours */

struct ae_kf_rgb ae_kf_navy(void) { struct ae_kf_rgb c = { 3, 24, 58 }; return c; }
struct ae_kf_rgb ae_kf_tick_mine(void) { struct ae_kf_rgb c = { 184, 214, 255 }; return c; }
struct ae_kf_rgb ae_kf_tick_assist(void) { struct ae_kf_rgb c = { 40, 148, 254 }; return c; }
struct ae_kf_rgb ae_kf_ice(void) { struct ae_kf_rgb c = { 184, 214, 255 }; return c; }
struct ae_kf_rgb ae_kf_grey(void) { struct ae_kf_rgb c = { 188, 198, 212 }; return c; }
struct ae_kf_rgb ae_kf_white(void) { struct ae_kf_rgb c = { 229, 240, 255 }; return c; }

boolean ae_kf_is_ally(boolean teams, boolean same_player, boolean same_team)
{
	return (boolean)(same_player || (teams && same_team));
}

boolean ae_kf_name_white(boolean ae_style, boolean enemy_name_red, boolean ally)
{
	return (boolean)(!ae_style && !enemy_name_red && !ally);
}

/* ---------- layout */

real ae_kf_text_px(real scale)
{
	real px = AE_KF_TEXT_PX * scale;

	return px < AE_KF_TEXT_FLOOR_PX ? AE_KF_TEXT_FLOOR_PX : px;
}

boolean ae_kf_icon_fit(real picture_width, real picture_height, real scale, real *width, real *height)
{
	real across, down, fit;

	if (!(picture_width > 0.0f) || !(picture_height > 0.0f) || !(scale > 0.0f))
		return FALSE;
	across = AE_KF_ICON_WIDTH * scale / picture_width;
	down = AE_KF_ICON_HEIGHT * scale / picture_height;
	fit = across < down ? across : down;
	*width = picture_width * fit;
	*height = picture_height * fit;
	return TRUE;
}

void ae_kf_layout(struct ae_kf_parts const *parts, struct ae_kf_layout *layout)
{
	real gap = AE_KF_GAP * parts->scale;
	real inset = AE_KF_INSET * parts->scale;
	real x = inset;

	if (parts->has_killer)
	{
		layout->killer_x = x;
		x += parts->killer_width + gap;
	}
	else
		layout->killer_x = x;
	layout->icon_x = x;
	x += parts->icon_width;
	if (parts->has_mark)
	{
		x += gap;
		layout->mark_x = x;
		x += parts->mark_width;
	}
	else
		layout->mark_x = x;
	x += gap;
	layout->victim_x = x;
	x += parts->victim_width;
	layout->width = x + inset;
}

real ae_kf_row_left(real anchor_x, real width)
{
	return AE_KF_ALIGN == AE_KF_ALIGN_RIGHT ? anchor_x - width : anchor_x;
}

real ae_kf_anchor_x(boolean sensor_known, real sensor_left, real area_left, real scale)
{
	return sensor_known ? sensor_left - AE_KF_SENSOR_GAP * scale : area_left + AE_KF_FALLBACK_X * scale;
}

real ae_kf_column_left(real width, real height)
{
	real column = height * 16.0f / 9.0f;

	return width > column + 1.0f && height > 0.0f ? (width - column) * 0.5f : 0.0f;
}

real ae_kf_anchor_x_column(real column_left, real scale)
{
	return column_left + AE_KF_COLUMN_INSET * scale;
}

real ae_kf_anchor_y(boolean mcc_medals, real scale)
{
	return (mcc_medals ? AE_KF_BOTTOM_MCC : AE_KF_BOTTOM) * scale;
}

real ae_kf_slot_top(real anchor_y, real slot, real scale)
{
	if (AE_KF_GROWS_UP)
		return anchor_y - AE_KF_ROW_HEIGHT * scale - slot * AE_KF_ROW_PITCH * scale;
	return anchor_y + slot * AE_KF_ROW_PITCH * scale;
}

/* ---------- a split-screen line */

struct writer
{
	wchar_t *out;
	int size;
	int length;
	boolean full;
};

static void put(struct writer *w, wchar_t c)
{
	if (w->length >= w->size - 1)
		w->full = TRUE;
	else
		w->out[w->length++] = c;
}

static void put_text(struct writer *w, wchar_t const *text)
{
	for (; text && *text; text++)
		put(w, *text);
}

static void put_word(struct writer *w, char const *word)
{
	for (; word && *word; word++)
		put(w, (wchar_t)(unsigned char)*word);
}

static void put_color(struct writer *w, struct ae_kf_rgb color)
{
	put(w, AE_RUN_RGB);
	put(w, AE_RUN_BYTE(color.r));
	put(w, AE_RUN_BYTE(color.g));
	put(w, AE_RUN_BYTE(color.b));
}

int ae_kf_split_line(wchar_t *out, int size, struct ae_kf_split const *line)
{
	struct writer w;
	struct ae_kf_rgb word_color = line->mine ? ae_kf_ice() : ae_kf_grey();
	char const *word = ae_kf_weapon_word(line->weapon);

	if (!out || size < 1)
		return 0;
	w.out = out;
	w.size = size;
	w.length = 0;
	w.full = FALSE;
	put(&w, AE_RUN_NOWRAP);
	if (line->kind == AE_KF_KIND_SUICIDE)
	{
		put_color(&w, line->victim_rgb);
		put_text(&w, line->victim);
		put_color(&w, word_color);
		put_word(&w, " SUICIDE");
	}
	else if (line->kind == AE_KF_KIND_ENVIRONMENT)
	{
		put_color(&w, word_color);
		put_word(&w, word[0] ? word : "DIED");
		put_color(&w, line->victim_rgb);
		put(&w, L' ');
		put_text(&w, line->victim);
	}
	else
	{
		put_color(&w, line->killer_rgb);
		put_text(&w, line->killer);
		put_color(&w, word_color);
		put(&w, L' ');
		put_word(&w, word[0] ? word : "?");
		if (line->perfect)
			put_word(&w, " PERFECT");
		else if (line->headshot)
			put_word(&w, " HEADSHOT");
		put_color(&w, line->victim_rgb);
		put(&w, L' ');
		put_text(&w, line->victim);
	}
	if (w.full)
	{
		out[0] = 0;
		return 0;
	}
	out[w.length] = 0;
	return w.length;
}

/* ---------- the game's kill lines */

boolean ae_kf_message_is_kill_line(long message)
{
	return (boolean)((message >= 1 && message <= 6) || message == 8 || message == 13 || message == 19);
}

boolean ae_kf_message_is_medal_line(long message)
{
	return (boolean)(message == 7 || (message >= 9 && message <= 12) || (message >= 14 && message <= 18));
}

boolean ae_kf_pickup_text_shown(boolean feed_on)
{
	return (boolean)!feed_on;
}
