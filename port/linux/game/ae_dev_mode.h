/*
AE_DEV_MODE.H

DEV MODE: this PC's switch for the tools made for testing AE (the dev
camera first; the test checklist and dev hit markers later). It is
system.dev_mode, off by default, and never on for a client in another
machine's network game, whose rules are the host's.
*/

#ifndef AE_DEV_MODE_H
#define AE_DEV_MODE_H

/* whether DEV MODE is on for this machine now: the setting on, and this machine not a network game's client */
int ae_dev_mode_on(void);
/* the setting alone (system.dev_mode), for menus that show it */
int ae_dev_mode_setting(void);

#endif
