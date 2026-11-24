/*
AE_SPAWNER_MSG_RULES.H

The spawners' state to guests (ae_spawners.h): distributed message 128's body, pure. The host writes one
entry per spawner, keyed by the map item (the first of a merged pair), so a guest whose sidecar differs still reads the
right spot; the item timers on a guest show READY / a countdown / RED from it instead of the clock.

The body: AE_SPAWNER_MSG_HEAD bytes (version 1, the set id (AE_WEAPONSET_* or AE_SPAWNER_MSG_SET_OFF), count, little
endian), then count entries of AE_SPAWNER_MSG_ENTRY bytes: the map item (u16) and one u16 = state (2 bits) | seconds
left << 2 (14 bits, 0 = none / unknown). A reader refuses another version, a count over the maximum, a size that is not
whole, a repeated map item.
*/

#ifndef AE_SPAWNER_MSG_RULES_H
#define AE_SPAWNER_MSG_RULES_H

enum
{
	AE_SPAWNER_MSG_VERSION = 1,
	AE_SPAWNER_MSG_MAXIMUM = 256,
	AE_SPAWNER_MSG_HEAD = 4,
	AE_SPAWNER_MSG_ENTRY = 4,
	AE_SPAWNER_MSG_SIZE = AE_SPAWNER_MSG_HEAD + AE_SPAWNER_MSG_MAXIMUM * AE_SPAWNER_MSG_ENTRY,
	AE_SPAWNER_MSG_SECONDS_MAXIMUM = 16383,
	AE_SPAWNER_MSG_SET_OFF = 255,
};

/* the wire states (ae_spawner_logic_rules.h's enum ae_spawner_state, with the static logic's "on spot" folded in) */
enum
{
	AE_SPAWNER_MSG_OFF = 0,	/* dormant, or not in this game type */
	AE_SPAWNER_MSG_WAITING,	/* nothing on the spot; seconds counts down */
	AE_SPAWNER_MSG_READY,	/* the weapon sits on its spot */
	AE_SPAWNER_MSG_RED	/* taken, the next waits for this one to leave play */
};

struct ae_spawner_msg_entry
{
	unsigned short item;	/* the map item (scenario netgame equipment index) */
	unsigned char state;	/* AE_SPAWNER_MSG_* */
	unsigned short seconds;	/* left until the next spawn (a static spawner counts on while READY; RED = its period), 0 none */
};

struct ae_spawner_msg
{
	unsigned char set;	/* AE_WEAPONSET_* or AE_SPAWNER_MSG_SET_OFF */
	int count;
	struct ae_spawner_msg_entry entries[AE_SPAWNER_MSG_MAXIMUM];
};

/* the wire state of a spawner from its machine's view: the state (enum ae_spawner_state), whether its weapon counts as
on its spot (the static logic), and the ticks to its next spawn (ae_spawner_logic_ticks_left); *seconds gets the ceiling
in seconds, clamped, 0 for OFF */
int ae_spawner_msg_state(int state, int on_spot, long ticks_left, unsigned short *seconds);

/* bytes written (AE_SPAWNER_MSG_HEAD + count * AE_SPAWNER_MSG_ENTRY), 0 when the buffer is too small or the count is over
the maximum; seconds over the maximum are clamped, a state outside 0-3 reads as OFF */
int ae_spawner_msg_encode(struct ae_spawner_msg const *msg, unsigned char *bytes, int size);

/* 1 when the bytes are a whole body of this version; 0 refused (the message is left as it was) */
int ae_spawner_msg_decode(unsigned char const *bytes, int size, struct ae_spawner_msg *msg);

/* the entry of a map item, -1 none */
int ae_spawner_msg_find(struct ae_spawner_msg const *msg, int item);

/* whether two messages differ in anything but their seconds: a host resends at once on such a change, else on its
slow beat (the guest counts the seconds down itself) */
int ae_spawner_msg_states_differ(struct ae_spawner_msg const *a, struct ae_spawner_msg const *b);

#endif
