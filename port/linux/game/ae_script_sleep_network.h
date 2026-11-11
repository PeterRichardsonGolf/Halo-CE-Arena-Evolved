#ifndef AE_SCRIPT_SLEEP_NETWORK_H
#define AE_SCRIPT_SLEEP_NETWORK_H
#include "cseries.h"
/* The scenario scripts the host put to sleep forever, sent to clients (ae_script_sleep_network.c). */
void ae_script_sleep_network_new_game(void);
void ae_script_sleep_network_note(short script);
void ae_script_sleep_network_host_tick(void);
word ae_script_sleep_network_entry_size(void);
void ae_script_sleep_network_receive(void const *entries, short count);
#endif
