/*
AE_TARGET_FRAMERATE_RULES.H

TARGET FRAMERATE (display.target_framerate for one player, display.split_target_framerate for two to four): the frame
rate dynamic resolution holds by lowering the 3D picture's resolution (ae_dynamic_resolution_rules.h). It caps no
frames: FRAME RATE LIMIT (display.max_fps) does that. Pure rules (port/linux/tests/ae_target_framerate_rules_test.c).

The registry: the rows' texts, the settings' words and the help, in the rows' order. The AE menus' rows
(ae_settings_rows.c), the settings' words (port/linux/src/port_config.c) and the classic menus' ARENA EVOLVED rows
(tools/port_settings_ae.py, which reads the #define lines below) all take them from here.
*/

#ifndef AE_TARGET_FRAMERATE_RULES_H
#define AE_TARGET_FRAMERATE_RULES_H

#define AE_TARGET_FRAMERATE_TEXTS "OFF", "60", "90", "120", "144", "DISPLAY"
#define AE_TARGET_FRAMERATE_WORDS "off", "60", "90", "120", "144", "display"
#define AE_TARGET_FRAMERATE_COUNT 6
#define AE_TARGET_FRAMERATE_HELP "Lowers the 3D picture's resolution while the game runs slower than this (DISPLAY: " \
	"your screen's refresh rate), and never limits your frame rate. FRAME RATE LIMIT rises to match."
#define AE_TARGET_FRAMERATE_SPLIT_HELP "With split screen: lowers the 3D picture's resolution while the game runs slower " \
	"than this, and never limits your frame rate. FRAME RATE LIMIT rises to match."
/* (FRAME RATE LIMIT's, the one row each menu has: it lowers the targets, ae_target_limit_follow) */
#define AE_TARGET_FRAMERATE_LIMIT_HELP "With V-SYNC off, the most frames a second. AUTO is twice your screen's refresh " \
	"rate. TARGET FRAMERATE comes down to fit under it."
/* (the classic menus' three lines of help, as narrow as their other rows') */
#define AE_TARGET_FRAMERATE_CLASSIC_HELP "Lowers the 3D resolution when the game runs\n" \
	"slower than this, and never limits frame rate.\n" \
	"FRAME RATE LIMIT rises to match."
#define AE_TARGET_FRAMERATE_CLASSIC_SPLIT_HELP "With split screen: lowers the 3D resolution when\n" \
	"slower than this, and never limits frame rate.\n" \
	"FRAME RATE LIMIT rises to match."
#define AE_TARGET_FRAMERATE_CLASSIC_LIMIT_HELP "With V-Sync off, the most frames a second. Auto:\n" \
	"twice the display's refresh rate.\n" \
	"TARGET FRAMERATE comes down to fit under it."

/* the defaults (one player: off; split screen: 60, as the earlier display.split_target_60_fps's true) */
#define AE_TARGET_FRAMERATE_DEFAULT "off"
#define AE_TARGET_FRAMERATE_SPLIT_DEFAULT "60"

/* a choice: 0 off, the frames a second, or AE_TARGET_FRAMERATE_DISPLAY */
#define AE_TARGET_FRAMERATE_DISPLAY (-1)
/* the display's refresh rate when it is not known */
#define AE_TARGET_FRAMERATE_UNKNOWN_HZ 60.0f

/* the choice a word names (any case; NULL, empty or another word: off) */
int ae_target_framerate_choice(const char *word);

/* the word an earlier version's display.target_60_fps or display.split_target_60_fps carries over as: "60" for true
(it held 60 frames a second, or the refresh of a display slower than that, with V-SYNC on or off), "off" for false */
const char *ae_target_framerate_carry_over(int earlier_on);
/* (the same as a macro, for port_config.c, which links no game file) */
#define AE_TARGET_FRAMERATE_CARRY_OVER(earlier_on) ((earlier_on) ? "60" : "off")

/* the frames a second held for a choice (0: off): DISPLAY is the display's refresh rate (refresh_hz; 0 or below when
not known: AE_TARGET_FRAMERATE_UNKNOWN_HZ), and no target is above what the frames can reach: the refresh rate with
V-SYNC on (vsync), FRAME RATE LIMIT's cap with it off (max_fps: above 0 that many, 0 twice the refresh rate, below 0
none) */
float ae_target_framerate_hz(int choice, int vsync, float refresh_hz, long max_fps);

/* TARGET FRAMERATE and FRAME RATE LIMIT adjust together (V-SYNC off only: with it on the limit holds no frames): the
limit never stays below the higher of the two targets (DISPLAY: the refresh rate; the limit's 0, AUTO: twice the
refresh rate; -1, NONE: above every target, so it never moves). A target set above the limit raises the limit to it,
keeping the limit the player set (own); a target set lower, or OFF, takes the limit back down toward that (no lower
than the targets still need). A limit set below a target steps the target down to its highest value at or under the
limit (under 60: off); a limit the player sets is their own (own forgotten). V-SYNC turned off applies the raise once. */
#define AE_TARGET_LIMIT_NO_OWN (-2L)
#define AE_TARGET_LIMIT_OWN_KEY "display.max_fps_own"
enum ae_target_limit_change
{
	AE_TARGET_LIMIT_TARGET,
	AE_TARGET_LIMIT_SPLIT_TARGET,
	AE_TARGET_LIMIT_LIMIT,
	AE_TARGET_LIMIT_VSYNC
};
struct ae_target_limit
{
	/* the two targets as choices (ae_target_framerate_choice) */
	int target;
	int split_target;
	/* display.max_fps: above 0 that many, 0 AUTO, -1 NONE */
	long max_fps;
	/* the limit the player set while a target holds it higher (AE_TARGET_LIMIT_NO_OWN: none) */
	long own;
};
/* after the setting `change` names was set to its value in `state`: the others moved to follow; returns 1 when any of
`state` changed */
int ae_target_limit_follow(enum ae_target_limit_change change, int vsync, float refresh_hz,
	struct ae_target_limit *state);
/* a choice as its word ("off", "60" .. "144", "display") */
const char *ae_target_framerate_word(int choice);

#endif
