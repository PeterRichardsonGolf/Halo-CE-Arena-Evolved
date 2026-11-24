/*
AE_PAD_NOTICE.H

The controller notice (ae_pad_notice_rules.h): one slot for the menus and each local player's own in the game. A pad whose
battery is low says so once a drop, and a pad put on its charger after a low notice says that; a Steam Controller that
Linux has not let the game open says it acts as a mouse, once a session, in the menus. The menus' notice is drawn over every
screen, under the crumb on the right; the game's under that player's shield meter, in that player's view only.
*/

#ifndef AE_PAD_NOTICE_H
#define AE_PAD_NOTICE_H

/* (ae_ui_draw's end, an AE hook line) the menus' notice: reads the pads, then draws the newest one over the screen */
void ae_pad_notice_menu_draw(void);
/* (hud.c's view loop, an AE hook line) one view's notice, from that player's pad */
void ae_pad_notice_hud_draw(short local_player_index);
/* the DEV `pad` command's helpers: the notices stay on screen while held (a staged scene is looked at, not waited for);
reset forgets every pad's state, the notices and the staging */
void ae_pad_notice_hold(int on);
void ae_pad_notice_reset(void);

#endif
