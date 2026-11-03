/*
AE_SILENT.H

SILENT MOVEMENT and SILENT WEAPONS (the rules: ae_silent_rules.h): what the AE
hook lines call (port/linux/game/ae_hooks.txt lists them). Included after
cseries.h (boolean, real, word).

- Tagging: a creation site opens a scope (role, acting player) around the
  call that makes its sounds or effects; sound_new_impulse records every
  sound it makes with the innermost scope, effect_allocate every effect, and
  an effect's parts are made later inside its own recorded scope.
- Muting: update_channel_for_impulse_sound multiplies its gain by
  ae_silent_gain, 0 for a muted sound, at the listener. Nothing else changes.
- Joining: the client marks its join request (ae_silent_join_request_mark),
  the host reads the mark (ae_silent_join_request_heard) and, while the
  game's gametype has either option on, refuses a machine without it at its
  join and drops one already in the game.
*/

#ifndef __AE_SILENT_H
#define __AE_SILENT_H

#include "ae_silent_rules.h"

struct game_variant;
struct universal_variant;
struct widget_instance;

/* the gametype's byte: the high nibble of universal_variant's pad5 (a branch
naming that byte for another use must name it here too; ae_silent_rules.h).
Bits 0-1 are COMBAT FEEDBACK's (ae_hit_feedback_rules.h) */
#define AE_SILENT_VARIANT_BYTE(variant) ((variant)->universal_variant.pad5)

/* ---- tagging the sounds */

/* a scope of a role (enum in ae_silent_rules.h) and an acting player (a player
handle, NONE for none); every begin is ended */
void ae_silent_scope_begin(int role, long player_index);
void ae_silent_scope_end(void);
/* a weapon's ready / put-away (its holder's player), else no role */
void ae_silent_weapon_scope_begin(long weapon_index, boolean ready_or_put_away);
/* a unit's (third-person) action animation: actions 3 and 4 are ready and put away */
void ae_silent_unit_action_scope_begin(long unit_index, long action);
/* an effect allocated, and its parts made in its own scope */
void ae_silent_effect_created(long effect_index);
void ae_silent_effect_scope_begin(long effect_index);
/* an impulse sound made (NONE: none) */
void ae_silent_sound_created(long sound_index);
/* the gain of an impulse sound for its listener: 0 muted, else 1 (starting:
the sound's channel starts now, counted once) */
real ae_silent_gain(long sound_index, short listener_index, boolean starting);
/* the debug.txt lines: the game's options (at a game's start) and the counts
(at its end) */
void ae_silent_log_rules(void);
void ae_silent_log_counts(void);

/* ---- joining */

/* the client's join request's machine name (length: MAXIMUM_MACHINE_NAME_LENGTH) */
void ae_silent_join_request_mark(wchar_t *machine_name, short length);
/* the host heard a client machine's join request (before its name is cleaned;
the machine is the server's own record of it, opaque here) */
void ae_silent_join_request_heard(void const *client_machine, wchar_t const *machine_name, short length);
/* the host accepting that client machine into a game of this variant at its
slot: TRUE when refused (the code to tell it set) */
boolean ae_silent_join_refused_now(void const *client_machine, long machine_index, struct game_variant const *variant,
	short *refusal_code);
/* the host's look at a machine each frame: a remote one in the game without
the rules, while the variant has an option on, is dropped */
void ae_silent_host_machine(long machine_index, boolean remote_joined, struct game_variant const *variant);
/* the client was refused (code): this build's own text for AE's code */
void ae_silent_join_rejected(word rejection_code);

/* ---- the gametype editor's ARENA OPTIONS rows */

/* the rows' spinners shown from the variant, or (save) written to it */
void ae_silent_menu_rows(struct widget_instance *list, struct game_variant *variant, boolean save);
/* each frame of the list: SPLIT SCREEN and the two rows kept in step */
void ae_silent_menu_update(struct widget_instance *list);

/* ---- tests */

/* debug.network_test_silent: the automated test host's gametype's options */
void ae_silent_network_test(struct game_variant *variant);

#endif
