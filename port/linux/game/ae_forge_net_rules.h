/*
AE_FORGE_NET_RULES.H

The rules of Forge's network (port/forge/src/forge_network.c): what a Forge line may say, who may say it and how much. Pure: no game state,
the time is given. A Forge session speaks Content Evolved's wire as it is (one 64-byte chat line, `!F` and a tag, `:`-separated fields), so
every line is parsed here before any of it is acted on, into one checked record (struct ae_forge_net_record): the chat parser fills it, and
a binary reader of a later hardened mode would fill the same one. Nothing in it comes from the sender but numbers within their bounds.

A line is refused when its 64 bytes hold no NUL, a field is missing, extra, empty or not a whole number or finite number of the shape its
tag has, or a number is past its bound. A client's line names an object (obj_abs) only as the one of the Forge instance it names
(ae_forge_net_rules_object_ok), never a player's unit or an item in a hand. Budgets and token buckets bound what a client may do, and what a
host may ask of a client.
*/

#ifndef __AE_FORGE_NET_RULES_H
#define __AE_FORGE_NET_RULES_H

/* ---------- the numbers */

enum
{
	/* the chat line's text, as the wire has it */
	AE_FORGE_NET_TEXT_SIZE = 64,
	/* a Forge session's version (forge_engine.h's FORGE_NETWORK_VERSION) */
	AE_FORGE_NET_VERSION = 0x4777,
	AE_FORGE_NET_MAX_PLAYERS = 128,
	/* a client's budgets, and the whole map's: Forge instances a session may add over the map's own, and AI */
	AE_FORGE_NET_CLIENT_INSTANCES = 128,
	AE_FORGE_NET_SESSION_INSTANCES = 512,
	AE_FORGE_NET_CLIENT_AI_SPAWNS = 16,
	AE_FORGE_NET_CLIENT_AI_UNITS = 64,
	/* the most units one AI spawn starts, and the most a respawn delay, in seconds */
	AE_FORGE_NET_AI_UNIT_COUNT = 16,
	AE_FORGE_NET_AI_RESPAWN_SECONDS = 600,
	/* a client over its rate this long (milliseconds) is let go of */
	AE_FORGE_NET_STRIKE_MS = 5000,
	/* an AI spawn's team: forge_ai.c's own (the unit's, the player's, a human, covenant, flood, sentinel, ALL PLAYERS, red, blue) */
	AE_FORGE_NET_AI_TEAM_MAX = 8
};

struct ae_forge_net_limits
{
	/* the most any coordinate is from the origin, in world units */
	float bound;
	/* the live instances there can be (FORGE_MAX_LIVE_INSTANCES), and the objects the netcode tracks (FORGE_MAX_TRACKED_NET_OBJECTS) */
	int live_max;
	int object_max;
	/* the spawn effects there are (forge_ai.c's table) */
	int effect_count;
};

/* the limits as this build has them */
void ae_forge_net_rules_limits(struct ae_forge_net_limits *limits, int live_max, int object_max);

/* ---------- the line */

/* the text of a chat line's 64 bytes: copied into out (AE_FORGE_NET_TEXT_SIZE + 1 bytes) with its NUL; 0, out holding the empty text, when the 64
bytes hold no NUL (the line is not one: dropped) */
int ae_forge_net_rules_text(char const *line, char *out);

enum ae_forge_net_tag
{
	AE_FORGE_NET_NONE = -1,
	AE_FORGE_NET_FT, AE_FORGE_NET_FE, AE_FORGE_NET_FQ, AE_FORGE_NET_FS, AE_FORGE_NET_FH, AE_FORGE_NET_FM, AE_FORGE_NET_FD,
	AE_FORGE_NET_FU, AE_FORGE_NET_FA, AE_FORGE_NET_FJ, AE_FORGE_NET_FV, AE_FORGE_NET_FW, AE_FORGE_NET_FN, AE_FORGE_NET_FK,
	AE_FORGE_NET_FZ,
	/* the lines of Arena Evolved alone: the base tags of the host (count and fingerprint), and a refusal with its reason */
	AE_FORGE_NET_FB, AE_FORGE_NET_FX,
	/* the map's general settings, the host's word alone: the display name, a tag by index, a link, a movement, the view, the atmosphere */
	AE_FORGE_NET_FGN, AE_FORGE_NET_FGT, AE_FORGE_NET_FGL, AE_FORGE_NET_FGM, AE_FORGE_NET_FGV, AE_FORGE_NET_FGA,
	AE_FORGE_NET_TAGS
};

/* why a line was refused */
enum
{
	AE_FORGE_NET_OK = 0,
	AE_FORGE_NET_NOT_FORGE,
	AE_FORGE_NET_UNKNOWN_TAG,
	AE_FORGE_NET_FIELDS,
	AE_FORGE_NET_NUMBER,
	AE_FORGE_NET_RANGE,
	AE_FORGE_NET_FINITE
};

/* a checked line. Only the fields of its tag are set; the others are 0 */
struct ae_forge_net_record
{
	int tag;
	int reason;
	/* the instance (live index), the object (object index of the netcode: -1 none), a player */
	int live_idx;
	int obj_abs;
	int player;
	/* where, hundredths of a turn as the wire has them (yaw, pitch, roll), and the flags: held, on (a switch), monitor */
	float x, y, z;
	int yaw, pitch, roll;
	int held;
	int on;
	/* a tag's hash, the team (or usage) of an instance, a marker's usage, a scale in thousandths, an effect */
	unsigned int hash;
	int team;
	int usage;
	int thousandths;
	int effect;
	/* !FH: the object and weapon code (object abs + 1 in 12 bits, the weapon's in 12 more, held in bit 24) */
	unsigned int object_code;
	/* !FW: shape, effect, damage, the three sizes (hundredths), flags */
	int shape, damage, size[3], flags;
	/* !FA: the settings (hex fields in the order the line has them); !FJ: goal, link, who, start, follow, alert (-1: none given) */
	unsigned int weapon_hash;
	int unit_count, respawn_alive, respawn_delay, respawn_total, gametypes, detached, spawn_effect;
	int goal, link, who, start, follow, alert;
	/* !FB: the host's base tag count and fingerprint of its tag table; !FX: a reason */
	unsigned int base_count, fingerprint;
	int fields;
	/* !FGN: the display name; !FGT: a kind (sky, biped, hands), a slot (red, blue, free for all) and a tag index (in `object_code`); !FGL/!FGM: a slot and a
	link or a movement (in `team` and `usage`); !FGV: the render distance and the monitor speed (fog_start, fog_opaque); !FGA: atmosphere, color, density,
	start, opaque, indoors, sky */
	char name[48];
	int kind, slot, atmosphere, color, density, indoors, sky;
	float fog_start, fog_opaque;
};

/* the line's text checked and read into a record: the tag, or AE_FORGE_NET_NONE with the reason in out->reason */
int ae_forge_net_rules_parse(char const *text, struct ae_forge_net_limits const *limits, struct ae_forge_net_record *out);

/* a tag's name ("!FM" for AE_FORGE_NET_FM) */
char const *ae_forge_net_rules_tag_name(int tag);

/* ---------- who may say what */

/* whether a tag is one a client may say to the host (edits, its Monitor, the request for the state), one the host says to a client (what it
has placed and its settings), or both */
int ae_forge_net_rules_client_may_say(int tag);
int ae_forge_net_rules_host_may_say(int tag);

/* whether a line is an edit (counted by the edit bucket and the budgets) */
int ae_forge_net_rules_is_edit(int tag);

/* the object a client's line names: -1 (none) or the one the instance it names has (instance_obj_abs, -1 if it has none that the netcode
shares). A client never names another: a player's unit, an item in a hand, the map's. The host's own lines name what they like */
int ae_forge_net_rules_object_ok(int from_client, int obj_abs, int instance_obj_abs);

/* whether a client may edit at all: the reference lets any joined client (host_only 0); a host setting lets the host alone */
int ae_forge_net_rules_may_edit(int from_client, int host_only);

/* ---------- rates and budgets */

struct ae_forge_net_bucket
{
	double tokens;
	unsigned long last_ms;
	int started;
};

/* a bucket of `rate` a second holding at most `burst`: takes one if there is one (1), else 0. Time is given */
int ae_forge_net_rules_bucket_take(struct ae_forge_net_bucket *bucket, unsigned long now_ms, double rate, double burst);

/* the rates of each kind of line: edits (!FS !FM !FD ...), a Monitor's pose (!FT), the request for the state (!FQ) */
enum { AE_FORGE_NET_BUCKET_EDIT, AE_FORGE_NET_BUCKET_POSE, AE_FORGE_NET_BUCKET_SYNC, AE_FORGE_NET_BUCKETS };
int ae_forge_net_rules_line_bucket(int tag);
void ae_forge_net_rules_bucket_rate(int bucket, double *rate, double *burst);

/* what one client has added: Forge instances, AI spawns and the units they start, and how long it has been over its rate */
struct ae_forge_net_client
{
	struct ae_forge_net_bucket buckets[AE_FORGE_NET_BUCKETS];
	int instances;
	int ai_spawns;
	int ai_units;
	/* the time it was first over each rate in this run of lines (0: not over): a bucket kept up does not clear another's; it is let go of for good
	(dropped) and told (told) */
	unsigned long over_since_ms[AE_FORGE_NET_BUCKETS];
	int dropped;
	int told;
};

void ae_forge_net_rules_client_reset(struct ae_forge_net_client *client);
/* its machine left: the request for the state starts afresh (a player who comes back is not struck for asking again, and is fed once more) */
void ae_forge_net_rules_client_machine_left(struct ae_forge_net_client *client);
/* a bucket brought up to date (refilled), and what it holds */
void ae_forge_net_rules_bucket_refill(struct ae_forge_net_bucket *bucket, unsigned long now_ms, double rate, double burst);

/* one line of a client counted: 1 when it may be acted on, 0 when it is over its rate (the line is dropped); a client over its rate for
AE_FORGE_NET_STRIKE_MS: *drop set to 1 (let it go of the session). A bucket clears its own strike when it next has a token to spare (half its burst, or its one) */
int ae_forge_net_rules_client_line(struct ae_forge_net_client *client, int tag, unsigned long now_ms, int *drop);

/* an instance added by a client (session_instances: what the whole session has added so far): 1, counted, or 0 over a budget */
int ae_forge_net_rules_client_instance(struct ae_forge_net_client *client, int session_instances);
/* an AI spawn of `units` units added: 1 or 0 */
int ae_forge_net_rules_client_ai(struct ae_forge_net_client *client, int units);
/* an instance of a client taken away: its count falls */
void ae_forge_net_rules_client_removed(struct ae_forge_net_client *client, int was_ai, int units);

/* a host's creates as a joiner takes them: at most this many each second (the host's own feed is 6 a tick of 30) */
int ae_forge_net_rules_joiner_create(struct ae_forge_net_bucket *bucket, unsigned long now_ms);

/* ---------- the version of a Forge session */

/* the network version this machine announces, and the range of versions it joins: the numbers given (the build's own) in a game that is not a
Forge session, exactly Forge's in one. (In the header: delta.c, which every build has, answers with them without linking this unit) */
static inline int ae_forge_net_rules_announce(int session, int normal)
{
	return session ? AE_FORGE_NET_VERSION : normal;
}

static inline int ae_forge_net_rules_minimum(int session, int normal)
{
	return session ? AE_FORGE_NET_VERSION : normal;
}

static inline int ae_forge_net_rules_maximum(int session, int normal)
{
	return session ? AE_FORGE_NET_VERSION : normal;
}

/* whether the shortcut a host's Delta wire gives a client (it joins whatever version is announced) applies: never in a Forge session, whose
version is its own */
static inline int ae_forge_net_rules_delta_shortcut(int session)
{
	return !session;
}

/* ---------- who may touch what (the facts the caller gathers, the decision the rules take) */

/* the kinds of instance the rules tell apart */
enum { AE_FORGE_NET_KIND_OTHER, AE_FORGE_NET_KIND_SPAWN, AE_FORGE_NET_KIND_FLAG, AE_FORGE_NET_KIND_VEHICLE, AE_FORGE_NET_KIND_ITEM };

struct ae_forge_net_facts
{
	/* the instance is there (live and active) */
	int active;
	int kind;
	/* it is one of the map's own (a default scenario object), not one placed in Forge */
	int map_own;
	/* its object has a parent (held, carried, attached) */
	int parented;
	/* a vehicle with a rider in a seat */
	int occupied;
	/* the object the netcode shares for it (-1: none) */
	int object_abs;
	/* who set it held (identity + 1, 0 nobody), and who placed it (identity + 1, 0 the host or the map) */
	int held_by;
	int owner;
	/* the class a game needs one of: a player spawn's team, a flag's type and team. The modes this instance serves (bits 0x1 CTF .. 0x10 race; 0 all of
	them) and the modes the others of its class together serve (0 none, so no other); a flag serves one thing (1) */
	int class_own_mask;
	int class_others_mask;
	/* the host holds it in its own hand (the grabbed instance), and where a move would put it is inside the level (the caller asks the level's leaves) */
	int host_holds;
	/* how many instances the sender holds now (a hold of a new one past AE_FORGE_NET_HOLD_MAX is refused) */
	int sender_holds;
	int dest_in_level;
	/* when its slot was last made (the ms clock), and by whom (identity + 1, 0 the host) */
	unsigned long created_ms;
	int creator;
};

enum
{
	AE_FORGE_NET_TOUCH_OK = 0,
	AE_FORGE_NET_TOUCH_INACTIVE,
	AE_FORGE_NET_TOUCH_OBJECT,
	AE_FORGE_NET_TOUCH_MAP_OWN,
	AE_FORGE_NET_TOUCH_PARENTED,
	AE_FORGE_NET_TOUCH_OCCUPIED,
	AE_FORGE_NET_TOUCH_HELD,
	AE_FORGE_NET_TOUCH_LAST,
	AE_FORGE_NET_TOUCH_BOUNDS,
	AE_FORGE_NET_TOUCH_FRESH
};

/* the time after a slot is made that only the one who made it may edit it: a line sent about the object that stood there before cannot hit the new one */
enum { AE_FORGE_NET_FRESH_MS = 1500 };

/* whether a client (sender: its identity + 1) may do what a line says to an instance: refused (the reason) when the instance is not there, the
line names another object than its own, it is the map's own and clients may not edit those (clients_edit_map 0), its object is held or carried by
a player or has a parent, it is a vehicle with a rider, it is held by another client (a line that holds it: `held` 1), it is the last of its class
and the line deletes it, or its slot was made a moment ago by another. The same for every tag a client may say that names an instance */
int ae_forge_net_rules_client_touch(struct ae_forge_net_facts const *facts, int tag, int obj_abs, int held, int sender, int clients_edit_map,
	unsigned long now_ms);

/* the modes of a spawn: all of them (a mask of 0, or all the bits) are the five a spawn can serve */
enum { AE_FORGE_NET_MODES_ALL = 0x1F, AE_FORGE_NET_HOLD_MAX = 8 };
/* whether an instance is the last of its class for some mode it serves: no other instance of the class serves it (own and others are masks, 0 own meaning all) */
int ae_forge_net_rules_last_of_class(int own_mask, int others_mask);
/* whether a client may delete an instance: not the last of its class (the only spawn of a team for a mode, the only flag of its type and team). The host's
own deletion is the host's: it asks nothing */
int ae_forge_net_rules_delete_ok(int kind, int last_of_class);
/* the kinds whose last of a class is kept, and whose place must be in the level */
int ae_forge_net_rules_protected_kind(int kind);

/* an AI spawn's team: the ones forge_ai.c's TEAM row has; and a spawn's team as an instance of a kind takes it (an AI spawn: that set; any other a
16-bit designator, a marker's usage or a teleporter's channel) */
int ae_forge_net_rules_ai_team_ok(int team);
int ae_forge_net_rules_team_ok(int is_ai_spawn, int team);

/* an instance's team (or usage) as its kind takes it from a peer: a flag by its type (oddball 0..15, race and hill 0..31, a capture flag 0 or 1, a generator 0),
a teleporter's channel (0..15), a spawn's team (0..2), an AI spawn's team; any other (a marker's usage, a route's) a 16-bit number the users mask */
enum { AE_FORGE_NET_TEAM_OTHER, AE_FORGE_NET_TEAM_SPAWN, AE_FORGE_NET_TEAM_FLAG, AE_FORGE_NET_TEAM_TELEPORTER, AE_FORGE_NET_TEAM_AI };
int ae_forge_net_rules_instance_team_ok(int kind, int flag_type, int team);

/* the actors an AI spawn may have alive at once: its start count, or the units kept alive by respawning, the more */
int ae_forge_net_rules_ai_live(int unit_count, int respawn_alive);
/* a spawn's new live actors against its sender's budget (owned_others: what the sender's other spawns have alive) and the session's (session_others) */
enum { AE_FORGE_NET_SESSION_AI_LIVE = 128 };
int ae_forge_net_rules_ai_budget_ok(int owned_others, int new_live, int session_others);

/* ---------- a machine's standing, the identities and the relay */

/* a machine in a Forge session: new until it asks for the state; only then do its lines count, and the level must be ready */
enum { AE_FORGE_NET_MACHINE_NEW, AE_FORGE_NET_MACHINE_SYNCED };
struct ae_forge_net_machine
{
	int state;
	/* the identity of its player (the game's: the machine's address), -1 until it speaks */
	int identity;
};
void ae_forge_net_rules_machine_reset(struct ae_forge_net_machine *machine);
/* whether a line of this tag from the machine is looked at: the level is ready (level_ready), and the machine is synced or this is its first request
for the state. Nothing is changed by the question: the machine is synced (ae_forge_net_rules_machine_synced) once that request is taken */
int ae_forge_net_rules_machine_accepts(struct ae_forge_net_machine const *machine, int tag, int level_ready);
void ae_forge_net_rules_machine_synced(struct ae_forge_net_machine *machine);

/* the identities of a session: a key (a machine's address) to a slot, kept for the session so a machine that leaves and comes back is the same;
-1 when the table (max slots) is full of others */
int ae_forge_net_rules_identity(unsigned long *keys, int *count, int max, unsigned long key);
/* the key of a machine: its address, or (a tunnel's or unresolved peer: address 0) one of its own by its machine index, so that they share no budget */
unsigned long ae_forge_net_rules_identity_key(unsigned long address, long machine_index);
/* the slot to give up when the table is full: the one seen longest ago (last_seen_ms) that is not pinned (dropped, or with instances); -1 if all are pinned */
int ae_forge_net_rules_identity_evict(unsigned long const *last_seen_ms, unsigned char const *pinned, int count, unsigned long now_ms);
/* what a machine with no identity (the table full of pinned ones) may still do: be seen, ask for the state, and speak */
int ae_forge_net_rules_identityless_may(int tag);

/* the relay: what the host sends on to the other machines, a bucket for the session (a cost of one for each line to each machine) and one for each
sender that shares it, so that one cannot take it all: 1 when the line may be relayed */
struct ae_forge_net_relay
{
	struct ae_forge_net_bucket session;
	struct ae_forge_net_bucket senders[32];
};
int ae_forge_net_rules_relay_ok(struct ae_forge_net_relay *relay, int sender, int recipients, int active_senders, unsigned long now_ms);

/* the machines that sent a line lately (last_line_ms of each, within a second of now): the senders the relay is shared among */
int ae_forge_net_rules_active_senders(unsigned long const *last_line_ms, int count, unsigned long now_ms);

/* the host's feed to a machine that just joined: so many lines a second whatever the frame rate (a bucket of 150 a second, a burst of 24); the number
of lines it may send now, and what it sent */
int ae_forge_net_rules_feed_allowance(struct ae_forge_net_bucket *bucket, unsigned long now_ms);
void ae_forge_net_rules_feed_spent(struct ae_forge_net_bucket *bucket, int lines);

/* the streams (a Monitor's pose, a held instance's moves): the latest line of each key is kept and sent when the relay's budget allows, the older ones
dropped (the table full: a new key's line is dropped, unless it lets go); a line that lets go (final) is kept until it is sent, and no later line of its
key overwrites it unless it is final too */
enum { AE_FORGE_NET_PENDING_MAX = 192 };
struct ae_forge_net_pending
{
	int valid;
	long key;
	int sender;
	int final;
	char text[AE_FORGE_NET_TEXT_SIZE + 1];
};
struct ae_forge_net_pending_table
{
	struct ae_forge_net_pending slot[AE_FORGE_NET_PENDING_MAX];
};
/* 1 kept; 0 dropped (the table full of other keys, and the line not final) */
int ae_forge_net_rules_pending_put(struct ae_forge_net_pending_table *table, long key, int sender, char const *text, int final);
/* what is waiting for a key is no more (its instance was deleted, or its line went out in order) */
void ae_forge_net_rules_pending_clear(struct ae_forge_net_pending_table *table, long key);
/* sends one waiting line when the relay's budget allows (into text, its sender in *sender): 1, else 0 and they stay. Final lines go first */
int ae_forge_net_rules_pending_flush(struct ae_forge_net_pending_table *table, struct ae_forge_net_relay *relay, int recipients, int active_senders,
	unsigned long now_ms, char *text, int *sender);

/* the instances (indices) a player holds, up to max of them: their number (what a leaver lets go of, each one) */
int ae_forge_net_rules_held_list(unsigned char const *held_by, int n, int identity_plus_one, int *out, int max);
/* how many instances a player (identity + 1) holds, of n marks */
int ae_forge_net_rules_count_held(unsigned char const *held_by, int n, int identity_plus_one);

/* a line formatted for relaying in 63 characters by construction: the numbers at the most digits that fit */
int ae_forge_net_rules_format_fm(char *text, int size, int live_idx, int obj_abs, float x, float y, float z, int yaw, int pitch, int roll, int held);
int ae_forge_net_rules_format_ft(char *text, int size, int player, int on, float x, float y, float z, int yaw, int pitch);

/* a peer's text as a log line may hold it: printable bytes only, at most size - 1 */
void ae_forge_net_rules_printable(char const *text, char *out, int size);

/* ---------- a joiner waits for the host's tags */

enum { AE_FORGE_NET_COMPARE_WAIT, AE_FORGE_NET_COMPARE_PASSED, AE_FORGE_NET_COMPARE_FAILED, AE_FORGE_NET_COMPARE_NOT_COMPARED };
/* the host says nothing of its tags this long after the request (ms): not compared, and the joiner goes on */
enum { AE_FORGE_NET_COMPARE_TIMEOUT_MS = 3000 };
/* the state once the request is sent at asked_ms (0: not yet) and the host has (or has not) said its tags: passed, failed, still waiting or timed out */
int ae_forge_net_rules_compare_state(int state, int host_said, int same, unsigned long asked_ms, unsigned long now_ms);
/* whether the host's lines that make things (creates, moves) are applied: after the compare passed or timed out */
int ae_forge_net_rules_apply_allowed(int state);

/* the lines a joiner has put by (its bucket of creates is empty, or the compare is not done): kept in order, none lost until the ring is full (then
the joiner asks for the state again) */
enum { AE_FORGE_NET_RING_LINES = 2048 };
struct ae_forge_net_ring
{
	char lines[AE_FORGE_NET_RING_LINES][AE_FORGE_NET_TEXT_SIZE + 1];
	int head;
	int count;
};
void ae_forge_net_rules_ring_reset(struct ae_forge_net_ring *ring);
int ae_forge_net_rules_ring_push(struct ae_forge_net_ring *ring, char const *text);
int ae_forge_net_rules_ring_pop(struct ae_forge_net_ring *ring, char *text);

/* ---------- the base tags */

enum { AE_FORGE_NET_TAGS_SAME, AE_FORGE_NET_TAGS_COUNT, AE_FORGE_NET_TAGS_FINGERPRINT, AE_FORGE_NET_TAGS_NOT_COMPARED };
/* the host's base tag count and fingerprint against this machine's (have_host 0: the host said none: not compared) */
int ae_forge_net_rules_tags_compare(int have_host, unsigned int host_count, unsigned int host_fingerprint, unsigned int count,
	unsigned int fingerprint);
/* what a refusal says (text of size bytes), for the player and the host's log */
void ae_forge_net_rules_tags_message(int result, unsigned int host_count, unsigned int count, char *text, int size);

/* ---------- object changes (the netcode's list, as a joiner takes it) */

/* the most changes a list holds (a wire short) */
enum { AE_FORGE_NET_CHANGES_MAX = 32768 };
/* a host's list of `count` changes as a wire short has it: how many are looked at, all of them (0 for a count of 0 or less) */
int ae_forge_net_rules_changes_count(int count);

#endif /* __AE_FORGE_NET_RULES_H */
