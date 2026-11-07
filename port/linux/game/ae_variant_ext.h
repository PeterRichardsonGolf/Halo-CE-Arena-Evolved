/* AE's extra gametype settings in play (ae_variant_ext_rules.h): the 'AEXV' block of a gametype file, the
host's chosen settings, and message 105 that gives them to every client. */
#ifndef AE_VARIANT_EXT_H
#define AE_VARIANT_EXT_H

#include "ae_variant_ext_rules.h"

enum
{
	/* after the 'AEDN' block (0x150 + 92 = 0x1AC): header 8, the settings, a 20-byte signature: to 0x1FC */
	AE_VARIANT_EXT_OFFSET = 0x1B0,
	AE_VARIANT_EXT_MAGIC = 0x56584541, /* 'AEXV' little-endian */
	AE_VARIANT_EXT_BLOCK_SIZE = 8 + AE_VARIANT_EXT_SIZE + 20,
};

/* the block in a gametype file's 512 bytes: 1 and ext (sanitized) when it holds a whole one, else 0 and the
default */
int ae_variant_ext_from_block(unsigned char const *block, struct ae_variant_ext *ext);
/* written into the 512 bytes; the default clears its place (no block) */
void ae_variant_ext_to_block(unsigned char *block, struct ae_variant_ext const *ext);
/* the block of old_block (when whole) written into block: every save carries it over */
void ae_variant_ext_carry(unsigned char *block, unsigned char const *old_block);

/* a gametype file's settings (playlist_profile.c): 1 when it holds a block */
int playlist_profile_get_ext(long playlist_profile_index, struct ae_variant_ext *ext);
/* only the block written (a player's own gametype; built-ins are read-only): 1 when written */
int playlist_profile_set_ext(long playlist_profile_index, struct ae_variant_ext const *ext);

/* the host's (or an offline game's) settings, from the gametype it chose: with its options, wherever the game
takes a gametype (AE hooks) */
void ae_variant_ext_choose(struct ae_variant_ext const *ext);
void ae_variant_ext_choose_profile(long playlist_profile_index);
/* a gametype read from a file or a block that is not the chosen one yet (the server's rotation, the network test): the
settings its block holds wait here until the variant is set (player_ui_set_game_variant picks a preset's by name, which
the block of a player's own copy replaces): pending_block keeps them (a block without any is Halo CE's rules, as a saved gametype
without one plays), pending_profile reads a saved gametype's, pending_apply chooses them and forgets them, pending_clear forgets */
void ae_variant_ext_pending_block(unsigned char const *block);
void ae_variant_ext_pending_profile(long playlist_profile_index);
void ae_variant_ext_pending_apply(void);
void ae_variant_ext_pending_clear(void);
/* player_ui_set_game_variant with the settings already chosen kept (the host's setup screen applying its copy) */
void ae_variant_ext_set_variant_keeping(struct game_variant *variant);
/* the settings the game plays: the chosen ones on the host or offline, the host's on a client (its message; the
default until it comes) */
struct ae_variant_ext const *ae_variant_ext_game(void);

/* the network: a client's copy cleared (a new connection), the host's sent when it changes and to a machine
that is ready */
void ae_variant_ext_reset(void);
void ae_variant_ext_update(void);
void ae_variant_ext_send(long machine_index);
int ae_variant_ext_handles_message(unsigned short const *message, unsigned short size);
void ae_variant_ext_handle_message(long machine_index, unsigned short const *message, unsigned short size);

#endif
