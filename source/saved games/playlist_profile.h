/*
PLAYLIST_PROFILE.H

header included in hcex build.
*/

#ifndef __PLAYLIST_PROFILE_H
#define __PLAYLIST_PROFILE_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct game_variant;

/* ---------- prototypes/PLAYLIST_PROFILE.C */

void playlist_profiles_initialize(
	void);
void playlist_profiles_dispose(
	void);
word playlist_profile_number_of_default_profiles_on_disk(
	void);
boolean playlist_profile_get(
	long playlist_profile_index,
	struct game_variant *variant);
long playlist_profile_new(
	short local_player_index,
	wchar_t *name);
void playlist_profile_save(
	long playlist_profile_index,
	struct game_variant *variant);
/* port: a gametype's PC options (game_engine.h), and a gametype saved with
them */
struct game_variant_options;
boolean playlist_profile_get_options(
	long playlist_profile_index,
	struct game_variant_options *options);
void playlist_profile_save_with_options(
	long playlist_profile_index,
	struct game_variant *variant,
	struct game_variant_options const *options);
/* port: whether a gametype's file is exactly what saving this variant with
these options writes */
boolean playlist_profile_matches(
	long playlist_profile_index,
	struct game_variant const *variant,
	struct game_variant_options const *options);
/* port: the file's block (SAVED_GAME_FILE_BLOCK_SIZE bytes) that saving this
variant with these options writes, as playlist_profile_matches compares */
void playlist_profile_expected_block(
	struct game_variant const *variant,
	struct game_variant_options const *options,
	unsigned char *block);
/* port: ... in the 'GPVO' block's version given: 2, this build's, or 1, as
builds before OpenCE's gametypes (Arena Evolved 0.3.8 and before) wrote it;
FALSE (nothing written) for version 1 when the options of OpenCE's gametypes
are not their defaults, which no such build wrote */
boolean playlist_profile_expected_block_version(
	struct game_variant const *variant,
	struct game_variant_options const *options,
	short version,
	unsigned char *block);
/* port: a gametype's own display name (the longer name of a player's own
gametype, an 'AEDN' block in its file, playlist_display_name.h): TRUE, and
the name (32 characters at least), when it has one */
boolean playlist_profile_get_own_display_name(
	long playlist_profile_index,
	wchar_t *name);
/* port: sets it (31 characters at most; empty or NULL: none), writing only
that block. TRUE when written */
boolean playlist_profile_set_own_display_name(
	long playlist_profile_index,
	wchar_t const *name);
/* port: the own display name only when the file's variant is exactly this one (the one in play) */
boolean playlist_profile_get_own_display_name_for_variant(
	long playlist_profile_index,
	struct game_variant const *variant,
	wchar_t *name);
/* port: a file changed outside playlist_profile.c (a seed's update in place) */
void playlist_profile_content_changed(
	void);
/* port: gametype files read whole so far (the tests' count) */
long playlist_profile_block_reads_get(
	void);
/* port: counts the changes of gametype files' content */
long playlist_profile_content_generation_get(
	void);
/* port: the asynchronous write (playlist_profile_save) finished */
void playlist_profile_wait_for_write(
	void);
void playlist_profile_delete(
	long playlist_profile_index);
boolean playlist_profile_get_from_path(
	char *full_path,
	struct game_variant *variant);
/* port: ... from the file's bytes, read already, and its PC options too
(playlist_profile.c) */
boolean playlist_profile_get_from_data_with_options(
	byte const *data,
	unsigned long size,
	struct game_variant *variant,
	struct game_variant_options *options);
boolean playlist_profile_get_display_name(
	long playlist_profile_index,
	wchar_t *display_name);
/* port: a default gametype's name: the map's string list's (string_list_index:
ui\default_multiplayer_game_setting_names), else the port's own built-ins'
(Bomb, Grifball); NULL for none */
wchar_t const *playlist_profile_default_name(
	short profile_index,
	long string_list_index);
void playlist_profiles_enumerate_available_to_local_player_index(
	short local_player_index,
	word *number_of_profiles,
	long *playlist_profile_indices);

/* ---------- globals */

/* ---------- public code */

#endif // __PLAYLIST_PROFILE_H
