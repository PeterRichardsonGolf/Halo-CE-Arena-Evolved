/* What the game in play names of the weapon cache (ae_weapon_selection.h's rules; design sections 7 and 8.4): the
host's chosen AECW payload (taken from the gametype it picked, as ae_variant_ext_choose_profile takes the AEXV block),
the selection a variant and its options make with it, and messages 109 (the host's cache id and selection, to every
client that told a block) and 110 (a client's own id, when it differs). */
#ifndef AE_WEAPON_SELECTION_GAME_H
#define AE_WEAPON_SELECTION_GAME_H

#include "cseries.h"
#include "game/game_engine.h"
#include "ae_weapon_selection.h"

/* the gametype the host picks: its file's AECW block (none: nothing named past the legacy bytes) */
void ae_weapon_selection_choose_profile(long playlist_profile_index);
void ae_weapon_selection_choose_bytes(struct ae_weapon_selection_bytes const *bytes);
/* a gametype read from a block that is not the chosen one yet (the server's rotation): kept until applied */
void ae_weapon_selection_pending_block(unsigned char const *block);
void ae_weapon_selection_pending_apply(void);
void ae_weapon_selection_pending_clear(void);

/* the selection of a game: the variant's legacy bytes, the options' bytes and the chosen AECW payload (a client: the
host's, from message 109) */
void ae_weapon_selection_of_game(struct game_variant const *variant, struct game_variant_options const *options,
	struct ae_weapon_selection *selection);
/* whether that game uses the cache */
boolean ae_weapon_selection_game_uses_cache(struct game_variant const *variant,
	struct game_variant_options const *options);

/* the no-cache guard (design 8.5): TRUE, with a log line naming `what`, when the host must NOT create an object of a cache
definition (loadout, layout, map spawn, bot or dev give): a network game whose selection does not use the cache. The AE
sites of those gives call it before the give; the sites arrive with the engine lane (T2-4) */
boolean ae_weapon_selection_guard(struct game_variant const *variant, struct game_variant_options const *options,
	char const *what);

/* the network (messages 109 / 110) */
void ae_weapon_selection_net_reset(void);
void ae_weapon_selection_net_send(long machine_index);
void ae_weapon_selection_net_update(void);
int ae_weapon_selection_net_handles_message(unsigned short const *message, unsigned short size);
void ae_weapon_selection_net_handle_message(long machine_index, unsigned short const *message, unsigned short size);

/* debug.network_test_cache_weapon / _set: an automated test host's game names the AE WEAPONS toggle, this set (0 casual,
1 comp, 2 BTB) and starts everyone with this weapon id (1-0xFFFF, the registry's) as the primary weapon: the harness's
way to name any cache weapon, the old pack mask and START byte stopping at slot 16. Does nothing when the weapon is 0 */
void ae_weapon_selection_network_test_options(struct game_variant_options *options);

/* the weapon id the running game's gametype names as loadout slot 0 or 1's START in its AE WEAPONS block (AECW), or 0:
the cache era's way to start with a weapon above the old 16 slots (game_engine_pack_loadout_weapon_slot) */
unsigned int ae_weapon_selection_running_start(short slot);

#endif
