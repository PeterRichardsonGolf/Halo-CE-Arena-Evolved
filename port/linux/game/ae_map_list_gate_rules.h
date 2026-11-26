/*
AE_MAP_LIST_GATE_RULES.H

When the menus' map list must be made again (ui_map_list_refresh), as pure rules
(port/linux/tests/ae_map_list_gate_rules_test.c runs them; ae_map_list_gate.c
applies them):

- THE FOLDERS: a full making of the list notes every folder it lists (its host
  path, whether it opened, its modification and creation times: on Linux the
  creation time is the status change time, which a copy that keeps the
  modification time still moves). A later making is skipped while every noted
  folder is as it was, the list's other inputs (a digest: the pack's own menus,
  the mod, the tag cache, MCC's maps) are the same, and the last full making is
  less than 10 seconds old (a file system whose folder times do not move, or a
  network share that keeps its listings a while, is caught up with then).
- SETTLED ONLY: a folder modified or created within 2 seconds of the making's
  start is not trusted (a file system's times can be that coarse): the next
  making is a full one. So is a making that noted more folders than are kept,
  or a folder that opened but whose time could not be read.
- MISSING NAMES: the names a lookup did not find since the last full making are
  kept (the last ones, a few dozen). A name looked for again is not a reason to
  make the list again while the folders and inputs are unchanged (no age limit
  here); while the gate is not trusted, not within 2 seconds of the last full
  making either, so a screen asking every frame makes the list at most that
  often.
*/

#ifndef __AE_MAP_LIST_GATE_RULES_H
#define __AE_MAP_LIST_GATE_RULES_H

enum
{
	AE_MAP_LIST_GATE_FOLDERS = 256,
	AE_MAP_LIST_GATE_PATH = 260,
	AE_MAP_LIST_GATE_SETTLE_SECONDS = 2,
	AE_MAP_LIST_GATE_MAXIMUM_AGE_MILLISECONDS = 10000,
	AE_MAP_LIST_MISSING_UNTRUSTED_MILLISECONDS = 2000,
	AE_MAP_LIST_MISSING_NAMES = 64,
	AE_MAP_LIST_MISSING_NAME = 64
};

/* a folder's times (seconds and nanoseconds since the Unix epoch) */
struct ae_map_list_gate_times
{
	unsigned long long modified_seconds;
	unsigned long modified_nanoseconds;
	unsigned long long created_seconds;
	unsigned long created_nanoseconds;
};

struct ae_map_list_gate_folder
{
	char path[AE_MAP_LIST_GATE_PATH];
	int present;
	struct ae_map_list_gate_times times;
};

struct ae_map_list_gate
{
	int valid;
	int recording;
	int overflow;
	int count;
	unsigned long long inputs;
	unsigned long long started;
	/* (the making's end, milliseconds of a clock that only runs forward) */
	int made_known;
	unsigned long made;
	struct ae_map_list_gate_folder folders[AE_MAP_LIST_GATE_FOLDERS];
};

struct ae_map_list_missing
{
	int count;
	int next;
	char names[AE_MAP_LIST_MISSING_NAMES][AE_MAP_LIST_MISSING_NAME];
};

/* a full making starts (started: its wall-clock second): nothing noted, the gate not valid until it ends */
void ae_map_list_gate_rules_begin(struct ae_map_list_gate *gate, unsigned long long started);
/* a folder the making listed (once each; a path too long or one folder too many makes the making untrusted) */
void ae_map_list_gate_rules_note(struct ae_map_list_gate *gate, const char *path, int present,
	struct ae_map_list_gate_times const *times);
/* a folder the making listed whose time can't be read: the making is not trusted (every later one is full) */
void ae_map_list_gate_rules_untrusted(struct ae_map_list_gate *gate);
/* the making ended (now: milliseconds) with these inputs: the gate valid when nothing overflowed and every folder had
settled */
void ae_map_list_gate_rules_end(struct ae_map_list_gate *gate, unsigned long long inputs, unsigned long now);
/* whether a noted folder is as it was */
int ae_map_list_gate_rules_folder_same(struct ae_map_list_gate_folder const *folder, int present,
	struct ae_map_list_gate_times const *times);
/* whether the last full making ended less than maximum milliseconds before now (0 while one is under way) */
int ae_map_list_gate_rules_young(struct ae_map_list_gate const *gate, unsigned long now, unsigned long maximum);

/* a digest of text (NULL as empty) and a number, chained from seed */
unsigned long long ae_map_list_gate_rules_digest(unsigned long long seed, const char *text, long long number);

void ae_map_list_missing_rules_clear(struct ae_map_list_missing *missing);
void ae_map_list_missing_rules_add(struct ae_map_list_missing *missing, const char *name);
int ae_map_list_missing_rules_has(struct ae_map_list_missing const *missing, const char *name);

#endif
