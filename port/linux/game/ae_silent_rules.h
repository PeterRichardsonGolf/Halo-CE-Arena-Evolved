/*
AE_SILENT_RULES.H

Arena Evolved's SILENT MOVEMENT and SILENT WEAPONS gametype options, as pure
rules (no engine or config calls: port/linux/tests/ae_silent_rules_test.c runs
them). The engine side is ae_silent.c's.

Two options, the host's choice for everyone, each NORMAL, JUST ME or SILENT:

  SILENT MOVEMENT  a biped's contact-point material effects: footsteps,
                   shuffle, slide, jump and landing
  SILENT WEAPONS   a weapon's READY and PUT-AWAY sounds only, first person and
                   third person (firing, reloading, melee and pickups sound)

  NORMAL   everyone hears them, as Halo CE
  JUST ME  the acting player hears their own; everyone else's are muted for
           them (an acting player not known, or a listener with no player,
           is muted)
  SILENT   nobody hears them, the acting player included

A sound is tagged with its ROLE (the event that made it, not its tag) and its
acting player when it is made; muting is a gain of 0 at the listener, so the
game's simulation never changes. Split screen shares the speakers: there,
JUST ME lets each player hear the others' too, which is why the menu's SPLIT
SCREEN pick sets both to SILENT.

The gametype keeps both in one byte of its universal variant (the high nibble
of the byte the Xbox game left as pad5: bits 4-5 SILENT MOVEMENT, bits 6-7
SILENT WEAPONS; 0 NORMAL, 1 JUST ME, 2 SILENT, 3 read as SILENT). Every
gametype from before has 0 there: NORMAL. The byte travels with the variant
(the saved file's and the network game's sizes are unchanged).

Fairness: a machine without these rules would hear everything. While either
option is not NORMAL, a host refuses a machine that has not told it, in its
join request, that it plays them (AE_SILENT_CAPABILITY): its machine name's
last character, after the name's end, is AE_SILENT_JOIN_MARK plus the
capability. Stock OpenCE and ChupathingyCE send zeros there.
*/

#ifndef __AE_SILENT_RULES_H
#define __AE_SILENT_RULES_H

enum
{
	AE_SILENT_NORMAL = 0,
	AE_SILENT_JUST_ME = 1,
	AE_SILENT_SILENT = 2,
	NUMBER_OF_AE_SILENT_MODES
};

/* what made a sound */
enum
{
	AE_SILENT_ROLE_NONE = 0,
	AE_SILENT_ROLE_MOVEMENT,
	AE_SILENT_ROLE_WEAPON,
	NUMBER_OF_AE_SILENT_ROLES
};

/* the byte's fields */
#define AE_SILENT_MOVEMENT_SHIFT 4
#define AE_SILENT_WEAPONS_SHIFT 6
#define AE_SILENT_FIELD_MASK 3u
#define AE_SILENT_BYTE_MASK 0xF0u

/* the join: the machine name's last character (a 16-bit one), after the name's
end: the mark's top 12 bits, then the capability */
#define AE_SILENT_JOIN_MARK 0xAE50u
#define AE_SILENT_JOIN_MARK_MASK 0xFFF0u
/* 1: SILENT MOVEMENT and SILENT WEAPONS, NORMAL / JUST ME / SILENT */
#define AE_SILENT_CAPABILITY 1
/* 0.7.6: the build's FEATURE MARK, beside the level: the name's character three before the end (two before the mark),
AE_SILENT_JOIN_FEATURE_MARK | bits, the name cut one character earlier still (it ends four before the end at the
latest). Bit 0: the grenade layer (ae_grenade_catalogue.h); bits 1-3: its version (1-7). Hosts of before read only
the mark and the level character, both where they were; the level character's place is 0 when the level is 15 or
less, as before */
#define AE_SILENT_JOIN_FEATURE_MARK 0xAC00u
#define AE_SILENT_JOIN_FEATURE_MARK_MASK 0xFF00u
#define AE_SILENT_JOIN_FEATURE_GRENADES 0x01u
#define AE_SILENT_JOIN_FEATURE_GRENADE_VERSION_SHIFT 1
#define AE_SILENT_JOIN_FEATURE_GRENADE_VERSION_MASK 0x0Eu
/* the refusal's reason, past the game's own rejection codes (0-6): a build
that does not know it says only that it could not join */
#define AE_SILENT_REJECTION_CODE 64
/* 2: and the battle rifle's weapon pack is loaded on the machine (a host whose gametype uses the pack refuses a
machine that tells less, with AE_RIFLE_REJECTION_CODE) */
#define AE_RIFLE_CAPABILITY 2
#define AE_RIFLE_REJECTION_CODE 65
/* 3 and up: and more weapon packs (ae_weapon_slots.h's levels: 3 the SMG's and the silenced SMG's, 4 and the
carbine's). A host whose gametype names a pack past the battle rifle refuses a machine below its level with this
code (a build that does not know it says only that it could not join) */
#define AE_PACKS_REJECTION_CODE 66
/* a TERRITORIES game (ae_variant_ext_rules.h's mode, the King engine's wire with the AETR tail) refuses a machine that is
not Arena Evolved: it would read the King state as a bad one and show frozen scores */
#define AE_TERRITORIES_REJECTION_CODE 67
/* a round mode's game (ELIMINATION: ae_variant_ext_rules.h's mode, the Slayer engine) refuses a machine that is not Arena
Evolved: the round machine's countdown, revives and zone are Arena Evolved's (owner 2026-11-03: AE players only) */
#define AE_ROUNDS_REJECTION_CODE 68
/* a round mode's game that holds the most players it can (AE_ROUND_PLAYER_LIMIT) refuses another */
#define AE_ROUNDS_FULL_REJECTION_CODE 70
/* a game whose gametype names AE grenade types (ae_grenade_catalogue.h's mask) refuses a machine whose feature mark
tells no grenade layer, or one of a lower version than the mask's highest id needs: "update Arena Evolved" */
#define AE_GRENADES_REJECTION_CODE 68

/* ---- the weapon cache (0.7.6; design section 8): the join request's machine name, 32 16-bit characters. Index 31 is
the AE mark (AE_SILENT_JOIN_MARK | the capability), 30 the widen's level character, 29 the feature mark. The cache block
is written LAST, below them: 28 AE_SILENT_JOIN_CACHE_MARK | the cache format version, 27 the cache's generation (its
serial, 16 bits), 26 and 25 the digest (the first 4 bytes of the cache's SHA-256, low half then high half). The name is
cut to 24 characters at the latest. The block is read only with the AE mark at 31 and a zero before index 25. All three
shipped readers decode a name with the block exactly as without it. Indices are counted from the name's end here, so a
name of any length (at least 8) works: index n of 32 is length - 32 + n. A cache build tells capability 1 (SILENT only),
never a pack level: it has no slot packs it could play (design 8.2, 8.6). */
#define AE_SILENT_JOIN_CACHE_MARK 0xAB00u
#define AE_SILENT_JOIN_CACHE_MARK_MASK 0xFF00u
#define AE_SILENT_JOIN_CACHE_MINIMUM_LENGTH 8
/* positions from the last character (index 31 of 32 = 0) */
#define AE_SILENT_JOIN_CACHE_FROM_END_MARK 3
#define AE_SILENT_JOIN_CACHE_FROM_END_GENERATION 4
#define AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_LOW 5
#define AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_HIGH 6
/* the refusal: past the game's own codes, after 64 SILENT, 65 BR, 66 packs, 67 TERRITORIES (68, the grenade layer's, is
the grenades branch's). 66 stays the answer to a machine with no cache block in a game that uses the cache */
#define AE_CACHE_REJECTION_CODE 69

/* the four cases of code 69, and the accept (design 8.3 steps 3-6; step 2, no block, is code 66) */
enum ae_cache_join_case
{
	AE_CACHE_JOIN_ACCEPT = 0,
	AE_CACHE_JOIN_NO_BLOCK, /* code 66, not 69 */
	AE_CACHE_JOIN_FORMAT, /* the block's format version differs */
	AE_CACHE_JOIN_CLIENT_OLDER, /* the client's generation is lower than the host's */
	AE_CACHE_JOIN_HOST_OLDER, /* the client's generation is higher */
	AE_CACHE_JOIN_DIFFERENT_BUILD /* the same generation, a different digest */
};

/* THE CODE-69 WIDTH (integration check STILL PENDING: does the network layer carry the whole 16-bit rejection word from
network_game_server_drop_machine to ae_silent_join_rejected(word), or only a byte? Until that is shown on a real
host/client pair, the case rides the HIGH BYTE of the word, code = 69 | case << 8, and every user goes through these two
functions. If the layer turns out to keep only the low byte, the four cases become four codes and only these two
functions and the test ae_silent_rules_test.c (test_cache_code_width) change. Not resolved here.) The refusal word of a
case: the packs' code 66 for NO_BLOCK, else 69 with the case in the high byte. */
unsigned int ae_cache_rejection_word(int cache_case);
/* the case a word carries, or -1 when it is not a code-69 word (the low byte is not 69) */
int ae_cache_rejection_case(unsigned int word);

/* writes the cache block into a marked name (call after ae_silent_join_mark, the level and the features); a name too
short (under AE_SILENT_JOIN_CACHE_MINIMUM_LENGTH) is left as it is */
void ae_silent_join_cache(unsigned short *name, int length, unsigned int format, unsigned int generation,
	unsigned char const *digest4);
/* reads the block: TRUE and the fields, or FALSE (no AE mark, no block, a nonzero before index 25) */
int ae_silent_join_cache_told(unsigned short const *name, int length, unsigned int *format, unsigned int *generation,
	unsigned char *digest4);
/* the host's decision for a machine (design 8.3): told is whether the machine told a block. The case; the caller maps
NO_BLOCK to code 66 and the others to AE_CACHE_REJECTION_CODE and the T69 text */
int ae_silent_join_cache_decide(int uses_cache, int told, unsigned int client_format, unsigned int client_generation,
	unsigned char const *client_digest4, unsigned int host_format, unsigned int host_generation,
	unsigned char const *host_digest4);

/* the option of a role (AE_SILENT_ROLE_MOVEMENT or _WEAPON) in the byte: its
mode (3 reads as SILENT); NORMAL for any other role */
int ae_silent_mode(unsigned char byte, int role);
/* the byte with the role's option set to the mode (0-2; others as SILENT) */
unsigned char ae_silent_byte_set(unsigned char byte, int role, int mode);
/* whether either option is not NORMAL */
int ae_silent_any(unsigned char byte);

/* whether a sound of the role made by the acting player (a player handle, -1
none) is muted for the listener's player (-1 none) under the byte */
int ae_silent_muted(unsigned char byte, int role, long acting_player, long listener_player);

/* the role of a weapon's animation sound: WEAPON for its READY or PUT-AWAY
state (or message), NONE for any other (firing, reloading, melee...) */
int ae_silent_weapon_role(int ready_or_put_away);
/* the role of a unit's (third-person) action animation's sound: WEAPON for
actions 3 (ready) and 4 (put away), NONE for the others */
int ae_silent_unit_action_role(int action);

/* the join request's machine name (length 16-bit characters): marked as this
build's, its text cut to end two characters before the end */
void ae_silent_join_mark(unsigned short *name, int length);
/* the name (already marked: ae_silent_join_mark) with the feature mark's bits too (0: left as it is) */
void ae_silent_join_features(unsigned short *name, int length, unsigned int bits);
/* the feature bits a join request's machine name tells (0: none, a build without the feature mark) */
unsigned int ae_silent_join_features_told(unsigned short const *name, int length);
/* the feature bits of a grenade layer version (1-7; 0: none), and the version the bits tell (0: none) */
unsigned int ae_silent_join_feature_grenades(int version);
int ae_silent_join_grenade_version(unsigned int bits);
/* the capability a join request's machine name tells (0: none, a build
without these rules) */
int ae_silent_join_capability(unsigned short const *name, int length);
/* whether a host playing the byte refuses a machine of that capability (a
local machine, the host's own, never) */
int ae_silent_join_refused(unsigned char byte, int capability, int local);

/* the strings of a mode and a role, for the logs */
char const *ae_silent_mode_name(int mode);
char const *ae_silent_role_name(int role);

/* the SPLIT SCREEN pick of ARENA OPTIONS: ON when both options are SILENT */
int ae_silent_split_screen(unsigned char byte);

/* ---- the sounds' and effects' tags, by datum handle (the low 16 bits its
slot): what made each, and who */

struct ae_silent_tag
{
	long handle;
	long player;
	unsigned char role;
};

/* a scope of the role and the acting player, and the stack of them (sounds
and effects made inside take the innermost) */
#define AE_SILENT_SCOPE_DEPTH 8

struct ae_silent_scopes
{
	int depth;
	/* pushes past the depth (each still popped) */
	int overflow;
	struct ae_silent_tag scopes[AE_SILENT_SCOPE_DEPTH];
};

void ae_silent_scope_push(struct ae_silent_scopes *scopes, int role, long player);
void ae_silent_scope_pop(struct ae_silent_scopes *scopes);
/* the innermost scope's role and player (NONE / -1 outside any) */
int ae_silent_scope_role(struct ae_silent_scopes const *scopes, long *player);

/* a datum made (handle; -1 nothing): its tag the innermost scope's, or none */
void ae_silent_table_set(struct ae_silent_tag *table, int capacity, long handle, int role, long player);
/* a datum's role and player (NONE / -1 when not tagged, or its slot reused) */
int ae_silent_table_get(struct ae_silent_tag const *table, int capacity, long handle, long *player);

#endif
