/*
AE_TEST_RUN_CARD.H
TEST RUN's card (DEV MODE), drawn with AE's renderer (ae_draw.h) in layout units over the whole frame: the owner-approved
mockups (test-run-mockups-v2): a dark panel with a cyan left bar at the top right under the shield bar (or a strip at the
bottom for a test with card_bottom), the header "TEST n / N · FEATURE", the title and up to 3 checks in sentence case, the
key hints, a REC dot on a video test; the FAIL / NOTE box under it (a keyboard field, or the pad's tag row); the end
summary. Nothing is drawn while no run is on or while the hide key is held. One card for the screen, not one per player
view.
*/
#ifndef AE_TEST_RUN_CARD_H
#define AE_TEST_RUN_CARD_H

/* once a frame after the game's drawing (main.c main_framerate_render): asks the renderer to draw the card at this
frame's Present, over the HUD and the AE screens */
void ae_test_run_card_draw(void);
/* (0.7.9 view bots) the card's box (layout units of the whole frame: x0, y0, x1, y1) as last drawn; 0 when none is up */
int ae_test_run_card_box(float box[4]);

#endif
