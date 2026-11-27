#ifndef AE_SERVER_INGAME_RULES_H
#define AE_SERVER_INGAME_RULES_H

/*
AE_SERVER_INGAME_RULES.H

The pure rules of running a dedicated server from inside the game (0.7.7, plan Task 12; the approved mockups E-2 to
E-5, F-1, F-2, F-4): which actions a role may take, the server command each runs, the ban lengths, the words the
player card and the SERVER page show, and the server's admin message on the wire (distributed message 113: from the
server to a signed-in player with a role, while a game is played). No game, socket or file dependency:
tools/test_ae_units.py compiles ae_server_ingame_rules_test.c on its own.

Who has a role: the moderator key a player signs in with (the game makes it from the player key, delta_moderation.h),
listed as a "key:<64 hex>" row of admins.txt (moderator or admin), or the server owner's key the menus' HOST A SERVER
gives the server it starts (HALO_SERVER_OWNER_KEY); or the address a player joins from, held by a "cidr:" row.
*/

enum
{
	/* the actions, in the order the card and the page ask for them */
	AE_INGAME_MOVE, AE_INGAME_MUTE, AE_INGAME_KICK, AE_INGAME_BAN, AE_INGAME_NEXT, AE_INGAME_END, AE_INGAME_SAY,
	AE_INGAME_RESTART,
	AE_INGAME_ACTIONS,

	/* the roles, as the server's numbers */
	AE_INGAME_ROLE_NONE = 0, AE_INGAME_ROLE_MODERATOR = 1, AE_INGAME_ROLE_ADMIN = 2, AE_INGAME_ROLE_OWNER = 3,

	/* Delta Peer's moderation actions this server takes (delta_wire.h's enum: kick 2, ban 3, end game 4, next map 5,
	mute 6, unmute 7; AE's from 8 on, refused as unknown by a server that has none) */
	AE_INGAME_DELTA_KICK = 2, AE_INGAME_DELTA_BAN = 3, AE_INGAME_DELTA_END = 4, AE_INGAME_DELTA_MUTE = 6,
	AE_INGAME_DELTA_UNMUTE = 7, AE_INGAME_DELTA_MOVE = 8, AE_INGAME_DELTA_NEXT = 9, AE_INGAME_DELTA_SAY = 10,
	AE_INGAME_DELTA_RESTART = 11,

	/* the ban lengths the confirm offers (F-4), 1 DAY first */
	AE_INGAME_BAN_LENGTHS = 4, AE_INGAME_BAN_FIRST = 1,
	/* a restart from the page waits this long, the players warned (60, 30, 10 and 5 s left) */
	AE_INGAME_RESTART_SECONDS = 60,
	/* the moderator key as text */
	AE_INGAME_KEY_HEX = 64,

	/* the wire */
	AE_INGAME_WIRE_VERSION = 1,
	AE_INGAME_WIRE_NAME = 32,
	AE_INGAME_WIRE_ENTRIES = 10,
	AE_INGAME_WIRE_MAP = 28,
	AE_INGAME_WIRE_GAMETYPE = 20,
	AE_INGAME_WIRE_HEAD = 16 + AE_INGAME_WIRE_NAME,
	AE_INGAME_WIRE_ENTRY = AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE + 2,
	AE_INGAME_WIRE_MAXIMUM = AE_INGAME_WIRE_HEAD + AE_INGAME_WIRE_ENTRIES * AE_INGAME_WIRE_ENTRY,
	AE_INGAME_WIRE_NO_ENTRY = 255,
	/* the flags */
	AE_INGAME_WIRE_HOLDING = 1,           /* NEW PLAYERS held (a whitelist or a stop that waits) */
	AE_INGAME_WIRE_RESTARTING = 2,        /* a stop or restart counts down */
	AE_INGAME_WIRE_FLAGS = 3,
	/* an entry's flag: it cannot be played (its map is missing, or its text is wrong) */
	AE_INGAME_ENTRY_REJECTED = 1,
};

/* ---------- roles and actions */

/* the server command an action runs ("sv_team", "mute", "kick", "ban", "sv_rotation_begin" (the next game: never the one
played now), "sv_end_game", "say", "sv_restart") */
const char *ae_ingame_command(int action);
/* the actions a role may take, as a mask (a bit each), given the role each command needs on this server (the command
table's own, or command_access.txt's): role_of(command, context). A ban also needs the admin role (the approved
design: moderators move, mute and kick; the playlist and bans are an admin's), whatever the command table says */
typedef int (*ae_ingame_role_of)(void *context, const char *command);
unsigned int ae_ingame_allowed(int role, ae_ingame_role_of role_of, void *context);
int ae_ingame_allows(unsigned int mask, int action);
/* the role word the page shows (YOU: ADMIN), upper case: "MODERATOR", "ADMIN", "OWNER"; "" for none */
const char *ae_ingame_role_word(int role);

/* ---------- the ban lengths (F-4) */

/* the minutes of a length (0: for ever) */
int ae_ingame_ban_minutes(int length);
/* its chip ("1 HOUR", "1 DAY", "1 WEEK", "FOREVER") */
const char *ae_ingame_ban_word(int length);
/* the confirm's strong choice: "BAN GOAT FOR 1 DAY", "BAN GOAT FOREVER" (the name upper case, cut to fit) */
void ae_ingame_ban_yes(const char *name, int length, char *text, int size);
/* the server command's length word: "60m", "1440m", "10080m", "forever" */
void ae_ingame_ban_command_length(int minutes, char *text, int size);

/* the player a card names among the players of one machine (names[0..count-1], in slot order): with a name given, only
the player of that name (-1 when it has left: the machine may hold a split-screen mate, or a new joiner may have taken
its index); with none, the first. -1: nobody */
int ae_ingame_card_pick(int count, char const *const *names, char const *name);

/* ---------- the player card's words */

/* MOVE TO BLUE TEAM / MOVE TO RED TEAM (team 0 red, 1 blue: the team they are on now) */
const char *ae_ingame_move_label(int team_now);
/* the card's line under its rows: "You are an ADMIN on this server. The web page lists every kick and ban (its audit
log)." */
void ae_ingame_card_role_line(int role, char *text, int size);

/* ---------- admins.txt's key rows */

/* 64 hexadecimal characters, made lower case in kept: 1 if it is a key */
int ae_ingame_key_hex(const char *text, char *kept, int size);
/* the bytes of a key's text (32): 1 if it is one */
int ae_ingame_key_bytes(const char *text, unsigned char *key);
/* a key's 32 bytes as 64 lower case hex characters and an end */
void ae_ingame_key_text(const unsigned char *key, char *text);

/* ---------- a reason or a line for everyone: printable ASCII, no quote or backslash (a server command's last word),
spaces at the ends dropped; 1 if anything is left */
int ae_ingame_clean_text(const char *text, char *kept, int size);

/* the player a card names among the players of one machine (names[0..count-1], in slot order): with a name given, only
the player of that name (-1 when it has left: the machine may hold a split-screen mate, or a new joiner may have taken
its index); with none, the first. -1: nobody */
int ae_ingame_card_pick(int count, char const *const *names, char const *name);

/* ---------- the server's admin message (distributed message 113) */

struct ae_ingame_entry
{
	char map[AE_INGAME_WIRE_MAP];             /* the rotation's map: "bloodgulch", "h2d_sanctuary", "beavercreek@pro" */
	char gametype[AE_INGAME_WIRE_GAMETYPE];   /* its gametype word: "ae_team_sly", "slayer" */
	unsigned char flags;                      /* AE_INGAME_ENTRY_REJECTED */
	unsigned char index;                      /* its number in the rotation (sv_rotation's) */
};

struct ae_ingame_wire
{
	unsigned char role;                       /* the player's role here (1 to 3) */
	unsigned char flags;                      /* AE_INGAME_WIRE_* */
	unsigned short allowed;                   /* the actions their role takes here (ae_ingame_allowed) */
	unsigned char players;                    /* the people playing, bots counted apart */
	unsigned char bots;
	unsigned char max_players;
	unsigned char count;                      /* the entries sent: the games to come, in the order they are played,
	                                             from the next on (AE_INGAME_WIRE_ENTRIES at most) */
	unsigned char total;                      /* the rotation's entries, sent or not (255 at most) */
	unsigned char now;                        /* the entry played now (AE_INGAME_WIRE_NO_ENTRY: none, or one put in) */
	unsigned char next;                       /* the entry played next (likewise) */
	unsigned short up_minutes;                /* the server has been up this long */
	unsigned short restart_seconds;           /* with RESTARTING: the seconds left */
	char name[AE_INGAME_WIRE_NAME];           /* the server's name */
	struct ae_ingame_entry entries[AE_INGAME_WIRE_ENTRIES];
};

/* the bytes to send: their count (0 if the state is not one to send) */
int ae_ingame_wire_encode(const struct ae_ingame_wire *state, unsigned char *bytes, int size);
/* a body read: 1 if it is whole and every field in range (texts printable and ended) */
int ae_ingame_wire_decode(const unsigned char *bytes, int size, struct ae_ingame_wire *state);

/* ---------- the SERVER page's UP NEXT */

/* left (-1) or right (1) from the entry shown (its place in entries): the next that can be played, round the list,
skipping rejected ones and the one played now (unless it is the only one); the same place if none other can */
int ae_ingame_next_step(const struct ae_ingame_wire *state, int shown, int direction);
/* an entry's words for the page: "SANCTUARY" (the map, upper case, its pack or Workshop part dropped) and
"AE KING" (the gametype word, upper case, its underscores spaces) */
void ae_ingame_map_word(const char *map, char *text, int size);
void ae_ingame_gametype_word(const char *gametype, char *text, int size);
/* "2 h 14 min", "14 min", "under a minute" */
void ae_ingame_up_text(unsigned int minutes, char *text, int size);


/* ---------- the sign-in offer (distributed message 114)

A client signs in to a server's moderation (sending its moderator key's public half) only where this PC has a role.
Before it does, the server offers a random salt of its own and the salted hashes of the key rows it holds (its owner's
key too); the client hashes its own key with that salt and signs in only on a match, so a stranger's server never learns
a key it did not already list. A server also says whether the address a player joins from holds a role ("cidr:" rows):
a role the key does not give. */

enum
{
	AE_INGAME_OFFER_VERSION = 1,
	AE_INGAME_OFFER_SALT = 16,
	AE_INGAME_OFFER_HASH = 8,
	AE_INGAME_OFFER_HASHES = 32,
	AE_INGAME_OFFER_HEAD = 4 + AE_INGAME_OFFER_SALT,
	AE_INGAME_OFFER_MAXIMUM = AE_INGAME_OFFER_HEAD + AE_INGAME_OFFER_HASHES * AE_INGAME_OFFER_HASH,
	AE_INGAME_OFFER_BY_ADDRESS = 1,       /* the address this player joined from holds a role here */
	AE_INGAME_OFFER_FLAGS = 1,
};

struct ae_ingame_offer
{
	unsigned char flags;                  /* AE_INGAME_OFFER_* */
	unsigned char count;                  /* the hashes (AE_INGAME_OFFER_HASHES at most) */
	unsigned char salt[AE_INGAME_OFFER_SALT];
	unsigned char hashes[AE_INGAME_OFFER_HASHES][AE_INGAME_OFFER_HASH];
};

/* the first 8 bytes of SHA-256(salt, key) */
void ae_ingame_offer_hash(const unsigned char *salt, const unsigned char *key, unsigned char *hash);
/* a key's hash added to the offer (its salt set): 1, 0 if the offer is full */
int ae_ingame_offer_add(struct ae_ingame_offer *offer, const unsigned char *key);
/* the bytes to send (always even): their count */
int ae_ingame_offer_encode(const struct ae_ingame_offer *offer, unsigned char *bytes, int size);
/* a body read: 1 if whole and every field in range */
int ae_ingame_offer_decode(const unsigned char *bytes, int size, struct ae_ingame_offer *offer);
/* whether this PC signs in on that offer: its address row holds a role, or its own key's hash is in the list */
int ae_ingame_offer_matches(const struct ae_ingame_offer *offer, const unsigned char *own_key);

#endif
