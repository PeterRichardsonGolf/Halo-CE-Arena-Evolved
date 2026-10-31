/*
AE_LOBBY_COUNTDOWN.H

(0.4.3) CREATE GAME's start countdown as every machine reads it (mockups-042/countdown SPEC, owner 10-28 "Build as
recommended"): the seconds the server sends to all machines (message_server_pregame_countdown, read through
network_game_client_get_seconds_to_game_start) followed frame by frame into what the START GAME row shows, the moments
to log and beep, the drain bar's time left, and which beep a number gets. Pure, no engine includes (unit test
port/linux/tests/ae_lobby_countdown_test.c): ae_screen_create_game.c draws it, ae_glue_sound.c sounds it.

The server sends the floored seconds about once a second: 5999 ms shows 5, 4, 3, 2, 1 a second each, then 0 (GO) for
the rest, and the game starts at 0 once every machine has the map. A 0 that lasts (a joiner still loading or
downloading the map: the engine's own hold) is HELD. NONE after a number is a cancel (the host's B, or the server's
own when the lobby can no longer start); a client past its pregame means the game is under way, not a cancel (the
client clears its seconds as the game starts).
*/

#ifndef __AE_LOBBY_COUNTDOWN_H
#define __AE_LOBBY_COUNTDOWN_H

enum ae_countdown_phase
{
	AE_COUNTDOWN_IDLE,
	AE_COUNTDOWN_PENDING,    /* the host pressed START GAME; the server's first number is a frame away */
	AE_COUNTDOWN_COUNTING,   /* a number over 0 */
	AE_COUNTDOWN_GO,         /* 0: the game starts (once every machine has the map) */
	AE_COUNTDOWN_HELD,       /* 0 for longer than AE_COUNTDOWN_HOLD_MS: machines still loading the map */
	AE_COUNTDOWN_CANCELLING  /* the host cancelled: its own client's last number is stale until the NONE comes */
};

/* what a frame's seconds brought (ae_countdown_update) */
enum
{
	AE_COUNTDOWN_NOTHING,
	AE_COUNTDOWN_NUMBER,     /* a new number over 0 */
	AE_COUNTDOWN_ZERO,       /* 0 came */
	AE_COUNTDOWN_HOLD,       /* 0 lasted: held */
	AE_COUNTDOWN_CANCELLED,  /* NONE after a number (or a host's start that never got one) */
	AE_COUNTDOWN_STARTED     /* the game is under way */
};

/* the seconds as the glue reads them (ae_lobby_seconds_to_start): none (the engine's NONE), the game under way */
enum { AE_COUNTDOWN_SECONDS_NONE = -1, AE_COUNTDOWN_SECONDS_STARTED = -2 };

enum
{
	/* 0 for this long is the hold, not the start's GO (GO lasts ~0.9 s at 5999 ms) */
	AE_COUNTDOWN_HOLD_MS = 1500,
	/* the host's start with no number from the server in this long: it never began (the lobby could not start) */
	AE_COUNTDOWN_PENDING_MS = 2500
};

struct ae_countdown
{
	int phase;
	short seconds;              /* the number shown (PENDING: the host's start's; GO, HELD: 0) */
	short whole;                /* the bar's whole, in seconds: the first number seen + 1 (5 -> 6 s) */
	unsigned long changed_at;   /* when the number shown came (the bar drains from there) */
};

void ae_countdown_reset(struct ae_countdown *countdown);
/* the host pressed START GAME counting from milliseconds (5999: 5): shown at once, before the server's first number */
void ae_countdown_begin(struct ae_countdown *countdown, long milliseconds, unsigned long now);
/* the host cancelled: START GAME shows again at once; the numbers its client still has are ignored until the NONE
the cancel sent comes back (or AE_COUNTDOWN_PENDING_MS went by) */
void ae_countdown_cancel(struct ae_countdown *countdown, unsigned long now);
/* a frame's seconds (>= 0, AE_COUNTDOWN_SECONDS_NONE or _STARTED): what they brought, AE_COUNTDOWN_NOTHING to
_STARTED */
int ae_countdown_update(struct ae_countdown *countdown, short seconds, unsigned long now);
/* whether the row shows the countdown (PENDING, COUNTING, GO, HELD; not CANCELLING): the host's input locked but for
cancel */
int ae_countdown_running(struct ae_countdown const *countdown);
/* the drain bar: the time left as a fraction of the whole (1 full, 0 empty), smooth from the moment the number
changed, never below the number's own second (a late message stalls it rather than jumping back); 0 idle or held */
float ae_countdown_left(struct ae_countdown const *countdown, unsigned long now);

/* the beep a number gets (owner 10-28, the SPEC's option 1: the beeps come right before play). The gametype without
PRE-GAME COUNTDOWN: a TICK at each number, GO at 0. With it (its own 3-2-1 beeps in the game): one SOFT tick a number,
nothing at 0 */
enum { AE_BEEP_NONE, AE_BEEP_TICK, AE_BEEP_SOFT, AE_BEEP_GO };
int ae_countdown_beep(short seconds, int pregame_countdown);

/* the beeps' tones, made in code (no asset): TICK 880 Hz 90 ms, SOFT 880 Hz 60 ms at a third of TICK's level, GO
1320 Hz 220 ms; 8 ms fades in and out. Into samples (mono, 16 bit, rate Hz), at most maximum frames: the frames
written (0 for AE_BEEP_NONE) */
enum { AE_BEEP_RATE = 44100, AE_BEEP_FRAMES = AE_BEEP_RATE / 4 };
unsigned long ae_countdown_tone(int beep, short *samples, unsigned long maximum, unsigned long rate);

#endif
