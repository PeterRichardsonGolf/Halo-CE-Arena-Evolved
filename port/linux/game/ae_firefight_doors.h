/*
AE_FIREFIGHT_DOORS.H

The engine side of ae_firefight_doors_rules.h: Firefight's data build and
the skulls' network messages, asked at the place each runs.
*/

#ifndef AE_FIREFIGHT_DOORS_H
#define AE_FIREFIGHT_DOORS_H

/* the FIREFIGHT lobby is open (CREATE GAME in Firefight's mode: set as it opens, cleared as it closes): this host may start a
Firefight game, and its data is built */
void ae_firefight_doors_lobby_set(int open);
int ae_firefight_doors_lobby_open(void);
/* whether Firefight's data is to be built at the main menu: the FIREFIGHT lobby open, or game.firefight_pack on with DEV MODE on
(said once when the setting is on and neither holds) */
int ae_firefight_doors_pack_on(void);
/* the data's state now (ae_firefight_doors_rules.h's AE_FFD_DATA_*), from the tag pack; read at most once a second */
int ae_firefight_doors_data(void);
/* (DEV MODE `menu firefight data <state>`) the data's state held at one of AE_FFD_DATA_*, for a staged scene; -1 reads the tag pack again */
void ae_firefight_doors_data_force(int state);
/* whether this host sends the skulls' network messages: game.skulls on */
int ae_firefight_doors_skulls_send_on(void);
/* whether the skulls' data pack is written before the unlock question is answered */
int ae_firefight_doors_skulls_pack_early_on(void);

#endif
