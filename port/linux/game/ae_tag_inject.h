/*
AE_TAG_INJECT.H

(Arena Evolved, DEV MODE only for now) Tags taken out of a Custom Edition map or an MCC map as bundles, into a tag
injector profile, and the profile's tags added to the levels loaded after: ae_tag_inject.c, over the tag injector of
the Custom Edition copy (cev/tag_injector.c). Nothing here does anything until DEV MODE's `inject` command is used.
*/

#ifndef __AE_TAG_INJECT_H
#define __AE_TAG_INJECT_H

/* ---------- the level's (called by ae_tag_import_spike.c from the cache file's hooks) */

/* a level's tags loaded: the profile's tags added, when the command turned them on (never to the main menu's map) */
void ae_tag_inject_tags_loaded(
	char const *map_name);
void ae_tag_inject_tags_unloaded(
	void);
/* a cache file read of a tag the profile added: TRUE when it was read */
boolean ae_tag_inject_read(
	long tag_index,
	long offset,
	long size,
	void *buffer);
/* (players.c) the biped last taken, when the level has it; else the one given */
long ae_tag_inject_player_biped(
	long unit_definition_index);

/* ---------- DEV MODE's command (ae_dev_cmd.c): inject status|maps|close|on|off|clear, inject open <level>,
inject list <category>, inject take <category> <number or part of a path>. The answer (one line) into answer; FALSE when refused */
boolean ae_tag_inject_command(
	int words,
	char const *verb,
	char const *first,
	char const *second,
	char *answer,
	unsigned int answer_size);

#endif /* __AE_TAG_INJECT_H */
