/*
AE_SCREEN_PARTY_PANEL.H

MY PARTY in AE TOURNAMENT's right column, drawn from the party record (ae_party_model_rules.h) so one panel shows an offline party
(bots) and an online one (friends, invites, ping, bots waiting): the leader's row, the members, an empty-seat line, RECENT PLAYERS with
PARTY UP, SEARCH FOR, and the state block. The tag chips (LEADER, PARTY, FAVOURITE, READY, PENDING, OFFLINE ONLY, BOT, NEW, STAYING,
YOU) are shared with the lobby's rows. The panel replaces the controller-seats panel while a party has bots in it or DEV MODE staged
one (`social party ...`).
*/

#ifndef __AE_SCREEN_PARTY_PANEL_H
#define __AE_SCREEN_PARTY_PANEL_H

#include "ae_party_model_rules.h"
#include "ae_party_store_rules.h"

struct ae_flow_screen;

/* a tag chip (ae_party_tag): left edge x or right edge x (the chip's width is returned), centred on mid_y. Filled tags are the
   ice-blue / white ones; the others are outlined; on_light: the row under it is the light focus bar, so an outlined tag is drawn dark */
float ae_party_tag_draw_left(struct ae_flow_screen *flow, float x, float mid_y, int tag, int on_light);
float ae_party_tag_draw_right(struct ae_flow_screen *flow, float right_x, float mid_y, int tag, int on_light);
/* a tag chip's width */
float ae_party_tag_width(struct ae_flow_screen *flow, int tag);

/* whether the party has more than this PC's players (bots in it) or DEV MODE staged one: MY PARTY then shows its members too */
int ae_party_panel_wanted(void);
/* the most seats the playlist AE TOURNAMENT is on lets a party take (the PARTY manager reads it; 4 until a playlist says); set each frame by the screen */
void ae_party_panel_set_limit(int limit);
int ae_party_panel_limit(void);
/* the party's members that are not this PC's players (bots, friends): up to max, in the order they joined; the count. The source (AE_PARTY_SOURCE_*) */
int ae_party_panel_others(struct ae_party_member *out, int max);
int ae_party_panel_source(void);

/* the rows, in the PC menus' own row kinds (ae_widget_party_row): a member's (a bot's 36 u row, a person's 50 u row) with its tags and detail, and a
   recent player's with PARTY UP; each returns its height and the gap under it. sits: the member sits out for the playlist's limit. focused: the
   white focus bar. The hits: hit_id, AE_PART_CARD, index */
float ae_party_panel_member_row(struct ae_flow_screen *flow, float x, float y, float width, struct ae_party_member const *m, int source,
	int sits, int focused, short hit_id, short index);
float ae_party_panel_recent_row(struct ae_flow_screen *flow, float x, float y, float width, struct ae_party_record const *r, int focused,
	short hit_id, short index);
/* the state block: "SEARCHING AS A PARTY" and its words (or SEARCH FOR online, or A PARTY OF n AT MOST); its height in u */
void ae_party_panel_state(struct ae_flow_screen *flow, float x, float y, float width, int limit);
int ae_party_panel_state_height(struct ae_flow_screen *flow);
/* under MY PARTY of the screen's own (ae_screen_main_menu_party_draw, which ended at y): the party's other members and the state block, no second panel */
void ae_party_panel_draw_below(struct ae_flow_screen *flow, float y, int limit);

#endif
