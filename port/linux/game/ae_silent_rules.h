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
