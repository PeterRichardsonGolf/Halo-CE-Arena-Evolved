/*
AE_BOT_RECORD_DEMO.H

(0.9.4) DEV MODE's made-up bot records (notes/specs/2026-11-15-bot-service-records.md section 6: `botrecord demo`), so every
bot record screen can be staged in seconds without a game: a store filled from a seed with names from the bot name pool
(ae_social_names_rules.h), varied games, wins, kills, tiers, styles (each name's own, ae_bots_style_rules.h) and weapons, and
for one profile versus lines with a spread of leads, one TEAMMATE, one FIRST MEETING and one RIVAL. It is never written to
bot_records.txt (ae_glue_bot_records.c keeps it apart). Pure code: port/linux/tests/ae_bot_record_rules_test.c runs it.
*/

#ifndef __AE_BOT_RECORD_DEMO_H
#define __AE_BOT_RECORD_DEMO_H

#include "ae_bot_record_rules.h"

enum
{
	AE_BOT_RECORD_DEMO_BOTS = 24,
	AE_BOT_RECORD_DEMO_MOST = 60
};

/* the made-up store: bots names (1 .. AE_BOT_RECORD_DEMO_MOST; 0 for the default) from seed, versus lines for profile (none
   when it is NULL or ""). The bots made */
int ae_bot_records_demo(struct ae_bot_records *records, unsigned long seed, int bots, char const *profile);

/* `botrecord set`: one of a made-up bot's numbers (made when it has no record). field: "games", "wins", "ties", "kills",
   "deaths", "with" (games with profile), or "vs" (value = the profile's wins, value2 = the bot's: games against grows to
   hold them). 1, or 0 for a field it does not know, a negative value or no name */
int ae_bot_records_demo_set(struct ae_bot_records *records, char const *name, char const *profile, char const *field,
	long value, long value2);

#endif
