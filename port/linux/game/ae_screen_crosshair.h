/*
AE_SCREEN_CROSSHAIR.H

(0.7.7) AE's CROSSHAIR screen (ae_screen_crosshair.c): the crosshair editor of HUD > RETICLE, a screen of its own (the
owner's pick, layout C "workbench"). The player's saved crosshairs in a strip at the top with NEW and the actions (RENAME,
DUPLICATE, SHARE CODE, DELETE); the edited one on a stage (1:1, a 4x magnifier, X: the next background) over its six state
tiles (NORMAL, ENEMY, ALLY, AT REST, MOVING, FIRING); a panel of tabs on the right, LB / RB: SHAPE (the 37 shapes in
place: the focused one shows on the stage, A picks it, B keeps the one picked), COLOR (the palette, CUSTOM COLOR typed or
stepped, OPACITY), LINES (inner and outer side by side), FRAME (frame and outline side by side), MOTION. Y opens SHARE CODE
from anywhere. Every change is saved at once (the game draws it on its next frame); B with changes asks KEEP / UNDO, and
UNDO puts every crosshair, the main one and the weapons' table back as the screen opened. The screen is the opening
player's (a split-screen player's own crosshairs).

Its log: "ae crosshair screen: open player <n> slot <s>", "tab <TAB>", "focus <what>", "set <key> <value>",
"shape <NAME>", "color <NAME>", "custom color <hex>", "named '<name>'", "edit slot <s>", "new slot <s>", "duplicate slot <s>",
"deleted", "preview background <NAME>", "keep", "undo", "closed (<changed|no changes>)".
*/

#ifndef __AE_SCREEN_CROSSHAIR_H
#define __AE_SCREEN_CROSSHAIR_H

/* opens it over the stack for a local player (0 to 3), editing their main crosshair (else their first saved one, else a new
one); 0 when the stack is full */
int ae_screen_crosshair_open(short player);

#endif
