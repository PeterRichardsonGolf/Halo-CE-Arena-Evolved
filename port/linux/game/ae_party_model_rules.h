/*
AE_PARTY_MODEL_RULES.H

The party record the AE TOURNAMENT / MATCHMAKING screens draw: one record for a member whether it is a local seat, a bot or a friend on
another PC, and the rules around it. Pure code (no engine headers): port/linux/tests/ae_party_model_rules_test.c runs it.

- RECORD: id, name, kind (local / bot / friend), seats (split screens on its machine), leader, state, tier (0 blank), saved, ping, the
  invite's seconds left and the record with the player (games and wins together and against).
- SOURCE: offline (bots fill, bots may be party members) or online (people only: a bot member waits, tagged OFFLINE ONLY).
- PARTY RULE of a playlist (`# party_rule:` in social.txt): team (up to a team), solo (one), any (up to the game or a given number),
  range lo-hi (up to hi). A missing line derives from the playlist's `# party:` line.
- TEAMS: every party member is on the leader's team; the other seats are balanced around them.
*/

#ifndef __AE_PARTY_MODEL_RULES_H
#define __AE_PARTY_MODEL_RULES_H

enum
{
	AE_PARTY_MAX = 8,
	AE_PARTY_NAME = 24,
	AE_PARTY_ID = 24
};

enum ae_party_kind { AE_MEMBER_LOCAL, AE_MEMBER_BOT, AE_MEMBER_FRIEND };
enum ae_party_state { AE_PARTY_IN_LOBBY, AE_PARTY_INVITED, AE_PARTY_READY, AE_PARTY_SEARCHING, AE_PARTY_IN_GAME, AE_PARTY_OFFLINE };
enum ae_party_source { AE_PARTY_SOURCE_OFFLINE, AE_PARTY_SOURCE_ONLINE };

/* the tag a row shows besides its name (never colour alone) */
enum ae_party_tag
{
	AE_TAG_NONE, AE_TAG_YOU, AE_TAG_LEADER, AE_TAG_PARTY, AE_TAG_FAVOURITE, AE_TAG_READY, AE_TAG_PENDING, AE_TAG_OFFLINE_ONLY,
	AE_TAG_BOT, AE_TAG_NEW, AE_TAG_STAYING
};

struct ae_party_member
{
	char id[AE_PARTY_ID];
	char name[AE_PARTY_NAME];
	int kind, seats, leader, state;
	int tier;                   /* the bot's tier 1 .. 19, a person's level for the playlist; 0 blank */
	int saved;                  /* favourite (a bot) or friend (a person) */
	int ping_ms;                /* -1: none */
	int expires_s;              /* an invite's seconds left; 0 none */
	int together_games, together_wins, against_games, against_wins;
};

struct ae_party
{
	int count;
	struct ae_party_member member[AE_PARTY_MAX];
};

void ae_party_clear(struct ae_party *party);
/* a member at the end: its index, -1 for a full party or an id already there; the first member added is the leader */
int ae_party_add(struct ae_party *party, struct ae_party_member const *member);
/* the member with this id, -1 none */
int ae_party_find_id(struct ae_party const *party, char const *id);
/* takes a member out (never the leader): 1 when it went */
int ae_party_remove(struct ae_party *party, int index);
/* a member's name made into a record of a bot (id "bot:<name>"), seats 1, no ping */
void ae_party_bot(struct ae_party_member *member, char const *name, int tier);

/* whether a member plays in a search of this source: offline everyone but a friend; online only people, and not one that is still
   invited or offline */
int ae_party_takes_part(struct ae_party_member const *member, int source);
/* the seats a search takes: the members taking part, their seats added */
int ae_party_search_seats(struct ae_party const *party, int source);
/* the tag a row of the member wears in a source; leader is the leader row's */
int ae_party_tag(struct ae_party_member const *member, int source);
/* the tag's word ("LEADER", "OFFLINE ONLY"); "" for none */
char const *ae_party_tag_word(int tag);
/* the state's word ("in lobby", "invited", ...) */
char const *ae_party_state_word(int state);

/* the party rule of a playlist */
enum ae_party_rule_kind { AE_PARTY_RULE_TEAM, AE_PARTY_RULE_SOLO, AE_PARTY_RULE_ANY, AE_PARTY_RULE_RANGE };
struct ae_party_rule { int kind, low, high; };
/* "team", "solo", "any", "any 4", "range 2-3" (any case); 1 when it read, else rule is left as it was */
int ae_party_rule_parse(char const *text, struct ae_party_rule *rule);
/* a playlist without the line: a team game's rule is team, a free for all's solo */
void ae_party_rule_derive(int teams, struct ae_party_rule *rule);
/* the most seats a party may take in a playlist of teams (0: a free for all) of size players, the cap AE_PARTY_MAX over it */
int ae_party_rule_limit(struct ae_party_rule const *rule, int teams, int size);
/* the members (indexes) that would sit out for a limit: the last joined first, never the leader. Returns how many (up to max) */
int ae_party_sit_out(struct ae_party const *party, int source, int limit, int *out, int max);

/* every party seat of a roster on the leader's team: team[] of count seats, party[i] 1 for a party member's seat (the leader's own
   seats too); a party seat on another team swaps teams with a seat that is not a party's, nearest first. Returns the swaps made */
int ae_party_keep_on_team(int *team, int const *party, int count, int leader_team);

/* a bot member's tier after games shared: one step toward the player's tier for every 5 games (0 for no change) */
int ae_party_tier_drift(int tier, int player_tier, int shared_games);

/* the first character of a name for an emblem, upper-cased (a whole UTF-8 character: Latin, Greek, Cyrillic letters change case);
   "?" for an empty name. out holds at most 5 bytes */
void ae_party_initial(char const *name, char *out, int size);

#endif
