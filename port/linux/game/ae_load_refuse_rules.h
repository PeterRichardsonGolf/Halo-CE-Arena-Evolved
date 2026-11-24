#ifndef AE_LOAD_REFUSE_RULES_H
#define AE_LOAD_REFUSE_RULES_H

/*
AE_LOAD_REFUSE_RULES.H

The pure rules of a map that will not load on a dedicated server. A refused map (MCC resources that do not match,
a missing or broken map file) must never end the server: its rotation entry is left out for a while, the next entry
is played, and when every entry is out the server waits and tries again, saying so once a minute.

Also the words for an MCC resource file that fails its check: which file (bitmaps.map or sounds.map), its path and
size, the first reference it could not satisfy, and what to install.

No game, socket or file dependency: tools/test_ae_units.py compiles ae_load_refuse_rules_test.c on its own.
*/

enum
{
	AE_LR_ENTRIES = 256,         /* the server's rotation maximum (SERVER_ROTATION_MAXIMUM_ENTRIES) */
	AE_LR_FIRST_WAIT = 300,      /* seconds an entry stays out after its first refusal */
	AE_LR_MAX_WAIT = 3600,       /* the longest an entry stays out */
	AE_LR_LOG_SECONDS = 60       /* the wait for a retry is said this often */
};

struct ae_lr_state
{
	int failures[AE_LR_ENTRIES];        /* refusals in a row; 0 = never refused (or played since) */
	double retry_at[AE_LR_ENTRIES];     /* seconds (server uptime) when the entry may be tried again */
	double last_log;                    /* when the wait was last said (a negative number = never) */
};

void ae_lr_reset(struct ae_lr_state *state);
/* a refusal of the entry at index: how many seconds it stays out (300, 600, 1200 ... at most 3600); 0 for a bad index */
double ae_lr_note_failure(struct ae_lr_state *state, long index, double now);
/* the entry played: its refusals are forgotten */
void ae_lr_note_success(struct ae_lr_state *state, long index);
/* whether the entry is out because of a refusal and its wait is over (it may be tried again) */
int ae_lr_due(struct ae_lr_state const *state, long index, double now);
/* whether the entry is out because of a refusal (due or not) */
int ae_lr_refused(struct ae_lr_state const *state, long index);
/* the entry's retry begun: it is no longer counted as out, its failure count stays (the next refusal waits longer) */
void ae_lr_release(struct ae_lr_state *state, long index);
/* how many of the first count entries wait for a retry, and the seconds to the soonest (negative when none) */
long ae_lr_waiting(struct ae_lr_state const *state, long count, double now, double *soonest);
/* TRUE when the wait should be said now (once a minute); records the time */
int ae_lr_log_due(struct ae_lr_state *state, double now);

/* the file a resource type is: 1 bitmaps.map, 2 sounds.map, else "resource.map" */
char const *ae_lr_resource_name(unsigned type);
/* the line a failed resource check logs and shows: names the file, its leaf path and size, the first reference that
failed (reference may be NULL), how many failed (0 = not counted), and the Steam / package resource to install */
void ae_lr_resource_failure_text(char *out, unsigned long size, unsigned type, char const *path,
	unsigned long file_bytes, char const *reference, unsigned long failed_count, char const *level);

#endif
