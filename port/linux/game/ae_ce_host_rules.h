/*
AE_CE_HOST_RULES.H

Arena Evolved joining a Content Evolved (CE) host that plays a modded game (Firefight, or a campaign with the
randomizer or skulls). The rules that need no game, which the unit tests check
(port/linux/tests/ae_ce_host_rules_test.c); ae_ce_host.c is the game's side.

A CE host of such a game announces 0xFF00 plus its mod version (112) in the version's place and its network version
(44, which is this game's) in the advertisement's last reserved bytes; the game's record is the stock one
(this game's size) unless the game is Firefight, whose record is 0x2C bytes longer: CE's game variant options are
0x58 bytes, this game's 0x2C, and the Firefight options fill the difference. CE's Firefight is engine 6 (the slot
Bomb has here); this game's is 7. Nothing here changes what this game advertises, sends or accepts from any other host:
the reader takes a host that signed itself, or a record of exactly the long size, and no other.

Plain C on bytes, no game dependency.
*/

#ifndef AE_CE_HOST_RULES_H
#define AE_CE_HOST_RULES_H

/* the signature: 0xFF00 | the mod's version, in the advertised version's place */
#define AE_CE_HOST_MARK 0xFF00
/* the mod version this game plays (CE's HALO_PORT_MOD_NETWORK_VERSION of the build its Firefight and skulls came from) */
#define AE_CE_HOST_MOD_VERSION 112
/* where the host's network version is in the advertisement's reserved bytes (a little-endian word) */
#define AE_CE_HOST_BASE_VERSION_OFFSET 24
/* the advertisement's flags of a modded game (the skulls' is halo_port_limits.h's too) */
#define AE_CE_HOST_FLAG_SKULLS 0x08
#define AE_CE_HOST_FLAG_RANDOMIZER 0x20
#define AE_CE_HOST_FLAG_TAG_SHARE 0x40
/* the engine index CE gives Firefight, and the one this game does */
#define AE_CE_HOST_ENGINE_CE_FIREFIGHT 6
#define AE_CE_HOST_ENGINE_AE_FIREFIGHT 7
/* the game variant options: CE's, and the first part this game's own are the size of */
#define AE_CE_HOST_OPTIONS_CE_SIZE 0x58
#define AE_CE_HOST_OPTIONS_AE_SIZE 0x2C
#define AE_CE_HOST_RECORD_EXTRA (AE_CE_HOST_OPTIONS_CE_SIZE - AE_CE_HOST_OPTIONS_AE_SIZE)

/* what a host that advertises this version, base version and flags is, to a game of network version ours */
enum
{
	AE_CE_HOST_NOT_CE = 0,       /* not a signed host: every other rule of this game applies, as before */
	AE_CE_HOST_JOIN,             /* a CE host of this mod version and this network version */
	AE_CE_HOST_MOD_OLDER,        /* a CE host of an older mod version */
	AE_CE_HOST_MOD_NEWER,        /* a CE host of a newer mod version */
	AE_CE_HOST_BASE_OTHER,       /* a CE host of another network version */
	AE_CE_HOST_TAG_SHARE         /* a CE host whose game shares Tag Injector tags, which this game cannot take */
};

/* whether an advertised version is a signed one */
int ae_ce_host_signed(unsigned version);
/* the mod version of a signed one */
unsigned ae_ce_host_mod_of(unsigned version);
int ae_ce_host_classify(unsigned version, unsigned base_version, unsigned flags, unsigned our_version);
/* the lobby listing's format of a modded game (a signed host's), and whether a listing of the format and the mod version its
stamp holds is shown: a game that is not modded always is, a modded one only of this mod version */
#define AE_CE_HOST_LISTING_FORMAT 2
int ae_ce_host_listing_taken(int modded, int mod_version);
/* ... and the engine index it shows: CE's Firefight (6) as this game's (ae_firefight), another as sent */
int ae_ce_host_listing_engine(int modded, int engine, int ae_firefight);
/* whether a host of the class is joined */
int ae_ce_host_class_joinable(int host_class);

/* ---- the join check: what a host of a class is told as, and what is done about it

A listing (tell_player 0) is a classification and nothing else: nothing is said, logged or requested. Only the join
the player makes (tell_player 1) logs, tells the player and asks for the data packs it lacks. */

/* what the join says */
enum
{
	AE_CE_HOST_SAY_JOINABLE = 0,
	AE_CE_HOST_SAY_PACKS_MISSING,
	AE_CE_HOST_SAY_MOD_VERSION_OLDER,
	AE_CE_HOST_SAY_MOD_VERSION_NEWER,
	AE_CE_HOST_SAY_TAG_SHARE,
	AE_CE_HOST_SAY_NETWORK_VERSION
};

/* the data packs a join asks for */
#define AE_CE_HOST_PACK_RANDOMIZER 0x01
#define AE_CE_HOST_PACK_SKULLS 0x02

struct ae_ce_host_join_verdict
{
	int joinable;       /* the host is joined (a host that is not CE's always is) */
	int say;            /* AE_CE_HOST_SAY_*: why not (or JOINABLE); set whether or not the player is told */
	int log;            /* a line goes to the log */
	int tell_player;    /* the player is shown the reason */
	int request_packs;  /* AE_CE_HOST_PACK_* the join asks to have written */
};

/* randomizer_pack_missing / skulls_pack_missing: the game uses the pack and it is not ready (plain reads) */
void ae_ce_host_join_decide(int host_class, int randomizer_pack_missing, int skulls_pack_missing, int tell_player,
	struct ae_ce_host_join_verdict *verdict);

/* ---- the joined host's state: what is kept of a CE host, and when it goes */

enum
{
	AE_CE_HOST_STATE_NOT_CE = 0, /* no CE host (or one whose game is not CE's) */
	AE_CE_HOST_STATE_CE,         /* a CE host of this mod version is joined (its record's options kept once accepted) */
	AE_CE_HOST_STATE_UNKNOWN,    /* joined by an address: CE's long record says what it is */
	AE_CE_HOST_STATE_REFUSED     /* the host's record was refused (told once): its later pieces are dropped, until the next join starts */
};

enum
{
	AE_CE_HOST_EVENT_JOIN_STARTED_CE = 0,
	AE_CE_HOST_EVENT_JOIN_STARTED_OTHER,
	AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN,
	AE_CE_HOST_EVENT_RECORD_ACCEPTED, /* the settings record was converted and this game accepted it */
	AE_CE_HOST_EVENT_RECORD_REFUSED,  /* the record could not be read (the game is left) */
	AE_CE_HOST_EVENT_REJECTED,        /* the host or this game's own checks turned the join down */
	AE_CE_HOST_EVENT_DISCONNECTED,
	AE_CE_HOST_EVENT_LEFT,
	AE_CE_HOST_EVENT_HOSTING,         /* this machine starts a game of its own */
	AE_CE_HOST_EVENT_SOLO_START,
	AE_CE_HOST_EVENTS
};

/* the state after the event; *clear_options is set when the Firefight options kept from the host must go (the state held
a CE host's). RECORD_ACCEPTED leaves the options to be applied by the caller when the new state is CE. */
int ae_ce_host_state_next(int state, int event, int *clear_options);

/* ---- the level's tags against the host's

A modded co-op host sends a client its level's shared tag count and fingerprint once the client is ready (skulls.c's
mod_tags message); the host's objects name tags by index, so a client whose shared tags differ would play the host's
enemies, weapons and skulls as other things. Such a client leaves the game with AE_CE_HOST_TAGS_MESSAGE; a replay played
back leaves nothing. */
#define AE_CE_HOST_TAGS_MESSAGE "This game's level did not load the same tags as the host's, so its enemies, weapons and " \
	"skulls would not match. The game was left.\n\nJoin again once both games use the same randomizer and skulls data."
/* whether the client leaves: the tags differ (count or fingerprint) and no replay is being played back */
int ae_ce_host_tags_leave(long count, unsigned long hash, long host_count, unsigned long host_hash, int replay_playing);

/* ---- CE's game variant options, as CE's struct game_variant_options (source/game/game_engine.h) has them */

/* the number of Firefight options read out of the extra bytes and the base part, in the order of Arena Evolved's
firefight_options wire table (ae_firefight_glue.c: net_fields) */
#define AE_CE_HOST_VALUES 33
/* the bytes of each, in that order (1, 2 or 4) */
extern unsigned char const ae_ce_host_value_widths[AE_CE_HOST_VALUES];
/* the value of Firefight's options a host shares a Tag Injector profile by (its folder's hash; 0: none) */
#define AE_CE_HOST_VALUE_TAG_PROFILE 11

/* where the record's parts are in this build's record (the caller's offsetofs), all in bytes */
struct ae_ce_host_layout
{
	unsigned long ae_size;             /* this build's record (HALO_PORT_NETWORK_GAME_SIZE) */
	unsigned long variant_options;     /* the game variant options' offset in it */
	unsigned long engine_index;        /* the game variant's engine index's offset */
	unsigned long engine_index_size;   /* ... and its size (1 to 8; little-endian on the wire) */
	unsigned long ae_engine_firefight; /* this game's Firefight engine index */
	unsigned long ae_jump_percent;     /* the offset in the options of this game's first own byte after CE's
	                                   force-team-spawns (CE's floodfection bytes sit there) */
};

enum
{
	AE_CE_HOST_RECORD_OK = 0,
	AE_CE_HOST_RECORD_BAD_SIZE,
	AE_CE_HOST_RECORD_NOT_FIREFIGHT, /* a long record of another engine than CE's Firefight */
	AE_CE_HOST_RECORD_TAG_PROFILE    /* the game plays a Tag Injector profile the host shares */
};

/* the size of the record a CE host sends for a Firefight game (the stock one and the extra bytes) */
unsigned long ae_ce_host_record_size(struct ae_ce_host_layout const *layout);
/* a CE Firefight record of ce_size bytes made this build's, in place of nothing: ae gets ae_size bytes (CE's first
0x2C option bytes with this build's own last five zero, the engine made this game's Firefight, the rest of the record
moved up over the Firefight bytes), and values the Firefight options (AE_CE_HOST_VALUES numbers) */
int ae_ce_host_record_convert(unsigned char const *ce, unsigned long ce_size, unsigned char *ae,
	struct ae_ce_host_layout const *layout, unsigned long *values);

#endif
