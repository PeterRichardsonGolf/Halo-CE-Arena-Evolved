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
/* port: the asynchronous write (playlist_profile_save) finished */
void playlist_profile_wait_for_write(
	void);
void playlist_profile_delete(
	long playlist_profile_index);
boolean playlist_profile_get_from_path(
	char *full_path,
	struct game_variant *variant);
boolean playlist_profile_get_display_name(
	long playlist_profile_index,
	wchar_t *display_name);
void playlist_profiles_enumerate_available_to_local_player_index(
	short local_player_index,
	word *number_of_profiles,
	long *playlist_profile_indices);

/* ---------- globals */

/* ---------- public code */

#endif // __PLAYLIST_PROFILE_H
