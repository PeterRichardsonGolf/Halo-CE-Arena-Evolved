/*
AE_GLUE_BOT_RECORDS.H

(0.9.4) The bot service records' store and their facts from the game (ae_bot_record_rules.h; notes/specs/
2026-11-15-bot-service-records.md 2.3-2.4): bot_records.txt in the private folder beside career.txt (never touched),
read once a run, written once for each COUNTED AE TOURNAMENT game: ae_glue_social_game.c's result_count, when at least one
local player with a profile had the game saved to career.txt. A quit, a forgiven crash, a game that closed with no
post-game, a DEV made-up post-game or a non-tournament game never reaches it. When the post-game report's summaries are not
there yet, the game waits for them up to AE_BOT_RECORDS_WAIT_MS (as the service record does), then records what it has.
A file refused (edited, bad) is kept aside as bot_records.txt.bad and a fresh one started; a newer one is read and never
written. A failed write leaves the records in memory: the next game writes them.
*/

#ifndef __AE_GLUE_BOT_RECORDS_H
#define __AE_GLUE_BOT_RECORDS_H

#include "ae_bot_record_rules.h"

enum { AE_BOT_RECORDS_WAIT_MS = 4000 };

struct ae_social_plan;

/* the records (read at the first call of a run) */
struct ae_bot_records *ae_bot_records_store(void);
/* the records written: 1, else 0 (a newer file, or the write failed) */
int ae_bot_records_store_save(void);
/* the records the screens read: DEV MODE's made-up store while `botrecord demo` is on (never written), else the file's */
struct ae_bot_records *ae_bot_records_active(void);
/* whether a counted game waits for its summaries (its numbers are not in the records yet) */
int ae_bot_records_waiting(void);
/* forget what was read: the next ae_bot_records_store() reads bot_records.txt again (a profile's progress imported) */
void ae_bot_records_store_forget(void);
/* whether the made-up store is on */
int ae_bot_records_demo_on(void);
/* (0.9.4 fix, owner 11-16: after the TEST RUN's career-empty test the made-up store stayed on through a real game, and the
   post-game's bot pages read "0 games") the made-up store off, the screens on the file's records again: a TEST RUN test's
   setup undone (the next test, or leaving the run) and a real game counted call it; `why` goes to the log line */
void ae_bot_records_demo_end(char const *why);
/* DEV MODE's `botrecord` (spec section 6): demo [seed <n>] [bots <n>] | demo off | set <name> <field> <value> ... [player <n>]
   | forget | nothing (the store's state). The answer's text into text; 1 done, 0 refused */
struct ae_dev_cmd;
int ae_bot_records_dev(struct ae_dev_cmd const *cmd, char *text, int size);

/* DEV MODE's `social carnage <playlist> [seed <n>] [players <1-4>] [cursor <name>]` (spec section 6): a made-up
   tournament report (flagged demo: never recorded) of the playlist's shape, its other players pool bots from the seed
   (the demo store's first names for the same seed), on the reopened report's screen with the cursor on that bot (a name
   no row has takes the first bot's place). 1 opened */
int ae_bot_records_dev_carnage(char const *playlist, unsigned long seed, int players, char const *cursor, char *text,
	int size);

/* a counted tournament game of plan, at its post-game: profiles[local] the key of each local player whose result was saved
   (NULL or "" for none). Recorded now when the report's summaries are there, else when they come (ae_bot_records_tick) */
void ae_bot_records_game_counted(struct ae_social_plan const *plan, char const *const *profiles, int locals);
/* each frame of a tournament game: a game waiting for its summaries recorded when they come or the wait is over */
void ae_bot_records_tick(void);

#endif
