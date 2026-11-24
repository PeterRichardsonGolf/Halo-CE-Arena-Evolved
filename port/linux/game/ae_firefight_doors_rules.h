/*
AE_FIREFIGHT_DOORS_RULES.H

Which test-only modes may start in this build, as pure rules (no engine or
config calls: tests/ae_firefight_doors_rules_test.c runs them). The Firefight
engine and the skulls' network messages are DEV MODE's: a host starts them
only with DEV MODE on, a client plays the host's game. The engine side is
ae_firefight_doors.c's.
*/

#ifndef AE_FIREFIGHT_DOORS_RULES_H
#define AE_FIREFIGHT_DOORS_RULES_H

/* whether this machine may start a Firefight game: DEV MODE on, or a client (the host's game is the host's call) */
int ae_firefight_doors_engine_allowed(int dev_mode_setting, int network_client);
/* the engine index a variant keeps: Firefight's, or fallback when it is refused */
int ae_firefight_doors_engine_index(int engine_index, int firefight_index, int fallback, int allowed);
/* whether Firefight's data is built at the main menu: its setting on and DEV MODE on */
int ae_firefight_doors_pack_build(int pack_setting, int dev_mode_setting);
/* whether the host sends the skulls' network messages: only with the skulls setting on */
int ae_firefight_doors_skulls_send(int skulls_setting);
/* whether the skulls' data pack is written before the unlock question is answered: the skulls on, or DEV MODE on */
int ae_firefight_doors_skulls_pack_early(int skulls_setting, int dev_mode_setting);

#endif
