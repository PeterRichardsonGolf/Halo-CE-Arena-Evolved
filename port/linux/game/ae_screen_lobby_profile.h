/* ae_screen_lobby_profile.h: the profile chooser shown before a player joins AE's lobby. */

#ifndef __AE_SCREEN_LOBBY_PROFILE_H
#define __AE_SCREEN_LOBBY_PROFILE_H

#include "ae_style.h"
#include "ae_ui.h"

struct ae_flow_screen;

typedef void (*ae_lobby_profile_done)(short local_player, int picked, void *context);

/* Opens a list of profiles unused by the other local players, plus GUEST (an inline card: NEW PROFILE too, named on
AE's keyboard, owned by the pad). The current automatic choice is focused, so
A keeps the old behaviour. picked is zero when B closes the chooser. Player 1 (a browser or invite join: no PLAYERS
panel) gets a small owner-only popover over the screen; players 2-4 get an INLINE CARD in the PLAYERS panel (the main
menu's, the lobby's roster), which the screen draws with ae_screen_lobby_profile_card_draw and feeds its pad's events
with ae_screen_lobby_profile_event. Several pads can choose at once, each its own card: a profile one picks is gone from
the others' lists at once. */
int ae_screen_lobby_profile_open(short local_player, struct ae_flow_screen const *flow,
	ae_lobby_profile_done done, void *context);

/* whether player 2-4's inline card is open (a popover is not: it is a screen of its own) */
int ae_screen_lobby_profile_choosing(short local_player);
/* how many inline cards are open */
short ae_screen_lobby_profile_choosing_count(void);
/* the height the card of player 2-4 takes now (0: none open) */
float ae_screen_lobby_profile_card_height(struct ae_density const *density, short local_player);
/* the card of player 2-4 at x, y, width wide (nothing drawn when none is open), hit_id its rows' hits; returns its
height, 0 for none */
float ae_screen_lobby_profile_card_draw(struct ae_density const *density, short local_player, float x, float y,
	float width, short hit_id);
/* a pad's event for its own card: up / down move, A picks (NEW PROFILE: opens the keyboard), B cancels (the others do
nothing); 1 when the pad has a card (the event is taken either way) */
int ae_screen_lobby_profile_event(short local_player, struct ae_event const *event);
/* the pointer on a row of a card (the hit index ae_screen_lobby_profile_card_draw recorded): hover focuses, a click
picks */
void ae_screen_lobby_profile_pointer(short hit_index, int hover, int click);
/* every inline card closes as cancelled (a screen is about to take the pad's place) */
void ae_screen_lobby_profile_cancel_all(void);
/* a button's hint as the cards show it, "(B) Leave": a round menu-blue chip with the letter knocked out, then the
label (a keyboard shows its key cap); left edge x, centred on center_y; returns its width (measure only when draw is 0) */
float ae_screen_lobby_profile_hint(struct ae_density const *density, int button, const char *label, float x,
	float center_y, int draw);

#endif
