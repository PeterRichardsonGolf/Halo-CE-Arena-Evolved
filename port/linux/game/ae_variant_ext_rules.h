/* AE's extra gametype settings (the 'AEXV' block): the settings a gametype has besides Halo CE's variant and
OpenCE's options (game_engine.h), from the second-generation game's settings list: rounds for every mode, player
traits, power-ups on the map, slayer points, king/oddball/CTF/bomb/juggernaut rules, and the AE ASSAULT and
TERRITORIES modes. Every field's zero is Halo CE's (and AE's before it) play, so a gametype without the block,
or a build without it, plays as before. No game dependency: unit-tested (port/linux/tests/ae_variant_ext_rules_test.c).
The block's place in the file and on the network: ae_variant_ext.c. */
#ifndef AE_VARIANT_EXT_RULES_H
#define AE_VARIANT_EXT_RULES_H

enum
{
	AE_VARIANT_EXT_SIZE = 48,
	AE_VARIANT_EXT_VERSION = 1,
};

/* NUMBER OF ROUNDS (0: the gametype's own, which is one round, AE SND's its ROUNDS TO WIN) */
enum ae_ext_rounds
{
	AE_EXT_ROUNDS_GAMETYPE = 0,
	AE_EXT_ROUNDS_1, AE_EXT_ROUNDS_2, AE_EXT_ROUNDS_4, AE_EXT_ROUNDS_6,
	AE_EXT_ROUNDS_BEST_OF_3, AE_EXT_ROUNDS_BEST_OF_5, AE_EXT_ROUNDS_BEST_OF_7,
	AE_EXT_ROUNDS_COUNT
};

/* RESPAWN TIME MODIFIER */
enum ae_ext_respawn_modifier
{
	AE_EXT_RESPAWN_MODIFIER_NONE = 0,
	/* a player who dies waits as long as the teammate who died last has left */
	AE_EXT_RESPAWN_MODIFIER_INHERITANCE,
	/* a team's players come back together, in waves of the respawn time */
	AE_EXT_RESPAWN_MODIFIER_CYCLING,
	AE_EXT_RESPAWN_MODIFIER_COUNT
};

/* WEAPON RESPAWN: how long a map weapon takes to come back */
enum ae_ext_weapon_respawn
{
	AE_EXT_WEAPON_RESPAWN_MAP = 0,
	AE_EXT_WEAPON_RESPAWN_NONE,
	AE_EXT_WEAPON_RESPAWN_HALF,
	AE_EXT_WEAPON_RESPAWN_DOUBLE,
	AE_EXT_WEAPON_RESPAWN_COUNT
};

/* a waypoint rule (oddball's BALL WAYPOINT, CTF's OWN FLAG WAYPOINT, the ENEMY BOMB INDICATOR) */
enum ae_ext_waypoint
{
	AE_EXT_WAYPOINT_GAMETYPE = 0,
	AE_EXT_WAYPOINT_ALWAYS,
	/* oddball: when nobody holds the ball; CTF: when the flag is away from home; bomb: when nobody holds it */
	AE_EXT_WAYPOINT_LOOSE,
	/* oddball: when a team holds it; bomb: when it is armed */
	AE_EXT_WAYPOINT_HELD,
	AE_EXT_WAYPOINT_OFF,
	AE_EXT_WAYPOINT_COUNT
};

/* CTF's FLAG TYPE, the bomb's BOMB TYPE */
enum ae_ext_objective_type
{
	AE_EXT_OBJECTIVE_GAMETYPE = 0,
	/* each team has its own (CTF's multi flag) */
	AE_EXT_OBJECTIVE_EVERY_TEAM,
	/* one team defends it, the other attacks, sides switching each round */
	AE_EXT_OBJECTIVE_SINGLE,
	/* one in the middle of the map: both teams carry it to the other's base */
	AE_EXT_OBJECTIVE_NEUTRAL,
	AE_EXT_OBJECTIVE_COUNT
};

/* the movement of a carrier or the juggernaut */
enum ae_ext_movement
{
	AE_EXT_MOVEMENT_GAMETYPE = 0,
	AE_EXT_MOVEMENT_SLOW,
	AE_EXT_MOVEMENT_NORMAL,
	AE_EXT_MOVEMENT_FAST,
	AE_EXT_MOVEMENT_COUNT
};

/* the mode a gametype's engine plays (besides its engine's own) */
enum ae_ext_mode
{
	AE_EXT_MODE_NONE = 0,
	/* the Bomb engine: respawns, a team scores by arming the bomb in the other's base */
	AE_EXT_MODE_ASSAULT,
	/* the King engine: teams take and hold territories, a point a second each */
	AE_EXT_MODE_TERRITORIES,
	AE_EXT_MODE_COUNT
};

/* the flags of a group: their zero is the gametype's own play */
enum
{
	/* match */
	AE_EXT_MATCH_ROUNDS_KEEP_MAP = 0x01,  /* ROUNDS RESET MAP off */
	AE_EXT_MATCH_RESOLVE_TIES = 0x02,

	/* players */
	AE_EXT_PLAYER_DAMAGE_RESISTANCE = 0x01,
	AE_EXT_PLAYER_EXTRA_DAMAGE = 0x02,
	AE_EXT_PLAYER_OVERSHIELDS = 0x04,     /* SHIELD TYPE OVERSHIELDS: everyone spawns overshielded */
	AE_EXT_PLAYER_NO_CAMO_ON_MAP = 0x08,
	AE_EXT_PLAYER_NO_OVERSHIELDS_ON_MAP = 0x10,

	/* slayer */
	AE_EXT_SLAYER_DEATH_POINT_LOSS = 0x01,
	AE_EXT_SLAYER_SUICIDE_POINT_LOSS = 0x02,
	AE_EXT_SLAYER_BONUS_POINTS = 0x04,

	/* king (and territories' hills) */
	AE_EXT_KING_UNCONTESTED = 0x01,       /* a hill scores only while nobody else is in it */
	AE_EXT_KING_TEAM_TIME_MULTIPLIER = 0x02, /* a team scores a point a second for each of its players in it */
	AE_EXT_KING_CAMO_ON_HILL = 0x04,
	AE_EXT_KING_DAMAGE_RESISTANCE_ON_HILL = 0x08,
	AE_EXT_KING_EXTRA_DAMAGE_ON_HILL = 0x10,

	/* oddball (the flags byte's high bits; the low nibble is the BALL WAYPOINT) */
	AE_EXT_ODDBALL_CARRIER_CAMO = 0x10,
	AE_EXT_ODDBALL_TOUGHNESS = 0x20,      /* the carrier resists damage */
	AE_EXT_ODDBALL_NO_VEHICLES = 0x40,    /* VEHICLE OPERATION off: the carrier cannot drive or ride */

	/* CTF */
	AE_EXT_CTF_TOUCH_RETURN = 0x01,
	AE_EXT_CTF_SUDDEN_DEATH = 0x02,
	AE_EXT_CTF_SLOW_CARRIER = 0x04,
	AE_EXT_CTF_MASSIVE_DAMAGE = 0x08,     /* a melee blow with the flag kills */
	AE_EXT_CTF_CARRIER_CAMO = 0x10,
	AE_EXT_CTF_CARRIER_DAMAGE_RESISTANCE = 0x20,
	AE_EXT_CTF_NO_DRIVING = 0x40,         /* DRIVE WITH FLAG off: the carrier cannot take a driver's seat */

	/* bomb (AE SND and AE ASSAULT) */
	AE_EXT_BOMB_STICKY_ARMING = 0x01,     /* stepping off keeps the arming's progress */
	AE_EXT_BOMB_TOUCH_RETURN = 0x02,
	AE_EXT_BOMB_SUDDEN_DEATH = 0x04,
	AE_EXT_BOMB_SLOW_CARRIER = 0x08,
	AE_EXT_BOMB_MASSIVE_DAMAGE = 0x10,
	AE_EXT_BOMB_NO_DRIVING = 0x20,        /* DRIVE WITH BOMB off: the carrier cannot take a driver's seat */
	AE_EXT_BOMB_CARRIER_CAMO = 0x40,
	AE_EXT_BOMB_CARRIER_DAMAGE_RESISTANCE = 0x80,

	/* juggernaut */
	AE_EXT_JUGGERNAUT_CAMO = 0x01,
	AE_EXT_JUGGERNAUT_DAMAGE_RESISTANCE = 0x02,
	AE_EXT_JUGGERNAUT_EXTRA_DAMAGE = 0x04,
	AE_EXT_JUGGERNAUT_INFINITE_AMMO = 0x08,
	AE_EXT_JUGGERNAUT_MOTION_SENSOR = 0x10,
	AE_EXT_JUGGERNAUT_OVERSHIELD = 0x20,
	AE_EXT_JUGGERNAUT_BETRAYAL_POINT_LOSS = 0x40,
};

/* the block's 48 bytes, each a byte so the struct is its own wire and file form */
struct ae_variant_ext
{
	unsigned char rounds;               /* enum ae_ext_rounds */
	unsigned char round_time_10s;       /* ROUND TIME LIMIT, tens of seconds; 0 none */
	unsigned char match_flags;          /* AE_EXT_MATCH_* */
	unsigned char respawn_modifier;     /* enum ae_ext_respawn_modifier */
	unsigned char player_flags;         /* AE_EXT_PLAYER_* */
	unsigned char max_active_players;   /* 0 all; 2-16 */
	unsigned char weapon_respawn;       /* enum ae_ext_weapon_respawn */
	unsigned char slayer_flags;         /* AE_EXT_SLAYER_* */
	unsigned char king_move_10s;        /* MOVING HILL's time, tens of seconds; 0 the king variant's own */
	unsigned char king_flags;           /* AE_EXT_KING_* */
	unsigned char oddball_flags;        /* low nibble enum ae_ext_waypoint; AE_EXT_ODDBALL_* */
	unsigned char ctf_flag_type;        /* enum ae_ext_objective_type */
	unsigned char ctf_reset_time;       /* seconds a loose flag lies; 0 the gametype's own */
	unsigned char ctf_flags;            /* AE_EXT_CTF_* */
	unsigned char ctf_own_flag_waypoint;/* enum ae_ext_waypoint */
	unsigned char bomb_type;            /* enum ae_ext_objective_type */
	unsigned char bomb_flags;           /* AE_EXT_BOMB_* */
	unsigned char bomb_enemy_indicator; /* enum ae_ext_waypoint */
	unsigned char mode;                 /* enum ae_ext_mode */
	unsigned char juggernaut_movement;  /* enum ae_ext_movement */
	unsigned char juggernaut_flags;     /* AE_EXT_JUGGERNAUT_* */
	unsigned char territories;          /* 0 the map's hills; 1-8 */
	unsigned char control_time;         /* seconds; 0 the default (5) */
	unsigned char contest_time;         /* seconds; 0 the default (5) */
	unsigned char carrier_movement;     /* oddball/CTF/bomb carrier speed: enum ae_ext_movement */
	unsigned char reserved[AE_VARIANT_EXT_SIZE - 25];
};

typedef char ae_variant_ext_size_check[sizeof(struct ae_variant_ext) == AE_VARIANT_EXT_SIZE ? 1 : -1];

/* every field Halo CE's play */
void ae_variant_ext_default(struct ae_variant_ext *ext);
/* 1 when every field is zero (no block is needed) */
int ae_variant_ext_is_default(struct ae_variant_ext const *ext);
/* every field in range (another machine's, a file's): out of range is the default; returns 1 when nothing changed */
int ae_variant_ext_sanitize(struct ae_variant_ext *ext);

/* rounds a game plays (0: the gametype's own), and the wins a team needs (best of: a majority, else every round
is played and the most wins take it) */
int ae_ext_rounds_count(struct ae_variant_ext const *ext);
int ae_ext_rounds_to_win(struct ae_variant_ext const *ext);
/* the round time in seconds (0 none) */
long ae_ext_round_time_seconds(struct ae_variant_ext const *ext);
/* a map weapon's respawn ticks from the map's (NONE: never again) */
long ae_ext_weapon_respawn_ticks(struct ae_variant_ext const *ext, long map_ticks);
/* the damage a player takes (percent) and deals (percent), given the traits that apply to the player now */
int ae_ext_damage_taken_percent(int resistant);
int ae_ext_damage_dealt_percent(int extra);
/* a movement's speed (percent of normal) */
int ae_ext_movement_percent(int movement);
/* MOVING HILL's seconds between moves (0: the king variant's own rule) */
long ae_ext_king_move_seconds(struct ae_variant_ext const *ext);
/* a respawn wait, in ticks, under the RESPAWN TIME MODIFIER: base the player's own wait, team_last_left the ticks
the teammate who died last still waits (NONE: none waiting), wave_ticks the cycle's length, now_ticks the game
time */
long ae_ext_respawn_wait_ticks(int modifier, long base, long team_last_left, long now_ticks);
/* the waypoint shows: rule (enum ae_ext_waypoint), whether the objective is held, armed, home */
int ae_ext_waypoint_shows(int rule, int held, int armed, int home);
/* territories: the count in play (count 1-8, else the map's hills up to 8), the control and contest seconds */
int ae_ext_territories_count(struct ae_variant_ext const *ext, int map_hills);
int ae_ext_control_seconds(struct ae_variant_ext const *ext);
int ae_ext_contest_seconds(struct ae_variant_ext const *ext);

#endif
