/*
AE_PLAYLIST_RULES.H

Pure rules of a dedicated server's playlist (port/linux/tests/ae_playlist_rules_test.c): the weights of its rotation
entries (+weight=1..5), the last entries played that its vote keeps out, the vote's nominations, rock the vote, the
one extension of a game, and who can play an entry (its audience: ANYONE, AE, AE + MCC) against who is on the server.
No engine calls; server_director.c reaches them through ae_server_playlist.c.
*/

#ifndef AE_PLAYLIST_RULES_H
#define AE_PLAYLIST_RULES_H

enum
{
	AE_PL_WEIGHT_MAXIMUM = 5,
	AE_PL_ENTRIES = 64,
	AE_PL_HISTORY_MAXIMUM = 8,
	AE_PL_PEOPLE = 32,
	AE_PL_TEXT = 256
};

/* who can play an entry */
enum
{
	AE_AUDIENCE_ANYONE,	/* any Halo CE PC build */
	AE_AUDIENCE_AE,		/* Arena Evolved */
	AE_AUDIENCE_AE_MCC,	/* Arena Evolved with MCC's files */
	AE_AUDIENCE_COUNT
};

/* a rotation entry's weight as written (0: not given) as the one used: 1 to 5, 1 when not given or out of range */
int ae_pl_weight(
	int written);

/* a weight text ("3"): 1 and the weight if it is a whole number 1 to 5 */
int ae_pl_weight_parse(
	char const *text,
	int *weight);

/* up to wanted distinct eligible entries (eligible[i] nonzero), each draw weighted by the entries' weights
(weights may be NULL: all 1), without replacement; state is a xorshift state. out gets the entries; returns
how many (fewer than wanted when fewer are eligible) */
int ae_pl_sample(
	int const *weights,
	unsigned char const *eligible,
	int count,
	int wanted,
	unsigned long *state,
	int *out);

/* the last entries played (their texts), newest last, at most AE_PL_HISTORY_MAXIMUM */
struct ae_pl_history
{
	char text[AE_PL_HISTORY_MAXIMUM][AE_PL_TEXT];
	int count;
};

void ae_pl_history_push(
	struct ae_pl_history *history,
	char const *text);

/* whether text is among the last depth entries played (depth 0: nothing is) */
int ae_pl_history_recent(
	struct ae_pl_history const *history,
	char const *text,
	int depth);

/* the entry a "!nominate" text means among the names (entry i's map name and its text): the first whose map name equals
it, else the only map whose name or text holds it (any case). Returns its index, -1 for none, -2 for several maps */
int ae_pl_match_entry(
	char const *const *map_names,
	char const *const *texts,
	int count,
	char const *query);

/* the people a vote counts: who nominated what (one each), who asked to rock the vote */
struct ae_pl_people
{
	long id[AE_PL_PEOPLE];
	int nominated[AE_PL_PEOPLE];
	unsigned char rtv[AE_PL_PEOPLE];
	int count;
};

void ae_pl_people_reset(
	struct ae_pl_people *people);

/* a person's nomination (entry >= 0; replaces theirs): 1, 0 if there is no room for another person */
int ae_pl_nominate(
	struct ae_pl_people *people,
	long id,
	int entry);

/* the distinct entries nominated, in the order first nominated: how many; out holds at least AE_PL_PEOPLE */
int ae_pl_nominated(
	struct ae_pl_people const *people,
	int *out);

/* a person asks to rock the vote: the asks so far; -1 if there was no room */
int ae_pl_rtv_ask(
	struct ae_pl_people *people,
	long id);

/* the asks of the people who are still here (present: the humans' ids; bots never are in it): 1 when more than half of
them asked. Zero humans: 0 */
int ae_pl_rtv_passed(
	struct ae_pl_people const *people,
	long const *present,
	int present_count);

/* the asks a majority of humans needs */
int ae_pl_rtv_needed(
	int humans);

/* the one extension of a game: 1 if one more may be given (used is how many were), the minutes bounded 1 to 60 */
int ae_pl_extend_allowed(
	int used);
int ae_pl_extend_minutes(
	int setting);

/* an entry's audience from what it needs: needs_ae (an AE gametype or an AE-only feature), needs_mcc (a map of MCC's,
<name>@mcc or @ws<id>) */
int ae_pl_audience(
	int needs_ae,
	int needs_mcc);

/* the words of an audience in a listing ("all", "ae", "ae+mcc") and in a text */
char const *ae_pl_audience_word(
	int audience);
char const *ae_pl_audience_name(
	int audience);

/* whether a person can play an entry of that audience. ae: the person's machine is Arena Evolved; mcc: 1 has MCC's
files, 0 has not, -1 not known (counted as having them) */
int ae_pl_can_play(
	int audience,
	int ae,
	int mcc);

/* the first entry at or after start (wrapping) that can be played at all (playable[i]) and that every present person can
play, or -1 if none. audiences[i] is entry i's; has_non_ae: someone present is not Arena Evolved; has_no_mcc: someone
present is known not to have MCC's files */
int ae_pl_next_playable(
	int const *audiences,
	unsigned char const *playable,
	int count,
	int start,
	int has_non_ae,
	int has_no_mcc);

/* the audiences among a playlist's playable entries, one bit each (1 << audience); whether that is more than one kind */
int ae_pl_range_mask(
	int const *audiences,
	unsigned char const *playable,
	int count);
int ae_pl_range_mixed(
	int mask);

/* the line for a person who cannot play an entry of that audience */
void ae_pl_refusal_text(
	int audience,
	char *text,
	int size);

/* ---------- the map check (messages 111 and 112): which maps a machine holds */

enum
{
	AE_PL_MAPS = 64,		/* distinct @mcc / @ws maps a playlist asks about */
	AE_PL_MAP_NAME = 64,	/* an identity: the loader's level names hold 63 */
	AE_PL_ASK_PER_MESSAGE = 6
};

/* the maps of a playlist that need a machine's MCC files (identities, each once, in the order first met) */
struct ae_pl_map_table
{
	char name[AE_PL_MAPS][AE_PL_MAP_NAME];
	int count;
};

/* the identity's slot (added when new); -1 when the table is full or the name is empty or too long */
int ae_pl_map_table_slot(
	struct ae_pl_map_table *table,
	char const *identity);

/* the identity's slot, -1 if it is not in the table (any case) */
int ae_pl_map_table_find(
	struct ae_pl_map_table const *table,
	char const *identity);

/* whether two tables hold the same names in the same order */
int ae_pl_map_table_same(
	struct ae_pl_map_table const *a,
	struct ae_pl_map_table const *b);

/* how many asking messages a table takes (AE_PL_ASK_PER_MESSAGE names each) */
int ae_pl_ask_messages(
	int count);

/* what a machine told (message 111): has_mcc and a bit for each table slot it holds */
struct ae_pl_holding
{
	int told;
	int has_mcc;
	int bits_told;	/* the map bits are known (message 111); a join request tells has_mcc alone */
	unsigned char bits[AE_PL_MAPS / 8];
};

void ae_pl_holding_reset(
	struct ae_pl_holding *holding);

/* an answer taken: bits past count are dropped */
void ae_pl_holding_set(
	struct ae_pl_holding *holding,
	int has_mcc,
	unsigned char const *bits,
	int count);

/* what a join request told (the MCC mark): has_mcc alone, no map bits yet (a machine with MCC's files is then counted as
holding every map, until its answer) */
void ae_pl_holding_set_join(
	struct ae_pl_holding *holding,
	int has_mcc);

/* the mcc argument of ae_pl_can_play for a map slot (-1: not a map that needs a check): 1 holds it, 0 does not,
-1 not known (the machine told nothing: counted as holding it) */
int ae_pl_holding_state(
	struct ae_pl_holding const *holding,
	int slot);

/* a client's bit for slot: set */
void ae_pl_bits_set(
	unsigned char *bits,
	int slot);

enum
{
	AE_MISSING_NOTHING,
	AE_MISSING_MCC,		/* no MCC files at all */
	AE_MISSING_MAP		/* MCC, but not this map (stock map absent, Workshop item not subscribed) */
};

/* what a machine that cannot play the map lacks */
int ae_pl_missing(
	struct ae_pl_holding const *holding,
	int slot);

/* the line for it: "This game needs MCC ...", "Subscribe to <name> (item <id>) in the Steam Workshop." */
void ae_pl_missing_text(
	int missing,
	char const *identity,
	char *text,
	int size);

#endif
