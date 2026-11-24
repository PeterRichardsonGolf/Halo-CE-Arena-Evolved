/*
AE_PAD_NOTICE_RULES.H

A controller's battery and connection in words, for every pad: when the battery notice shows (low at 15 % or less on
the battery, once a drop; charging only after a low notice), the notice's texts, the details line of SETTINGS' card and
of CONTROLS' rail foot ("STEAM CONTROLLER . PUCK . BATTERY 80 %", each part its own text so the screen draws the dots
between them), and the backpack note's condition and words. No SDL here (port/linux/tests/ae_pad_notice_rules_test.c);
ae_steam_pad.c reads the pads and ae_pad_notice.c draws.
*/

#ifndef AE_PAD_NOTICE_RULES_H
#define AE_PAD_NOTICE_RULES_H

/* what a pad's power is (SDL's power state, as the pad reports it) */
enum
{
	AE_PAD_POWER_UNKNOWN = 0,
	AE_PAD_POWER_BATTERY = 1,    /* running on its battery */
	AE_PAD_POWER_CHARGING = 2,
	AE_PAD_POWER_CHARGED = 3,
	AE_PAD_POWER_NO_BATTERY = 4  /* plugged in, no battery to speak of */
};

/* the notice shows at this percent or less on the battery, and is armed again at AE_PAD_BATTERY_REARM_PERCENT or more
(or on charging); it shows for AE_PAD_NOTICE_SECONDS, then fades for AE_PAD_NOTICE_FADE_SECONDS */
#define AE_PAD_BATTERY_LOW_PERCENT 15
#define AE_PAD_BATTERY_REARM_PERCENT 20
#define AE_PAD_NOTICE_SECONDS 6.0
#define AE_PAD_NOTICE_FADE_SECONDS 0.4

enum { AE_PAD_NOTICE_NONE = 0, AE_PAD_NOTICE_LOW = 1, AE_PAD_NOTICE_CHARGING = 2 };

/* one pad's notice state: zeroed at the start of the session (and when the pad is another one) */
struct ae_pad_battery_state
{
	unsigned id;         /* the pad this state is of (SDL's instance id; 0: none): another pad on the port starts it again */
	int started;         /* the pad was read once */
	int disarmed;        /* a low notice was given and the level has not risen to the re-arm level, nor has the pad charged */
	int low_given;       /* a low notice was given this session (the charging notice may follow) */
	int was_charging;    /* the last read said charging */
};

/* the pad on the port now (id, 0 for none): a pad other than the state's own clears the state, so a pad that takes the place of
another does not inherit its low notice, its re-arm or its charging history; returns 1 when it cleared */
int ae_pad_battery_rules_pad(struct ae_pad_battery_state *state, unsigned id);

/* a read of the pad: its power and its percent (-1: not known); returns the notice due now (AE_PAD_NOTICE_*).
UNKNOWN power and NO_BATTERY give none; the first read of a pad that is low gives the low notice */
int ae_pad_battery_rules_step(struct ae_pad_battery_state *state, int power, int percent);

/* the notice's text: "CONTROLLER BATTERY LOW 12 %", "CONTROLLER CHARGING 14 %" (no percent: without it), with
"PLAYER 2: " in front when player is 2 or more (0 or 1: none); false for AE_PAD_NOTICE_NONE */
enum { AE_PAD_NOTICE_TEXT = 96 };
int ae_pad_notice_text(int notice, int player, int percent, char text[AE_PAD_NOTICE_TEXT]);

/* a notice's alpha (0 to 1) this many milliseconds after it began: full for AE_PAD_NOTICE_SECONDS, then the fade; 0 after */
double ae_pad_notice_alpha(unsigned long elapsed_ms);
/* the menu notice's colour pulse, 0 (the title colour) to 1 (ice blue): two pulses in the first second; 0 after, and always 0
with reduce motion */
double ae_pad_notice_pulse(unsigned long elapsed_ms, int reduce_motion);

/* the details line: its parts (the screen draws a dot between them) and which part is the low battery (-1: none) */
enum { AE_PAD_DETAIL_PARTS = 4, AE_PAD_DETAIL_TEXT = 40 };
struct ae_pad_details
{
	char part[AE_PAD_DETAIL_PARTS][AE_PAD_DETAIL_TEXT];
	int count;
	int low;
};
/* world: ae_steam_pad_rules.h's AE_STEAM_WORLD_* of this player's pad; keyboard: the player plays with the keyboard and
mouse; name: the pad's short name when it is not a Steam Controller (DUALSENSE); mode: "WIRED", "BLUETOOTH" or "PUCK"
for a Steam Controller, "WIRED" or "WIRELESS" for another pad, NULL when not known; power, percent: as AE_PAD_POWER_* */
void ae_pad_details_build(struct ae_pad_details *details, int world, int keyboard, const char *name, const char *mode,
	int power, int percent);

/* the backpack note shows when L4, R4, L5 and R5 are all set (set[i] nonzero: not NONE) */
int ae_pad_backpack_note_shown(const int set[4]);
#define AE_PAD_BACKPACK_HEADING "BACKPACK MODE"
#define AE_PAD_BACKPACK_NOTE "Holding L4, R4, L5, R5 and the Steam button turns the controller off. Hold them again until it wakes, or plug it in by USB-C."

/* the words of SETTINGS' card and the notices */
#define AE_PAD_RAIL_FOOT_LABEL "YOUR CONTROLLER"
#define AE_PAD_NOTE_WORLD_STEAM_INPUT "Steam Input drives your controller: Steam's controller layout sets its trackpads, L4, R4, L5, R5 and gyro. To set them here, turn Steam Input off for this game: in Steam, the game's Properties > Controller, then Disable Steam Input."
#define AE_PAD_NOTE_WORLD_NO_ACCESS "Linux has not given the game access to your Steam Controller, so it acts as a mouse and keys. Install Steam's controller rules (the steam-devices package on most systems), then plug the controller in again."
#define AE_PAD_NOTICE_WORLD_NO_ACCESS_MENU "STEAM CONTROLLER IS ACTING AS A MOUSE: SEE SETTINGS > CONTROLS"
#define AE_PAD_NOTICE_WORLD_NO_ACCESS_SETTINGS "STEAM CONTROLLER IS ACTING AS A MOUSE"
#define AE_PAD_NOTE_NO_DUALSENSE "DUALSENSE shows while one is plugged in."

#endif
