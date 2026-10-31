/*
AE_SCREEN_BROWSER.H

AE's SERVER BROWSER and LAN GAMES (ae_screen_browser.c; the 0.3.7 menus): the internet's games or this network's, one
joined and its lobby opened. Opened from AE's MULTIPLAYER.
*/

#ifndef __AE_SCREEN_BROWSER_H
#define __AE_SCREEN_BROWSER_H

enum { AE_BROWSER_INTERNET, AE_BROWSER_LAN };

/* whether this build has the kind (SERVER BROWSER needs the game browser: configure.py --game-browser) */
int ae_screen_browser_available(int kind);
/* opens the kind over the stack (the LAN searched, or the game list's listings followed, while it is open); 0 when
the build lacks it or the stack is full */
int ae_screen_browser_open(int kind);

#endif
