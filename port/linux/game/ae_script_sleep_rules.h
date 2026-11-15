/*
AE_SCRIPT_SLEEP_RULES.H

Which scenario scripts the host has put to sleep forever (an MCC map's named sleep_forever), as a bit vector by script
index. The host sends it to clients so a client's copy of a script the host stopped is stopped as well (a map's
startup check reads units, which are the host's, so a client's own check can take the other branch). Pure: no game,
no sockets.
*/

#ifndef AE_SCRIPT_SLEEP_RULES_H
#define AE_SCRIPT_SLEEP_RULES_H

enum
{
	AE_SLEEP_SCRIPTS = 480, /* script indices 0..479 are carried; a higher one is not */
	AE_SLEEP_BYTES = AE_SLEEP_SCRIPTS / 8
};

struct ae_script_sleeps
{
	unsigned char bits[AE_SLEEP_BYTES];
};

void ae_sleeps_clear(struct ae_script_sleeps *sleeps);
/* sets a script's bit: 1 if it was newly set, 0 if it was set already or is out of range */
int ae_sleeps_mark(struct ae_script_sleeps *sleeps, long script);
int ae_sleeps_test(struct ae_script_sleeps const *sleeps, long script);
int ae_sleeps_any(struct ae_script_sleeps const *sleeps);
/* the first script at or after from that the host has and the client has not yet applied, or -1 */
long ae_sleeps_next_pending(struct ae_script_sleeps const *host, struct ae_script_sleeps const *applied, long from);
/* a received entry is for this map: its script count is the client's own */
int ae_sleeps_entry_valid(long entry_scripts, long local_scripts);

#endif
