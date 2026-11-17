/*
AE_BOTS_LEARNT_RULES.H

What the bots keep of a map from game to game (ae_bots_learnt.h; owner 2026-11-15, ask-me: "Keep stuck spots" and "Learn
your jumps"), as pure rules: the learnt file's text read and written, a game's news merged in, what fades, and which of a
person's jumps are kept. No engine includes: port/linux/tests/ae_bots_learnt_rules_test.c compiles these alone.

A map's learnt file (the config folder's bots_learnt/<map file>.txt) begins with the map's layout header line
("layout N <file> <hash>", ae_layout_map_header): a file of another header is another map's (the map changed) and is
not used. Its lines:
	wedge <surface> <count>                          a walkable piece where drivers wedged or crashed
	stuck <surface> <count> <x> <y> <z>              a walkable piece where bots on foot stood stuck
	jump <ax> <ay> <az> <bx> <by> <bz> <uses> <misses>
	                                                 a person's jump from A to B that the bots may take
	deadly <volume>                                  a trigger volume of the map's that kills (0.9.4)
Lines it does not know are skipped. Surfaces are the map's collision surfaces (the same file, the same numbers).

- WEDGES: a game's wedges add to a piece's count, up to AE_LEARNT_WEDGE_MOST; a game with AE_LEARNT_DRIVE_SECONDS of bot
  driving or more and none on a piece takes one off it (a piece the drivers keep away from fades and is tried again).
- STUCK: one report a bot a piece a game, at most AE_LEARNT_STUCK_A_GAME a piece a game; the count is kept up to
  AE_LEARNT_STUCK_MOST, and fades as wedges do in games of AE_LEARNT_PLAY_SECONDS with bots. Bots on foot pay to walk
  onto a piece from AE_LEARNT_STUCK_TO_AVOID reports (ae_learnt_stuck_levels): a cost, never a wall.
- JUMPS: a person's jump is kept when it rose or dropped AE_LEARNT_JUMP_HEIGHT or more, was AE_LEARNT_JUMP_AIR seconds or
  more in the air, no faster across the ground than AE_LEARNT_JUMP_SPEED, and the person was alive AE_LEARNT_JUMP_ALIVE
  seconds after landing (ae_learnt_jump_keep). One whose
  ends are both within AE_LEARNT_JUMP_SAME of a kept one's is that one again (its uses + 1, up to AE_LEARNT_JUMP_USES).
  At most AE_LEARNT_JUMPS a map: a new one takes the place of the one used least. A game in which the bots missed a
  kept jump AE_LEARNT_JUMP_GAME_MISSES times or more and made it never adds a miss; at AE_LEARNT_JUMP_MISSES it is
  dropped.
*/

#ifndef __AE_BOTS_LEARNT_RULES_H
#define __AE_BOTS_LEARNT_RULES_H

#define AE_LEARNT_HEADER 96
#define AE_LEARNT_WEDGES 64
#define AE_LEARNT_WEDGE_MOST 4
#define AE_LEARNT_DRIVE_SECONDS 60
#define AE_LEARNT_STUCKS 64
#define AE_LEARNT_STUCK_A_GAME 2
#define AE_LEARNT_STUCK_MOST 6
#define AE_LEARNT_STUCK_TO_AVOID 3
#define AE_LEARNT_STUCK_LEVELS 4
#define AE_LEARNT_PLAY_SECONDS 120
#define AE_LEARNT_JUMPS 48
#define AE_LEARNT_JUMP_HEIGHT 0.8f
#define AE_LEARNT_JUMP_AIR 0.33f
#define AE_LEARNT_JUMP_ALIVE 5.0f
#define AE_LEARNT_JUMP_SAME 1.5f
#define AE_LEARNT_JUMP_USES 9
#define AE_LEARNT_JUMP_GAME_MISSES 2
#define AE_LEARNT_JUMP_MISSES 3
/* a jump faster than this across the ground (units a second in the air) is not a run's jump: a grenade's, an
explosion's, a launcher's; not kept */
#define AE_LEARNT_JUMP_SPEED 5.0f
/* coordinates read from the file beyond this (or not numbers) are a broken line's: skipped */
#define AE_LEARNT_FARTHEST 100000.0f
/* the bots a game's stuck reports can tell apart */
#define AE_LEARNT_BOTS 128
/* DEADLY VOLUMES (0.9.4; Fractured Peak 11-15: 43 bot deaths in the script's kill volume under the cracks): the map's
trigger volumes a unit was killed in by the map (an act of god, ae_bots_falls.inc), at most AE_LEARNT_DEADLY; one a
living unit stood in is taken off (ae_learnt_deadly_remove) */
#define AE_LEARNT_DEADLY 16

struct ae_learnt_wedge
{
	long surface;
	int count;
	/* this game's new ones (not in the file) */
	int fresh;
};

struct ae_learnt_stuck
{
	long surface;
	int count;
	float x, y, z;
	int fresh;
	/* the bots that reported it this game (one report a bot) */
	unsigned char reported[AE_LEARNT_BOTS];
};

struct ae_learnt_jump
{
	float from[3];
	float to[3];
	int uses;
	int misses;
	/* this game's: the bots' misses and makes */
	int game_misses;
	int game_makes;
};

struct ae_learnt_map
{
	char header[AE_LEARNT_HEADER];
	int wedge_count;
	struct ae_learnt_wedge wedges[AE_LEARNT_WEDGES];
	int stuck_count;
	struct ae_learnt_stuck stucks[AE_LEARNT_STUCKS];
	int jump_count;
	struct ae_learnt_jump jumps[AE_LEARNT_JUMPS];
	int deadly_count;
	long deadly[AE_LEARNT_DEADLY];
};

/* a map's learnt state from its file's text (NUL-ended), for the map of `header`: 1, or 0 when the text is another
map's (its header differs) or has no header; the state is then empty, with `header` */
int ae_learnt_read(struct ae_learnt_map *map, char const *text, char const *header);
/* the state as the file's text: the characters written (without the NUL), 0 when it did not fit */
int ae_learnt_write(struct ae_learnt_map const *map, char *text, int size);

/* a game's news: a wedge on a piece; a bot's stuck report on a piece at (x, y, z); a kept jump missed or made by a bot */
void ae_learnt_wedge(struct ae_learnt_map *map, long surface);
void ae_learnt_stuck(struct ae_learnt_map *map, long surface, int bot, float x, float y, float z);
void ae_learnt_jump_result(struct ae_learnt_map *map, int jump, int made);
/* a person's landed jump from `from` to `to`: whether it is one to keep (the rules above); then kept: the kept jump's
index (a new one, or the one it repeats) */
int ae_learnt_jump_keep(float const *from, float const *to, float seconds_in_air, float alive_seconds_after);
int ae_learnt_jump_add(struct ae_learnt_map *map, float const *from, float const *to);
/* the kept jump whose ends are both within AE_LEARNT_JUMP_SAME of these, -1 when none: how a bot's result finds its
jump (by its ends, not its index: the list changes while the links of a map load stay) */
int ae_learnt_jump_find(struct ae_learnt_map const *map, float const *from, float const *to);

/* the game over: this game's news into the counts, the fading of what had none (by the game's seconds of bot driving
and of play with bots), misses counted, the empty dropped; this game's marks cleared */
void ae_learnt_game_over(struct ae_learnt_map *map, int drive_seconds, int play_seconds);

/* a piece's stuck count to the foot route's penalty levels: 0 under AE_LEARNT_STUCK_TO_AVOID, then 1 up to
AE_LEARNT_STUCK_LEVELS */
int ae_learnt_stuck_levels(int count);

/* a deadly volume kept (1: new to the list; 0: there already, none, or the list full), taken off (1: it was there), and
whether it is kept */
int ae_learnt_deadly_add(struct ae_learnt_map *map, long volume);
int ae_learnt_deadly_remove(struct ae_learnt_map *map, long volume);
int ae_learnt_deadly_has(struct ae_learnt_map const *map, long volume);

#endif
