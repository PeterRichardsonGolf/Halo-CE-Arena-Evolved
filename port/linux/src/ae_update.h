/*
AE_UPDATE.H

Arena Evolved's part of the self-updater (updater.c includes this, AE hooks
there; port/linux/game/ae_hooks.txt): an Arena Evolved release's zip is not
the flat folder ChupathingyCE's is. Beside the game's files at its top it
has folders of the game's own that a new version changes: packs/ (the weapon
packs, *.aepack and their credits), voices/<voice>/ (the callout voices) and
sounds/ (the horn): tools/ci_build.py and the release kit put them there. The
upstream updater leaves out every entry in a folder, so an update would have
installed a new halo with the old version's weapon packs. Here:

- ae_update_entry_allowed: which of the zip's entries are installed. A file
  at the top as before (letters, digits and . _ + -, not starting with a dot,
  not one of the game's own folders' names), or a file in one of
  AE_UPDATE_FOLDERS, at most AE_UPDATE_DEPTH folders down, each part of its
  path named the same way. A folder's own entry ("packs/") is not a file;
  nothing outside those folders is ever written (data/, logs/, cache/,
  private/, maps/ and the player's own folders stay as they are).
- ae_update_make_parents: the folders a file of the zip goes in, in
  update.partial/ and in the game's folder.
- ae_update_clean_up (at the start, after the upstream clean-up of its own
  names): what an update leaves behind. Each file <name>.old beside a file
  <name> at the top or in AE_UPDATE_FOLDERS (update_replace_file keeps the
  file it replaces as <name>.old, and a running executable cannot be deleted
  until it has quit), and update.partial/ with the folders in it.

The pure part (ae_update_entry_allowed) is also built alone for the unit
test, port/linux/tests/ae_updater_pick_test.c (AE_UPDATER_PICK_TEST).
*/

#ifndef AE_UPDATE_H
#define AE_UPDATE_H

/* (the updater's test build reads its server and certificate authority from
the environment: never a release's) */
#if defined(HALO_UPDATE_TEST) && defined(HALO_RELEASE_BUILD)
#if HALO_RELEASE_BUILD
#error "a release build is never the updater's test build (HALO_UPDATE_TEST)"
#endif
#endif

#include <string.h>

/* the folders of an Arena Evolved release's zip that an update installs */
static const char *const ae_update_folders[] = { "packs", "voices", "sounds" };
#define AE_UPDATE_DEPTH 2

/* whether one part of an entry's path is a name a release gives a file or a
folder: letters, digits and . _ + -, not starting with a dot */
static int ae_update_part_allowed(const char *part, size_t length)
{
	size_t index;

	if (!length || part[0] == '.')
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = part[index];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
			c == '+' || c == '-'))
		{
			return 0;
		}
	}
	return 1;
}

static int ae_update_same_word(const char *left, size_t length, const char *right)
{
	size_t index;

	if (strlen(right) != length)
		return 0;
	for (index = 0; index < length; index++)
	{
		char a = left[index], b = right[index];

		if (a >= 'A' && a <= 'Z')
			a = (char)(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z')
			b = (char)(b - 'A' + 'a');
		if (a != b)
			return 0;
	}
	return 1;
}

/* whether the zip's entry name is a file an update installs (above) */
static int ae_update_entry_allowed(const char *name)
{
	static const char *const own_folders[] = { "data", "logs", "cache", "private", "maps", "gametypes" };
	const char *part = name, *slash;
	size_t index, length;
	int depth = 0;

	if (!name || !*name)
		return 0;
	slash = strchr(part, '/');
	length = slash ? (size_t)(slash - part) : strlen(part);
	if (!ae_update_part_allowed(part, length))
		return 0;
	if (!slash)
	{
		/* (a file at the top: never one by a folder's name of the game's) */
		for (index = 0; index < sizeof(own_folders) / sizeof(*own_folders); index++)
		{
			if (ae_update_same_word(part, length, own_folders[index]))
				return 0;
		}
		return 1;
	}
	for (index = 0; index < sizeof(ae_update_folders) / sizeof(*ae_update_folders); index++)
	{
		if (ae_update_same_word(part, length, ae_update_folders[index]))
			break;
	}
	if (index == sizeof(ae_update_folders) / sizeof(*ae_update_folders))
		return 0;
	/* (each part below it, the last a file's: no empty part, no "..", no
	backslash, no deeper than AE_UPDATE_DEPTH folders) */
	for (part = slash + 1;; part = slash + 1)
	{
		slash = strchr(part, '/');
		length = slash ? (size_t)(slash - part) : strlen(part);
		if (!ae_update_part_allowed(part, length))
			return 0;
		if (!slash)
			return 1;
		if (++depth >= AE_UPDATE_DEPTH)
			return 0;
	}
}

#ifndef AE_UPDATER_PICK_TEST

/* the folders between the game's folder (the first base_length characters of
path) and the file at path, made where they are not */
static void ae_update_make_parents(const char *path, size_t base_length)
{
	char folder[1300];
	size_t index, length = strlen(path);

	if (length >= sizeof(folder) || base_length >= length)
		return;
	memcpy(folder, path, length + 1);
	for (index = base_length + 1; index < length; index++)
	{
		if (folder[index] == '/' || folder[index] == '\\')
		{
			char separator = folder[index];

			folder[index] = 0;
			update_make_directory(folder);
			folder[index] = separator;
		}
	}
}

/* whether a file of the update would be written through a link: the folder it goes in (or the file) is a symbolic
link, or on Windows a junction or another reparse point, anywhere below the game's folder (from base_length on). The
names are checked already (ae_update_entry_allowed), but a packs/ the player made a link to another folder would have
had the update write there; such an update is refused whole, before any file is replaced */
/* the platform's: posix_update.c's lstat, win32_update.c's reparse point (one AE hook line each: a system header here
would meet the game's own types in updater.c) */
int ae_update_is_link(const char *path);

static int ae_update_through_link(const char *path, size_t base_length)
{
	char part[1300];
	size_t index, length = strlen(path);

	if (length >= sizeof(part) || base_length >= length)
		return 0;
	memcpy(part, path, length + 1);
	for (index = base_length + 1; index <= length; index++)
	{
		if (index == length || part[index] == '/' || part[index] == '\\')
		{
			char separator = part[index];

			part[index] = 0;
			if (ae_update_is_link(part))
				return 1;
			part[index] = separator;
		}
	}
	return 0;
}

/* every file of the update and update.partial/ itself, before the first is replaced: 1 when none is through a link,
else 0 and the error says which */
static int ae_update_no_links(const char *directory, const char *partial, char names[][256], int name_count,
	char *error, size_t error_size)
{
	char path[1300];
	size_t base_length = strlen(directory);
	int index;

	if (ae_update_through_link(partial, base_length))
	{
		snprintf(error, error_size, "the update's folder is a link (%s)", partial);
		return 0;
	}
	for (index = 0; index < name_count; index++)
	{
#ifdef _WIN32
		snprintf(path, sizeof(path), "%s\\%s", directory, names[index]);
#else
		snprintf(path, sizeof(path), "%s/%s", directory, names[index]);
#endif
		if (ae_update_through_link(path, base_length))
		{
			snprintf(error, error_size, "%s goes through a link: nothing installed", names[index]);
			return 0;
		}
	}
	return 1;
}

struct ae_update_sweep
{
	int depth;
	int partial;
	int removed;
};

static SDL_EnumerationResult SDLCALL ae_update_sweep_entry(void *context, const char *folder, const char *name);

/* the folder at path swept (ae_update_sweep_entry) */
static void ae_update_sweep(const char *path, int depth, int partial, int *removed)
{
	struct ae_update_sweep sweep;

	sweep.depth = depth;
	sweep.partial = partial;
	sweep.removed = 0;
	SDL_EnumerateDirectory(path, ae_update_sweep_entry, &sweep);
	*removed += sweep.removed;
}

/* one entry of a folder swept: in update.partial/, everything (its folders
emptied first); elsewhere a <name>.old beside its <name>, and the folders of
AE_UPDATE_FOLDERS at the top, gone through the same way */
static SDL_EnumerationResult SDLCALL ae_update_sweep_entry(void *context, const char *folder, const char *name)
{
	struct ae_update_sweep *sweep = context;
	char path[1300];
	SDL_PathInfo information;
	size_t length = strlen(name);

	if (snprintf(path, sizeof(path), "%s%s", folder, name) >= (int)sizeof(path) || !SDL_GetPathInfo(path, &information))
		return SDL_ENUM_CONTINUE;
	if (information.type == SDL_PATHTYPE_DIRECTORY)
	{
		size_t index;
		int go_in = sweep->partial;

		for (index = 0; !go_in && index < sizeof(ae_update_folders) / sizeof(*ae_update_folders); index++)
			go_in = sweep->depth > 0 || ae_update_same_word(name, length, ae_update_folders[index]);
		if (go_in && sweep->depth <= AE_UPDATE_DEPTH)
		{
			int removed = 0;

			ae_update_sweep(path, sweep->depth + 1, sweep->partial, &removed);
			sweep->removed += removed;
			if (sweep->partial && SDL_RemovePath(path))
				sweep->removed++;
		}
		return SDL_ENUM_CONTINUE;
	}
	if (sweep->partial)
	{
		if (SDL_RemovePath(path))
			sweep->removed++;
	}
	else if (length > 4 && !strcmp(name + length - 4, ".old"))
	{
		char replaced[1300];

		snprintf(replaced, sizeof(replaced), "%.*s", (int)(strlen(path) - 4), path);
		if (SDL_GetPathInfo(replaced, NULL) && SDL_RemovePath(path))
			sweep->removed++;
	}
	return SDL_ENUM_CONTINUE;
}

/* what an update left behind in the game's folder (above) */
static void ae_update_clean_up(const char *directory)
{
	char partial[1300];
	int removed = 0;

	snprintf(partial, sizeof(partial), "%s/update.partial", directory);
	if (SDL_GetPathInfo(partial, NULL))
	{
		ae_update_sweep(partial, 0, 1, &removed);
		if (SDL_RemovePath(partial))
			removed++;
	}
	ae_update_sweep(directory, 0, 0, &removed);
	if (removed)
		platform_log("update: %d files and folders an update left behind deleted", removed);
}

#endif

#endif
