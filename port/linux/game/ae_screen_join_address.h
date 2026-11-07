/*
AE_SCREEN_JOIN_ADDRESS.H

AE's JOIN BY ADDRESS (ae_screen_join_address.c; 0.7.0): a host's address typed or pasted (an IP address, a computer's
name or a private network's name, with :port after it when the host uses another port) or picked from RECENT (the
hosts joined this way), joined, then the joiner's LOBBY (ae_screen_create_game.h). Opened from JOIN LAN GAMES' Y and
SERVER BROWSER's Y (ae_screen_browser.c).
*/

#ifndef __AE_SCREEN_JOIN_ADDRESS_H
#define __AE_SCREEN_JOIN_ADDRESS_H

/* opens it over the stack; crumb: where it was opened from ("JOIN LAN GAMES"). Leaving the joined game's lobby comes
back here; 0 when the stack is full */
int ae_screen_join_address_open(const char *crumb);

#endif
