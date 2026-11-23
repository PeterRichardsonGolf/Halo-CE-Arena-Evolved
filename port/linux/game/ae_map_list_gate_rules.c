/* AE_MAP_LIST_GATE_RULES.C: when the menus' map list must be made again (ae_map_list_gate_rules.h) */

#include <stddef.h>
#include <string.h>

#include "ae_map_list_gate_rules.h"

void ae_map_list_gate_rules_begin(struct ae_map_list_gate *gate, unsigned long long started)
{
	gate->valid = 0;
	gate->recording = 1;
	gate->overflow = 0;
	gate->count = 0;
	gate->inputs = 0;
	gate->started = started;
	gate->made_known = 0;
	gate->made = 0;
}

void ae_map_list_gate_rules_note(struct ae_map_list_gate *gate, const char *path, int present,
	struct ae_map_list_gate_times const *times)
{
	static struct ae_map_list_gate_times const none;

	struct ae_map_list_gate_folder *folder;
	int index;

	if (!gate->recording || !path)
		return;
	if (strlen(path) >= AE_MAP_LIST_GATE_PATH)
	{
		gate->overflow = 1;
		return;
	}
	for (index = 0; index < gate->count; index++)
	{
		if (!strcmp(gate->folders[index].path, path))
			return;
	}
	if (gate->count >= AE_MAP_LIST_GATE_FOLDERS)
	{
		gate->overflow = 1;
		return;
	}
	folder = &gate->folders[gate->count++];
	strcpy(folder->path, path);
	folder->present = present != 0 && times != NULL;
	folder->times = folder->present ? *times : none;
}

void ae_map_list_gate_rules_untrusted(struct ae_map_list_gate *gate)
{
	if (gate->recording)
		gate->overflow = 1;
}

void ae_map_list_gate_rules_end(struct ae_map_list_gate *gate, unsigned long long inputs, unsigned long now)
{
	int index;

	gate->recording = 0;
	gate->inputs = inputs;
	gate->made_known = 1;
	gate->made = now;
	gate->valid = !gate->overflow;
	for (index = 0; index < gate->count && gate->valid; index++)
	{
		struct ae_map_list_gate_times const *times = &gate->folders[index].times;
		unsigned long long latest = times->modified_seconds > times->created_seconds ? times->modified_seconds :
			times->created_seconds;

		if (gate->folders[index].present && latest + AE_MAP_LIST_GATE_SETTLE_SECONDS >= gate->started)
			gate->valid = 0;
	}
}

int ae_map_list_gate_rules_folder_same(struct ae_map_list_gate_folder const *folder, int present,
	struct ae_map_list_gate_times const *times)
{
	if ((present != 0 && times != NULL) != folder->present)
		return 0;
	return !folder->present || (times->modified_seconds == folder->times.modified_seconds &&
		times->modified_nanoseconds == folder->times.modified_nanoseconds &&
		times->created_seconds == folder->times.created_seconds &&
		times->created_nanoseconds == folder->times.created_nanoseconds);
}

int ae_map_list_gate_rules_young(struct ae_map_list_gate const *gate, unsigned long now, unsigned long maximum)
{
	return !gate->recording && gate->made_known && now - gate->made < maximum;
}

unsigned long long ae_map_list_gate_rules_digest(unsigned long long seed, const char *text, long long number)
{
	unsigned long long hash = seed ^ 0xCBF29CE484222325ull;
	int byte;

	for (; text && *text; text++)
	{
		hash ^= (unsigned char)*text;
		hash *= 0x100000001B3ull;
	}
	/* (an end to the text, so "a" then 1 and "a1" differ) */
	hash ^= 0xFFu;
	hash *= 0x100000001B3ull;
	for (byte = 0; byte < 8; byte++)
	{
		hash ^= (unsigned long long)(number >> (byte * 8)) & 0xFFu;
		hash *= 0x100000001B3ull;
	}
	return hash;
}

void ae_map_list_missing_rules_clear(struct ae_map_list_missing *missing)
{
	missing->count = 0;
	missing->next = 0;
}

void ae_map_list_missing_rules_add(struct ae_map_list_missing *missing, const char *name)
{
	if (!name || !name[0] || strlen(name) >= AE_MAP_LIST_MISSING_NAME ||
		ae_map_list_missing_rules_has(missing, name))
	{
		return;
	}
	strcpy(missing->names[missing->next], name);
	missing->next = (missing->next + 1) % AE_MAP_LIST_MISSING_NAMES;
	if (missing->count < AE_MAP_LIST_MISSING_NAMES)
		missing->count++;
}

int ae_map_list_missing_rules_has(struct ae_map_list_missing const *missing, const char *name)
{
	int index;

	if (!name)
		return 0;
	for (index = 0; index < missing->count; index++)
	{
		if (!strcmp(missing->names[index], name))
			return 1;
	}
	return 0;
}
