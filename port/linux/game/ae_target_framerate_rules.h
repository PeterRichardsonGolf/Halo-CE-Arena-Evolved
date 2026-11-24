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
	"your screen's refresh rate), and never limits your frame rate."
#define AE_TARGET_FRAMERATE_SPLIT_HELP "With split screen: lowers the 3D picture's resolution while the game runs slower " \
	"than this, and never limits your frame rate."
/* (the classic menus' two lines of help, as narrow as their other rows') */
#define AE_TARGET_FRAMERATE_CLASSIC_HELP "Lowers the 3D resolution when the game runs\n" \
	"slower than this, and never limits frame rate."
#define AE_TARGET_FRAMERATE_CLASSIC_SPLIT_HELP "With split screen: lowers the 3D resolution when\n" \
	"slower than this, and never limits frame rate."

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

#endif
