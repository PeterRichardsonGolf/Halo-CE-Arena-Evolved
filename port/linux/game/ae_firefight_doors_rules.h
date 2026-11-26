/*
AE_FIREFIGHT_DOORS_RULES.H

Which modes may start in this build, as pure rules (no engine or config calls:
tests/ae_firefight_doors_rules_test.c runs them). The Firefight engine starts
on a host only from the FIREFIGHT lobby (open while the game it started is
played) or with DEV MODE on; a client plays the host's game. The skulls'
network messages follow the skulls setting. Firefight's data (built from the
campaign maps) is what the FIREFIGHT row of the main menu waits on. The
engine side is ae_firefight_doors.c's.
*/

#ifndef AE_FIREFIGHT_DOORS_RULES_H
#define AE_FIREFIGHT_DOORS_RULES_H

/* whether this machine may start a Firefight game: the FIREFIGHT lobby open, DEV MODE on, or a client (the host's game is the
host's call) */
int ae_firefight_doors_engine_allowed(int dev_mode_setting, int network_client, int lobby_open);
/* the engine index a variant keeps: Firefight's, or fallback when it is refused */
int ae_firefight_doors_engine_index(int engine_index, int firefight_index, int fallback, int allowed);
/* whether Firefight's data is built at the main menu: the FIREFIGHT lobby open, or its setting and DEV MODE on */
int ae_firefight_doors_pack_build(int pack_setting, int dev_mode_setting, int lobby_open);

/* the state of Firefight's data, from what the tag pack says: maps_present how many of the campaign maps it is built from this PC
has, ready the pack is there and current, writing it is being written, failed it was not written */
enum { AE_FFD_DATA_READY, AE_FFD_DATA_PREPARING, AE_FFD_DATA_NEEDS_MAPS, AE_FFD_DATA_FAILED };
int ae_firefight_doors_data_state(int maps_present, int ready, int writing, int failed);
/* the words a waiting row shows ("" for READY) and the sentence for its help */
const char *ae_firefight_doors_data_reason(int state);
const char *ae_firefight_doors_data_help(int state);
/* whether START may run now: the data is there */
int ae_firefight_doors_data_startable(int state);
/* whether the host sends the skulls' network messages: only with the skulls setting on */
int ae_firefight_doors_skulls_send(int skulls_setting);
/* whether the skulls' data pack is written before the unlock question is answered: the skulls on, or DEV MODE on */
int ae_firefight_doors_skulls_pack_early(int skulls_setting, int dev_mode_setting);

#endif
