/*
AE_GAMETYPE_PICKER.C

CHANGE GAMETYPE's model. See ae_gametype_picker.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_gametype_picker.h"
#include "ae_setup_options.h"

static const char *const folder_names[AE_GTP_FOLDERS] = { "RECENT", "FAVOURITES", "MY GAMETYPES", "AE FFA", "AE 2V2",
	"AE 4V4", "AE BTB", "AE OBJECTIVE", "AE ACTION SACK", "AE PRACTICE", "NHE", "OPENCE", "STOCK" };
static const char *const filter_names[AE_GTP_FILTERS] = { "ALL", "SLAYER", "CTF", "KING", "ODDBALL", "RACE", "BOMB" };
static const char *const weapon_names[AE_GTP_WEAPONS] = { "NO WEAPON", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN",
	"SNIPER RIFLE", "ROCKET LAUNCHER", "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER" };
/* (a start's short words: the assault rifle is AR, the sniper rifle SNIPER, ...) */
static const char *const weapon_starts[AE_GTP_WEAPONS] = { "NO WEAPON", "RANDOM", "AR", "PISTOL", "SHOTGUN", "SNIPER",
	"ROCKET", "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER" };

/* game_engine.c's enum game_engine_weapons */
enum { SET_NORMAL, SET_PISTOLS, SET_RIFLES, SET_PLASMA, SET_SNIPING, SET_NO_SNIPING, SET_ROCKETS, SET_SHOTGUNS,
	SET_SHORT_RANGE, SET_HUMAN, SET_NO_GRENADES, SET_COVENANT, SET_CLASSIC, SET_HEAVY, SETS };

const char *ae_gtp_folder_name(int folder)
{
	return folder >= 0 && folder < AE_GTP_FOLDERS ? folder_names[folder] : "";
}

const char *ae_gtp_folder_label(struct ae_gtp const *gtp, int folder)
{
	if (ae_gtp_folder_is_own(folder))
		return gtp && folder - AE_GTP_FOLDER_OWN < gtp->own_count ? gtp->own_names[folder - AE_GTP_FOLDER_OWN] : "";
	return ae_gtp_folder_name(folder);
}

const char *ae_gtp_filter_name(int filter)
{
	return filter >= 0 && filter < AE_GTP_FILTERS ? filter_names[filter] : "";
}

const char *ae_gtp_weapon_name(int weapon)
{
	return weapon >= 0 && weapon < AE_GTP_WEAPONS ? weapon_names[weapon] : "";
}

int ae_gtp_filter_of_engine(int engine)
{
	switch (engine)
	{
	case AE_SETUP_ENGINE_SLAYER: return AE_GTF_SLAYER;
	case AE_SETUP_ENGINE_CTF: return AE_GTF_CTF;
	case AE_SETUP_ENGINE_KING: return AE_GTF_KING;
	case AE_SETUP_ENGINE_ODDBALL: return AE_GTF_ODDBALL;
	case AE_SETUP_ENGINE_RACE: return AE_GTF_RACE;
	case AE_SETUP_ENGINE_BOMB: return AE_GTF_BOMB;
	}
	return AE_GTF_ALL;
}

int ae_gtp_weapon_in_set(int weapon, int weapon_set)
{
	int rifle = weapon == AE_GTW_ASSAULT_RIFLE;

	if (!rifle && weapon != AE_GTW_PISTOL)
		return weapon;
	switch (weapon_set)
	{
	case SET_PISTOLS: return rifle ? AE_GTW_PISTOL : AE_GTW_PLASMA_PISTOL;
	case SET_RIFLES: return rifle ? AE_GTW_ASSAULT_RIFLE : AE_GTW_PLASMA_RIFLE;
	case SET_PLASMA: return rifle ? AE_GTW_PLASMA_RIFLE : AE_GTW_PLASMA_PISTOL;
	case SET_SNIPING: return rifle ? AE_GTW_SNIPER : AE_GTW_PISTOL;
	case SET_NO_SNIPING: return AE_GTW_ASSAULT_RIFLE;
	case SET_ROCKETS: case SET_HEAVY: return AE_GTW_ROCKET;
	case SET_SHOTGUNS: case SET_SHORT_RANGE: return AE_GTW_SHOTGUN;
	case SET_COVENANT: return rifle ? AE_GTW_PLASMA_RIFLE : AE_GTW_PLASMA_PISTOL;
	}
	return weapon;
}

void ae_gtp_start_text(const char *pack, int custom, int primary, int weapon_set, char *text, int size)
{
	const char *words;

	if (size <= 0)
		return;
	if (pack && pack[0])
		words = pack;
	else if (custom)
		words = primary >= 0 && primary < AE_GTP_WEAPONS ? weapon_starts[primary] : "CUSTOM";
	else
		words = weapon_starts[ae_gtp_weapon_in_set(AE_GTW_ASSAULT_RIFLE, weapon_set)];
	snprintf(text, (size_t)size, "%s START", words);
}

/* ---------- names */

static char upper(char c)
{
	return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

static int same_text(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (upper(*a) != upper(*b))
			return 0;
	}
	return *a == *b;
}

int ae_gtp_match(const char *text, const char *query)
{
	int at, index;

	if (!text || !query || !query[0])
		return -1;
	for (at = 0; text[at]; at++)
	{
		for (index = 0; query[index] && text[at + index] && upper(text[at + index]) == upper(query[index]); index++)
			;
		if (!query[index])
			return at;
	}
	return -1;
}

/* a name without its battle rifle word ("TEAM AE BR 2V2 CTF": "TEAM AE 2V2 CTF"): the BR right after AE (not the
OCTAGON drills' own BR); 1 when it had one */
/* the name without its start word: 1 for "AE BR ...", 2 for "AE EVO ..." (0.7.0), 0 for neither */
static int without_br(const char *name, char *out, int size)
{
	static const char *const words[] = { "AE BR ", "AE EVO " };
	int kind;

	for (kind = 0; kind < 2; kind++)
	{
		const char *at = strstr(name, words[kind]);
		size_t length = strlen(words[kind]);

		if (at && (at == name || at[-1] == ' '))
		{
			snprintf(out, (size_t)size, "%.*sAE %s", (int)(at - name), name, at + length);
			return kind + 1;
		}
	}
	snprintf(out, (size_t)size, "%s", name);
	return 0;
}

/* a label without TEAM / FFA in front */
static const char *without_side(const char *name)
{
	if (!strncmp(name, "TEAM ", 5))
		return name + 5;
	if (!strncmp(name, "FFA ", 4))
		return name + 4;
	return name;
}

short ae_gtp_base(struct ae_gtp const *gtp, short gametype)
{
	if (!gtp || gametype < 0 || gametype >= gtp->count)
		return -1;
	if (gtp->gametypes[gametype].br && gtp->gametypes[gametype].twin >= 0)
		return gtp->gametypes[gametype].twin;
	if (gtp->gametypes[gametype].evo && gtp->gametypes[gametype].evo_twin >= 0)
		return gtp->gametypes[gametype].evo_twin;
	return gametype;
}

/* a base's group in the START switch's order (PISTOL, BR, EVO), -1 for the missing ones */
static void group_of(struct ae_gtp const *gtp, short base, short members[3])
{
	members[0] = base;
	members[1] = gtp->gametypes[base].br ? -1 : gtp->gametypes[base].twin;
	members[2] = gtp->gametypes[base].evo ? -1 : gtp->gametypes[base].evo_twin;
}

static int is_base(struct ae_gtp const *gtp, short gametype)
{
	return ae_gtp_base(gtp, gametype) == gametype;
}

void ae_gtp_finish(struct ae_gtp *gtp)
{
	static char plain[AE_GTP_MAXIMUM][AE_GTP_NAME];
	short index, other, written = 0;
	int folder;

	if (!gtp)
		return;
	for (index = 0; index < gtp->count; index++)
	{
		struct ae_gtp_gametype *gametype = &gtp->gametypes[index];

		int kind = without_br(gametype->name, plain[index], AE_GTP_NAME);

		gametype->twin = -1;
		gametype->evo_twin = -1;
		gametype->br = kind == 1;
		gametype->evo = kind == 2;
		gtp->shown[index] = index;
	}
	/* the pairs: a BR one and the one of its name without the word; (0.7.0) an EVO one the same way, the group's
	third */
	for (index = 0; index < gtp->count; index++)
	{
		if (!gtp->gametypes[index].br && !gtp->gametypes[index].evo)
			continue;
		for (other = 0; other < gtp->count; other++)
		{
			struct ae_gtp_gametype *plain_one = &gtp->gametypes[other];

			if (other == index || plain_one->br || plain_one->evo || !same_text(plain_one->name, plain[index]))
				continue;
			if (gtp->gametypes[index].br && plain_one->twin < 0)
			{
				gtp->gametypes[index].twin = other;
				plain_one->twin = index;
				break;
			}
			if (gtp->gametypes[index].evo && plain_one->evo_twin < 0)
			{
				gtp->gametypes[index].evo_twin = other;
				plain_one->evo_twin = index;
				break;
			}
		}
	}
	/* the labels: without TEAM / FFA, unless another pair's would read the same */
	for (index = 0; index < gtp->count; index++)
	{
		const char *label = without_side(plain[index]);
		int clash = 0;

		for (other = 0; other < gtp->count && !clash; other++)
		{
			if (other == index || ae_gtp_base(gtp, other) == ae_gtp_base(gtp, index))
				continue;
			clash = same_text(without_side(plain[other]), label);
		}
		snprintf(gtp->gametypes[index].row, AE_GTP_NAME, "%s", clash ? plain[index] : label);
	}
	/* the order: by folder, then place (then the glue's order) */
	for (folder = 0; folder < AE_GTP_FOLDERS; folder++)
	{
		short placed = written;

		for (index = 0; index < gtp->count; index++)
		{
			short at;

			if (gtp->gametypes[index].folder != folder)
				continue;
			for (at = written; at > placed && gtp->gametypes[gtp->sorted[at - 1]].order > gtp->gametypes[index].order; at--)
				gtp->sorted[at] = gtp->sorted[at - 1];
			gtp->sorted[at] = index;
			written++;
		}
	}
	/* (one in no folder: last, kept findable by search) */
	for (index = 0; index < gtp->count && written < gtp->count; index++)
	{
		if (gtp->gametypes[index].folder < 0 || gtp->gametypes[index].folder >= AE_GTP_FOLDERS)
			gtp->sorted[written++] = index;
	}
}

short ae_gtp_row_gametype(struct ae_gtp const *gtp, struct ae_gtp_row row)
{
	if (!gtp || row.gametype < 0)
		return -1;
	if (row.folder == AE_GTP_FOLDER_RECENT)
		return row.gametype < gtp->recent_count ? gtp->recent[row.gametype] : -1;
	if (row.gametype >= gtp->count)
		return -1;
	return gtp->shown[ae_gtp_base(gtp, row.gametype)];
}

int ae_gtp_start_place(struct ae_gtp const *gtp, short gametype, int *place)
{
	short members[3], base = ae_gtp_base(gtp, gametype);
	int index, count = 0;

	if (place)
		*place = 0;
	if (base < 0)
		return 0;
	group_of(gtp, base, members);
	for (index = 0; index < 3; index++)
	{
		if (members[index] < 0)
			continue;
		if (members[index] == gametype && place)
			*place = count;
		count++;
	}
	return count;
}

int ae_gtp_has_arrows(struct ae_gtp const *gtp, short gametype)
{
	return gtp && gametype >= 0 && gametype < gtp->count && ae_gtp_start_place(gtp, gametype, NULL) > 1 &&
		gtp->gametypes[gametype].ae;
}

int ae_gtp_step(struct ae_gtp *gtp, struct ae_gtp_row row, int step)
{
	short gametype = ae_gtp_row_gametype(gtp, row), base, members[3];
	int at, tries;

	if (gametype < 0 || row.folder == AE_GTP_FOLDER_RECENT || ae_gtp_start_place(gtp, gametype, NULL) < 2)
		return 0;
	base = ae_gtp_base(gtp, gametype);
	group_of(gtp, base, members);
	for (at = 0; at < 3 && members[at] != gametype; at++)
		;
	for (tries = 0; tries < 3; tries++)
	{
		at = (at + (step < 0 ? 2 : 1)) % 3;
		if (members[at] >= 0)
			break;
	}
	gtp->shown[base] = members[at];
	return 1;
}

int ae_gtp_toggle(struct ae_gtp *gtp, struct ae_gtp_row row)
{
	return ae_gtp_step(gtp, row, 1);
}

/* ---------- the list */

int ae_gtp_suits(struct ae_gtp const *gtp, short gametype, int filter, const char *query)
{
	struct ae_gtp_gametype const *base;
	short twin;

	gametype = ae_gtp_base(gtp, gametype);
	if (gametype < 0 || filter < 0 || filter >= AE_GTP_FILTERS)
		return 0;
	base = &gtp->gametypes[gametype];
	if (filter != AE_GTF_ALL && ae_gtp_filter_of_engine(base->engine) != filter)
		return 0;
	if (!query || !query[0])
		return 1;
	twin = base->twin;
	return ae_gtp_match(base->row, query) >= 0 || ae_gtp_match(base->name, query) >= 0 ||
		ae_gtp_match(base->stored, query) >= 0 || (twin >= 0 && ae_gtp_match(gtp->gametypes[twin].name, query) >= 0) ||
		(base->evo_twin >= 0 && ae_gtp_match(gtp->gametypes[base->evo_twin].name, query) >= 0);
}

/* a base in a view folder (FAVOURITES, one of the player's) */
static int in_view(struct ae_gtp_gametype const *gametype, int folder)
{
	if (folder == AE_GTP_FOLDER_FAVOURITES)
		return gametype->favourite;
	if (ae_gtp_folder_is_own(folder))
		return (gametype->own >> (folder - AE_GTP_FOLDER_OWN) & 1u) != 0;
	return gametype->folder == folder;
}

short ae_gtp_folder_count(struct ae_gtp const *gtp, int folder, int filter, const char *query)
{
	short index, count = 0;

	if (!gtp)
		return 0;
	if (folder == AE_GTP_FOLDER_RECENT)
	{
		for (index = 0; index < gtp->recent_count; index++)
			count = (short)(count + ae_gtp_suits(gtp, gtp->recent[index], filter, query));
		return count;
	}
	for (index = 0; index < gtp->count; index++)
	{
		struct ae_gtp_gametype const *gametype = &gtp->gametypes[index];

		if (!is_base(gtp, index))
			continue;
		if (!in_view(gametype, folder))
			continue;
		count = (short)(count + ae_gtp_suits(gtp, index, filter, query));
	}
	return count;
}

short ae_gtp_count(struct ae_gtp const *gtp, int filter, const char *query)
{
	short index, count = 0;

	for (index = 0; gtp && index < gtp->count; index++)
	{
		if (is_base(gtp, index))
			count = (short)(count + ae_gtp_suits(gtp, index, filter, query));
	}
	return count;
}

static short add_row(struct ae_gtp_row *rows, short written, short maximum, short folder, short gametype)
{
	if (written >= maximum)
		return written;
	rows[written].folder = folder;
	rows[written].gametype = gametype;
	return (short)(written + 1);
}

/* a folder's header and, open, its rows (FAVOURITES and the player's folders: listed when empty, their line open) */
static short folder_rows(struct ae_gtp const *gtp, int filter, int open_folder, int folder, struct ae_gtp_row *rows,
	short written, short maximum)
{
	int view = folder == AE_GTP_FOLDER_FAVOURITES || ae_gtp_folder_is_own(folder);
	short index;

	if (!view && !ae_gtp_folder_count(gtp, folder, filter, NULL))
		return written;
	written = add_row(rows, written, maximum, (short)folder, -1);
	if (folder != open_folder)
		return written;
	if (view && !ae_gtp_folder_count(gtp, folder, AE_GTF_ALL, NULL))
		return add_row(rows, written, maximum, (short)folder, AE_GTP_ROW_EMPTY);
	for (index = 0; index < gtp->count; index++)
	{
		short gametype = gtp->sorted[index];

		if (is_base(gtp, gametype) && in_view(&gtp->gametypes[gametype], folder) &&
			ae_gtp_suits(gtp, gametype, filter, NULL))
			written = add_row(rows, written, maximum, (short)folder, gametype);
	}
	return written;
}

short ae_gtp_rows(struct ae_gtp const *gtp, int filter, const char *query, int open_folder, struct ae_gtp_row *rows,
	short maximum)
{
	short written = 0, index;
	int folder;

	if (!gtp || !rows || maximum <= 0)
		return 0;
	if (query && query[0])
	{
		for (index = 0; index < gtp->count; index++)
		{
			short gametype = gtp->sorted[index];

			if (is_base(gtp, gametype) && ae_gtp_suits(gtp, gametype, filter, query))
				written = add_row(rows, written, maximum, gtp->gametypes[gametype].folder, gametype);
		}
		return written;
	}
	if (ae_gtp_folder_count(gtp, AE_GTP_FOLDER_RECENT, filter, NULL))
	{
		written = add_row(rows, written, maximum, AE_GTP_FOLDER_RECENT, -1);
		for (index = 0; !gtp->recent_folded && index < gtp->recent_count; index++)
		{
			if (ae_gtp_suits(gtp, gtp->recent[index], filter, NULL))
				written = add_row(rows, written, maximum, AE_GTP_FOLDER_RECENT, index);
		}
	}
	/* FAVOURITES, the player's folders (listed when empty), then the homes (left out when nothing suits) */
	written = folder_rows(gtp, filter, open_folder, AE_GTP_FOLDER_FAVOURITES, rows, written, maximum);
	for (folder = 0; folder < gtp->own_count; folder++)
		written = folder_rows(gtp, filter, open_folder, AE_GTP_FOLDER_OWN + folder, rows, written, maximum);
	for (folder = AE_GTP_FOLDER_FIRST_HOME; folder < AE_GTP_FOLDERS; folder++)
		written = folder_rows(gtp, filter, open_folder, folder, rows, written, maximum);
	return written;
}

static int home_header(struct ae_gtp_row row)
{
	return row.gametype == -1 && row.folder != AE_GTP_FOLDER_RECENT;
}

short ae_gtp_folder_step(struct ae_gtp_row const *rows, short count, short index, short direction)
{
	short step, at;

	if (!rows || count <= 0)
		return -1;
	if (index < 0 || index >= count)
		index = direction > 0 ? -1 : count;
	for (step = 1; step <= count; step++)
	{
		at = (short)(((index + (direction > 0 ? step : -step)) % count + count) % count);
		if (home_header(rows[at]))
			return at;
	}
	return -1;
}

short ae_gtp_header_row(struct ae_gtp_row const *rows, short count, int folder)
{
	short index;

	for (index = 0; rows && index < count; index++)
	{
		if (rows[index].gametype == -1 && rows[index].folder == folder)
			return index;
	}
	return -1;
}

void ae_gtp_recent_put(struct ae_gtp *gtp, short gametype)
{
	short index;

	if (!gtp || gametype < 0 || gametype >= gtp->count || gtp->recent_count >= AE_GTP_RECENT)
		return;
	for (index = 0; index < gtp->recent_count; index++)
	{
		/* (the same battle rifle pair: the newer half, put first, is the row; the older half is left out) */
		if (gtp->recent[index] == gametype || ae_gtp_base(gtp, gtp->recent[index]) == ae_gtp_base(gtp, gametype))
			return;
	}
	gtp->recent[gtp->recent_count++] = gametype;
}

void ae_gtp_favourite(struct ae_gtp *gtp, short gametype)
{
	short base = ae_gtp_base(gtp, gametype);

	if (base >= 0)
		gtp->gametypes[base].favourite = !gtp->gametypes[base].favourite;
}

/* ---------- PLAYS WELL ON */

static int engine_mode(int engine)
{
	switch (engine)
	{
	case AE_SETUP_ENGINE_SLAYER: return AE_SETUP_MODE_SLAYER;
	case AE_SETUP_ENGINE_CTF: return AE_SETUP_MODE_CTF;
	case AE_SETUP_ENGINE_KING: return AE_SETUP_MODE_KING;
	case AE_SETUP_ENGINE_ODDBALL: return AE_SETUP_MODE_ODDBALL;
	/* (Bomb's goals are the CTF flags, its bomb placed between them) */
	case AE_SETUP_ENGINE_BOMB: return AE_SETUP_MODE_CTF;
	}
	return 0;
}

/* a map's size for a side's players: small and medium maps for 1 or 2 a side (and free for all), medium for 4, large
for 8 */
static int size_suits(char size, int team)
{
	if (team >= 8)
		return size == 'l';
	if (team >= 4)
		return size == 'm';
	return size == 's' || size == 'm';
}

short ae_gtp_plays_well(struct ae_setup_maps const *maps, int engine, int team, unsigned int const *prefer_groups,
	short *indices, short maximum, short *have, short *read)
{
	int mode = engine_mode(engine), pass;
	short index, count = 0;

	if (have)
		*have = 0;
	if (read)
		*read = 0;
	if (!maps)
		return 0;
	for (index = 0; index < maps->count; index++)
	{
		struct ae_setup_map const *map = &maps->maps[index];

		if (map->facts != AE_SETUP_FACTS_READ)
			continue;
		if (read)
			(*read)++;
		if (have && (map->modes & mode))
			(*have)++;
	}
	if (!mode || !indices)
		return 0;
	for (pass = 0; pass < 2; pass++)
	{
		for (index = 0; index < maps->count && count < maximum; index++)
		{
			struct ae_setup_map const *map = &maps->maps[index];
			int preferred = 0, seen = 0, word;
			short at;

			for (word = 0; prefer_groups && word < AE_SETUP_MAP_GROUP_WORDS; word++)
				preferred |= (map->groups[word] & prefer_groups[word]) != 0;

			if (map->facts != AE_SETUP_FACTS_READ || !(map->modes & mode) || !size_suits(map->size, team) ||
				preferred != (pass == 0))
				continue;
			/* (a map listed twice by name, as NHE's Battle Creek: once) */
			for (at = 0; at < count && !seen; at++)
				seen = same_text(maps->maps[indices[at]].name, map->name);
			if (!seen)
				indices[count++] = index;
		}
	}
	return count;
}
