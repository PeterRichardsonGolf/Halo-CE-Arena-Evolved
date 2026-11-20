/*
AE_SCREEN_MOTD.H

Arena Evolved's MESSAGE OF THE DAY and WHAT'S NEW (menus phase 1, spec §5 and §7, mockup 02; ae_screen_motd.c). One
panel: a left well (the day, the month, the facts) and the message (dateline, headline, paragraphs, bullets) scrolling
in it. At launch (ae_screen_motd_launch_poll from the main menu) it opens first, after at most 4 s of the master's
answer, when ae_motd_rules.h says so: the master's message, else the saved one ("SAVED BRIEFING · OFFLINE"), else the
installed release's notes ("WHAT'S NEW · RELEASED <date>"). A Continue, Y What's New, B Close; both record what was seen
(<save root>'s private ae_motd.txt). By hand (the main menu's row) nothing is marked for the day.
WHAT'S NEW: the releases of the shipped CHANGELOG.md on the left (newest first), the picked one's notes on the right;
each opened release is remembered for the main menu's "N NEW".

Its log: "ae motd: open (update|message|by hand, fresh|saved|notes)", "ae motd: closed (continue|close|whats new|game)",
"ae whats new: open (<version>)", "ae whats new: release <version>", "ae whats new: closed".
*/

#ifndef __AE_SCREEN_MOTD_H
#define __AE_SCREEN_MOTD_H

/* once a frame while the main menu is up: the launch's fetch started, then the MOTD opened first when due */
void ae_screen_motd_launch_poll(void);
int ae_screen_motd_open(int by_hand);
int ae_screen_whats_new_open(const char *version);
void ae_screen_motd_close(void);
/* the main menu's row values: "NEW" (an unread message) or NULL; "N NEW" for WHAT'S NEW or NULL */
const char *ae_screen_motd_badge(void);
const char *ae_screen_whats_new_badge(void);
/* (DEV MODE) motd_reset: what was seen forgotten; motd_offline: no master asked */
void ae_screen_motd_reset(void);
void ae_screen_motd_offline(void);

#endif
