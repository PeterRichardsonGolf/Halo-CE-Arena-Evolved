/*
AE_MAP_LIST_GATE.C

Skips a remaking of the menus' map list while nothing it lists has changed
(ae_map_list_gate.h, ae_map_list_gate_rules.h). A making lists the maps'
folders, reads headers and sorts (milliseconds, more with a large library); the
menus ask for one on every picker opened and on every map name a screen looks
for in vain, a picture's lookup among them (three names a map).

The folders are those the making lists through FindFirstFileA (every map
family, the community maps, the packs, AE's map folders), noted on the thread
making it. MCC's maps are listed by their own code: their part of the inputs is
the modification time of the install's maps folder and the Workshop catalog's
generation (ae_mcc_workshop_generation: it moves whenever any rescan changes
the catalog).

Two checks:
- A REMAKING asked for (ui_map_list_refresh) rescans the Workshop first, then
  compares the folders and the inputs; it is skipped only while the last full
  making is less than 10 seconds old.
- A NAME LOOKED UP IN VAIN (ui_map_list_lookup) compares the folders and the
  inputs without a rescan, once a frame at most, with no age limit.
custom_edition_maps_look_again (after a map is installed, renamed, moved or
deleted) forgets the gate: the next remaking is a full one.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/posix.h"
#include "../src/ae_mcc.h"
#include "../src/ae_draw.h"
#include "ae_glue_mcc_maps.h"
#include "ae_map_list_gate.h"
#include "ae_map_list_gate_rules.h"

char const *cache_files_mod_started(void);
/* (halo_map_packs.h: the pack whose own menus are in use, NULL for none) */
char const *map_pack_menus(void);
/* (platform.h: the Custom Edition tag cache mapped) */
extern int platform_ce_tag_cache_ready;
unsigned long system_milliseconds(void);

static struct ae_map_list_gate gate;
static struct ae_map_list_missing missing;
/* (the thread making the list: only its folders are noted) */
static __thread int noting;
/* (set from any thread by ae_map_list_gate_forget; cleared as a full making begins) */
static int forgotten; /* (set from map share's threads: read and written atomically) */
/* (each full making's end: the lookup check's memo is kept for one making only) */
static unsigned long makings;

static int folder_state(const char *path, struct ae_map_list_gate_times *times)
{
	struct posix_file_information information;

	memset(times, 0, sizeof(*times));
	if (posix_stat(path, &information) != 0 || !(information.flags & _posix_file_is_directory))
		return 0;
	times->modified_seconds = information.modification_seconds;
	times->modified_nanoseconds = information.modification_nanoseconds;
	times->created_seconds = information.creation_seconds;
	times->created_nanoseconds = information.creation_nanoseconds;
	return 1;
}

static unsigned long long times_number(struct ae_map_list_gate_times const *times)
{
	return (times->modified_seconds * 1000000000ull + times->modified_nanoseconds) * 31ull +
		times->created_seconds * 1000000000ull + times->created_nanoseconds;
}

/* MCC's part of the inputs; rescan: the Workshop rescanned first (the remaking's check) */
static unsigned long long mcc_inputs(unsigned long long digest, int rescan)
{
	char const *root;

	if (!ae_mcc_menu_on())
		return ae_map_list_gate_rules_digest(digest, "mcc off", 0);
	root = ae_mcc_root();
	if (root)
	{
		char maps[AE_MCC_PATH_SIZE];
		struct ae_map_list_gate_times times;
		int present;

		snprintf(maps, sizeof(maps), "%s/halo1/maps", root);
		present = folder_state(maps, &times);
		digest = ae_map_list_gate_rules_digest(digest, maps, present ? (long long)times_number(&times) : -1);
	}
	if (rescan)
		ae_mcc_workshop_rescan();
	return ae_map_list_gate_rules_digest(digest, "workshop", (long long)ae_mcc_workshop_generation());
}

/* the list's inputs other than its folders */
static unsigned long long inputs_now(int rescan)
{
	unsigned long long digest = ae_map_list_gate_rules_digest(0, map_pack_menus(), platform_ce_tag_cache_ready);

	digest = ae_map_list_gate_rules_digest(digest, cache_files_mod_started(), 0);
	return mcc_inputs(digest, rescan);
}

static int folders_same(void)
{
	int index;

	for (index = 0; index < gate.count; index++)
	{
		struct ae_map_list_gate_times times;
		int present = folder_state(gate.folders[index].path, &times);

		if (!ae_map_list_gate_rules_folder_same(&gate.folders[index], present, &times))
			return 0;
	}
	return 1;
}

/* the lookup's check: the folders and the inputs unchanged, without a rescan; worked out once a frame (no longer than
250 ms either way, for a frame count that stands still) */
static int lookup_holds(void)
{
	static struct
	{
		int known;
		int holds;
		unsigned int frame;
		unsigned long time;
		unsigned long makings;
	} memo;
	unsigned int frame = ae_draw_frame();
	unsigned long now = system_milliseconds();

	if (!gate.valid || gate.recording || __atomic_load_n(&forgotten, __ATOMIC_ACQUIRE))
		return 0;
	if (!memo.known || memo.frame != frame || now - memo.time >= 250 || memo.makings != makings)
	{
		memo.holds = folders_same() && inputs_now(0) == gate.inputs;
		memo.known = 1;
		memo.frame = frame;
		memo.time = now;
		memo.makings = makings;
	}
	return memo.holds;
}

int ae_map_list_gate_unchanged(void)
{
	if (!gate.valid || gate.recording || __atomic_load_n(&forgotten, __ATOMIC_ACQUIRE) ||
		!ae_map_list_gate_rules_young(&gate, system_milliseconds(), AE_MAP_LIST_GATE_MAXIMUM_AGE_MILLISECONDS))
	{
		return 0;
	}
	return folders_same() && inputs_now(1) == gate.inputs;
}

void ae_map_list_gate_begin(void)
{
	__atomic_store_n(&forgotten, 0, __ATOMIC_RELEASE);
	ae_map_list_gate_rules_begin(&gate, (unsigned long long)time(NULL));
	noting = 1;
}

void ae_map_list_gate_end(void)
{
	noting = 0;
	/* (the making listed MCC's maps after its own Workshop rescan: no other here) */
	ae_map_list_gate_rules_end(&gate, inputs_now(0), system_milliseconds());
	/* (forgotten while it was made: what it listed may already be old) */
	if (__atomic_load_n(&forgotten, __ATOMIC_ACQUIRE))
		gate.valid = 0;
	makings++;
	ae_map_list_missing_rules_clear(&missing);
}

void ae_map_list_gate_forget(void)
{
	__atomic_store_n(&forgotten, 1, __ATOMIC_RELEASE);
}

void ae_map_list_gate_note_directory(const char *host_path, int opened)
{
	struct ae_map_list_gate_times times;
	int present = 0;

	if (!noting || !host_path)
		return;
	memset(&times, 0, sizeof(times));
	if (opened)
	{
		present = folder_state(host_path, &times);
		/* (listed, yet its time unknown: a change to it could not be seen) */
		if (!present)
			ae_map_list_gate_rules_untrusted(&gate);
	}
	ae_map_list_gate_rules_note(&gate, host_path, present, &times);
}

int ae_map_list_missing_known(const char *name)
{
	if (__atomic_load_n(&forgotten, __ATOMIC_ACQUIRE) || !ae_map_list_missing_rules_has(&missing, name))
		return 0;
	/* (a gate not trusted, a folder too new or too many: the last full making stands for 2 s) */
	if (!gate.valid)
	{
		return ae_map_list_gate_rules_young(&gate, system_milliseconds(),
			AE_MAP_LIST_MISSING_UNTRUSTED_MILLISECONDS);
	}
	/* (a missing name stands at most as long as a full making does: a map that came in where no folder time moves is
	found within that time) */
	return ae_map_list_gate_rules_young(&gate, system_milliseconds(), AE_MAP_LIST_GATE_MAXIMUM_AGE_MILLISECONDS) &&
		lookup_holds();
}

void ae_map_list_missing_add(const char *name)
{
	ae_map_list_missing_rules_add(&missing, name);
}
