/*
AE_PLAYLISTS_STORE.C

The playlists in memory and their files (ae_playlists_store.h).
*/

#include <stdio.h>
#include <string.h>

#include "../src/ae_private_files.h"
#include "ae_playlists_store.h"

/* (platform.h: its full header clashes with this file's; the one call needed) */
void platform_log(const char *format, ...);

#define PLAYLISTS_FILE "playlists.txt"
#define PLAYLIST_FILE "playlist.txt"

static struct ae_playlists set;
static int loaded, locked, staged;
static unsigned revision;
static char text[AE_PLAYLISTS_FILE];

/* (only "#" lines: the file the game writes when every playlist was
deleted, kept as it is: not torn, and playlist.txt must not bring the
deleted playlist back) */
static int only_comments(char const *at)
{
	while (*at)
	{
		size_t length = strcspn(at, "\n");
		size_t lead = strspn(at, " \t\r");

		if (lead < length && at[lead] != '#')
			return 0;
		at += length + (at[length] == '\n');
	}
	return 1;
}

static void load(void)
{
	char legacy[AE_PLAYLIST_TEXT];
	long length;

	loaded = 1;
	locked = 0;
	ae_playlists_init(&set);
	length = ae_private_file_read(PLAYLISTS_FILE, text, sizeof(text));
	if (length > 0)
	{
		ae_playlists_read(&set, text);
		if (set.count > 0 || !text[strspn(text, " \t\r\n")] || only_comments(text))
			return;
		/* (text that holds no playlist: hand-edited, torn or another version's, kept aside as playlists.txt.bad
		rather than written over by the next change; then as if there were none) */
		ae_playlists_init(&set);
		if (!ae_private_file_set_aside(PLAYLISTS_FILE))
		{
			locked = 1;
			return;
		}
	}
	else if (length == -2)
	{
		/* (kept as it is, never written over) */
		locked = 1;
		return;
	}
	else if (length == 0)
		return;
	else if (ae_private_file_exists(PLAYLISTS_FILE))
	{
		/* (there but unreadable: kept as it is, never written over) */
		locked = 1;
		return;
	}
	/* (no playlists.txt: today's playlist.txt is the first playlist, in use OFF as the lobby started with it) */
	length = ae_private_file_read(PLAYLIST_FILE, legacy, sizeof(legacy));
	if (length > 0 && ae_playlists_migrate(&set, legacy))
		ae_playlists_store_save();
}

struct ae_playlists *ae_playlists_store(void)
{
	if (!loaded)
		load();
	return &set;
}

int ae_playlists_store_save(void)
{
	char export_text[AE_PLAYLIST_TEXT];
	long length;

	if (!loaded || staged || locked)
		return 0;
	length = ae_playlists_write(&set, text, sizeof(text));
	if (length < 0 || !ae_private_file_write(PLAYLISTS_FILE, text, length))
		return 0;
	revision++;
	/* (playlist.txt: the in use playlist as a server's rotation; OFF leaves it as it was) */
	if (set.in_use >= 0 && set.in_use < set.count && set.lists[set.in_use].count > 0)
	{
		length = ae_playlist_write(&set.lists[set.in_use], export_text, sizeof(export_text));
		/* (a server reads playlist.txt as its rotation: say so when it is left as it was) */
		if (length < 0 || !ae_private_file_write(PLAYLIST_FILE, export_text, length))
			platform_log("playlists: %s not written (too long or not writable); a server keeps the old rotation",
				PLAYLIST_FILE);
	}
	return 1;
}

unsigned ae_playlists_store_revision(void)
{
	return revision;
}

int ae_playlists_store_locked(void)
{
	if (!loaded)
		load();
	return locked;
}

int ae_playlists_store_staged(void)
{
	return staged;
}

static void stage_games(struct ae_playlist *list, char const *const *games, int count)
{
	int at;

	for (at = 0; at < count; at++)
	{
		char map[AE_PLAYLIST_MAP];
		char const *colon = strchr(games[at], ':');

		snprintf(map, sizeof(map), "%.*s", (int)(colon - games[at]), games[at]);
		ae_playlist_add(list, map, colon + 1);
	}
}

void ae_playlists_store_stage(int kind)
{
	static char const *const arena[] = { "damnation:AE TEAM SLAY", "prisoner:AE TEAM SLAY", "hangemhigh:AE KING",
		"bloodgulch:AE CTF", "sidewinder:AE TEAM SLAY", "icefields:AE CTF", "wizard:AE KING" };
	static char const *const night[] = { "damnation:AE FFA SLAY", "hangemhigh:AE FFA SLAY", "prisoner:AE FFA SLAY",
		"wizard:AE FFA SLAY", "longest:AE FFA SLAY", "beavercreek:AE FFA SLAY" };
	static char const *const swat[] = { "damnation:AE TEAM SLAY", "bloodgulch:AE TEAM SLAY", "prisoner:AE TEAM SLAY",
		"hangemhigh:AE TEAM SLAY" };
	char old_text[AE_PLAYLIST_TEXT];
	int made;

	loaded = 1;
	locked = 0;
	revision++;
	ae_playlists_init(&set);
	if (kind == AE_PLAYLISTS_STAGE_REAL)
	{
		staged = 0;
		loaded = 0;
		return;
	}
	staged = 1;
	if (kind == AE_PLAYLISTS_STAGE_OLD)
	{
		snprintf(old_text, sizeof(old_text), "# playlist: MY PLAYLIST\n# map vote: 3\n# no repeats: 1\n"
			"damnation:file=AE TEAM SLAY  # weight 3\nhangemhigh:file=AE KING  # weight 2\n"
			"bloodgulch:file=AE CTF  # weight 1\n");
		ae_playlists_migrate(&set, old_text);
		return;
	}
	made = ae_playlists_new(&set, "4V4 ARENA", NULL);
	stage_games(&set.lists[made], arena, 7);
	set.lists[made].order = AE_PLAYLIST_ORDER_SHUFFLE;
	set.lists[made].entries[0].weight = 4;
	set.lists[made].entries[2].weight = 3;
	made = ae_playlists_new(&set, "FFA NIGHT", NULL);
	stage_games(&set.lists[made], night, 6);
	set.lists[made].order = AE_PLAYLIST_ORDER_RANDOM;
	set.lists[made].entries[1].weight = 4;
	made = ae_playlists_new(&set, "SWAT TIME", NULL);
	stage_games(&set.lists[made], swat, 4);
	set.lists[made].order = AE_PLAYLIST_ORDER_IN_TURN;
	set.in_use = 0;
}

int ae_playlists_store_host(char *line, int size, struct ae_playlist_host *host, char *name, int name_size)
{
	struct ae_playlists *all = ae_playlists_store();
	int games;

	line[0] = 0;
	if (name_size > 0)
		name[0] = 0;
	if (all->in_use < 0 || all->in_use >= all->count || all->lists[all->in_use].count <= 0)
		return 0;
	games = ae_playlist_host_rotation(&all->lists[all->in_use], line, size, host);
	if (games < 0)
		games = 0;
	snprintf(name, (size_t)name_size, "%s", all->lists[all->in_use].name);
	return games;
}
