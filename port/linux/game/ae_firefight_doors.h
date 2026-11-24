/*
AE_FIREFIGHT_DOORS.H

The engine side of ae_firefight_doors_rules.h: Firefight's data build and
the skulls' network messages, asked at the place each runs.
*/

#ifndef AE_FIREFIGHT_DOORS_H
#define AE_FIREFIGHT_DOORS_H

/* whether Firefight's data is to be built at the main menu: game.firefight_pack on and DEV MODE on (said once when
the setting is on and DEV MODE is off) */
int ae_firefight_doors_pack_on(void);
/* whether this host sends the skulls' network messages: game.skulls on */
int ae_firefight_doors_skulls_send_on(void);
/* whether the skulls' data pack is written before the unlock question is answered */
int ae_firefight_doors_skulls_pack_early_on(void);

#endif
