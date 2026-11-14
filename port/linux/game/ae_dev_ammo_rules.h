/*
AE_DEV_AMMO_RULES.H

DEV MODE's INFINITE AMMO and BOTTOMLESS CLIP (ae_dev_ammo.h), as a pure rule (no engine includes:
port/linux/tests/ae_dev_ammo_rules_test.c compiles it alone). The settings system.dev_infinite_ammo and
system.dev_bottomless_clip count only while DEV MODE is on (never a network game's client), and only for a weapon a
player holds who is not a bot: the bots and the dummies fire and reload as they always do.
*/

#ifndef __AE_DEV_AMMO_RULES_H
#define __AE_DEV_AMMO_RULES_H

/* whether the setting holds for a weapon: DEV MODE on, the setting on, the weapon held by a player, that player no bot */
int ae_dev_ammo_applies(int dev_mode_on, int setting_on, int held_by_player, int held_by_bot);

#endif
