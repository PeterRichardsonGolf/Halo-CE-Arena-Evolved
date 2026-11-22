/*
AE_PARTY_MODEL_RULES_TEST.C

The party record and its rules (port/linux/game/ae_party_model_rules.c): members, the party rule of a playlist, who sits out, the
leader's team, the bot tier drift, the emblem initial.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_party_model_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, wanted %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static void person(struct ae_party_member *member, char const *id, char const *name, int kind)
{
	memset(member, 0, sizeof(*member));
	snprintf(member->id, sizeof(member->id), "%s", id);
	snprintf(member->name, sizeof(member->name), "%s", name);
	member->kind = kind;
	member->ping_ms = -1;
}

static void test_members(void)
{
	struct ae_party party;
	struct ae_party_member m;
	int at;

	ae_party_clear(&party);
	person(&m, "you", "PLAYER 1", AE_MEMBER_LOCAL);
	EQ(ae_party_add(&party, &m), 0);
	CHECK(party.member[0].leader && party.member[0].seats == 1);
	ae_party_bot(&m, "TheRhino", 7);
	EQ(ae_party_add(&party, &m), 1);
	CHECK(!party.member[1].leader && !strcmp(party.member[1].id, "bot:TheRhino") && party.member[1].tier == 7);
	/* (the same id twice: refused) */
	EQ(ae_party_add(&party, &m), -1);
	EQ(ae_party_find_id(&party, "bot:TheRhino"), 1);
	EQ(ae_party_find_id(&party, "nobody"), -1);
	/* (the leader never goes) */
	EQ(ae_party_remove(&party, 0), 0);
	EQ(ae_party_remove(&party, 1), 1);
	EQ(party.count, 1);
	EQ(ae_party_remove(&party, 5), 0);
	/* (full: AE_PARTY_MAX) */
	for (at = 0; at < 20; at++)
	{
		char name[16];

		snprintf(name, sizeof(name), "b%d", at);
		ae_party_bot(&m, name, 5);
		ae_party_add(&party, &m);
	}
	EQ(party.count, AE_PARTY_MAX);
}

static void test_source(void)
{
	struct ae_party party;
	struct ae_party_member m;

	ae_party_clear(&party);
	person(&m, "you", "PLAYER 1", AE_MEMBER_LOCAL);
	ae_party_add(&party, &m);
	ae_party_bot(&m, "TheRhino", 7);
	m.saved = 1;
	ae_party_add(&party, &m);
	person(&m, "K7QD-2MXA-9PWE", "Kestrel44", AE_MEMBER_FRIEND);
	m.state = AE_PARTY_READY;
	ae_party_add(&party, &m);
	person(&m, "H1", "Hollow_Fox", AE_MEMBER_FRIEND);
	m.state = AE_PARTY_INVITED;
	m.expires_s = 292;
	ae_party_add(&party, &m);
	/* offline: you, the bot, no friend */
	EQ(ae_party_search_seats(&party, AE_PARTY_SOURCE_OFFLINE), 2);
	/* online: you and the ready friend: the bot waits, the invited friend is not in yet */
	EQ(ae_party_search_seats(&party, AE_PARTY_SOURCE_ONLINE), 2);
	EQ(ae_party_tag(&party.member[0], AE_PARTY_SOURCE_ONLINE), AE_TAG_LEADER);
	EQ(ae_party_tag(&party.member[1], AE_PARTY_SOURCE_ONLINE), AE_TAG_OFFLINE_ONLY);
	EQ(ae_party_tag(&party.member[1], AE_PARTY_SOURCE_OFFLINE), AE_TAG_FAVOURITE);
	EQ(ae_party_tag(&party.member[2], AE_PARTY_SOURCE_ONLINE), AE_TAG_READY);
	EQ(ae_party_tag(&party.member[3], AE_PARTY_SOURCE_ONLINE), AE_TAG_PENDING);
	CHECK(!strcmp(ae_party_tag_word(AE_TAG_OFFLINE_ONLY), "OFFLINE ONLY") && !strcmp(ae_party_tag_word(99), ""));
	CHECK(!strcmp(ae_party_state_word(AE_PARTY_INVITED), "invited"));
	/* a friend with two split screens takes two seats */
	party.member[2].seats = 2;
	EQ(ae_party_search_seats(&party, AE_PARTY_SOURCE_ONLINE), 3);
}

static void test_rule(void)
{
	struct ae_party_rule rule = { 99, 0, 0 };

	CHECK(ae_party_rule_parse("team", &rule) && rule.kind == AE_PARTY_RULE_TEAM);
	CHECK(ae_party_rule_parse("  SOLO", &rule) && rule.kind == AE_PARTY_RULE_SOLO);
	CHECK(ae_party_rule_parse("any", &rule) && rule.kind == AE_PARTY_RULE_ANY && rule.high == 0);
	CHECK(ae_party_rule_parse("Any 4", &rule) && rule.kind == AE_PARTY_RULE_ANY && rule.high == 4);
	CHECK(ae_party_rule_parse("range 2-3", &rule) && rule.kind == AE_PARTY_RULE_RANGE && rule.low == 2 && rule.high == 3);
	rule.kind = AE_PARTY_RULE_SOLO;
	CHECK(!ae_party_rule_parse("range 3-2", &rule) && rule.kind == AE_PARTY_RULE_SOLO);
	CHECK(!ae_party_rule_parse("range 3", &rule));
	CHECK(!ae_party_rule_parse("pairs", &rule));
	CHECK(!ae_party_rule_parse("", &rule));
	ae_party_rule_derive(2, &rule);
	EQ(rule.kind, AE_PARTY_RULE_TEAM);
	EQ(ae_party_rule_limit(&rule, 2, 4), 4);
	EQ(ae_party_rule_limit(&rule, 2, 8), AE_PARTY_MAX);
	EQ(ae_party_rule_limit(&rule, 2, 2), 2);
	ae_party_rule_derive(0, &rule);
	EQ(rule.kind, AE_PARTY_RULE_SOLO);
	EQ(ae_party_rule_limit(&rule, 0, 8), 1);
	/* a free for all that lets a party of up to 4 in one game */
	ae_party_rule_parse("any 4", &rule);
	EQ(ae_party_rule_limit(&rule, 0, 8), 4);
	EQ(ae_party_rule_limit(&rule, 0, 3), 3);
	ae_party_rule_parse("any", &rule);
	EQ(ae_party_rule_limit(&rule, 0, 8), AE_PARTY_MAX);
	ae_party_rule_parse("range 2-3", &rule);
	EQ(ae_party_rule_limit(&rule, 2, 4), 3);
	EQ(ae_party_rule_limit(&rule, 2, 1), 2);
}

static void test_sit_out(void)
{
	struct ae_party party;
	struct ae_party_member m;
	int out[AE_PARTY_MAX], count;

	ae_party_clear(&party);
	person(&m, "you", "PLAYER 1", AE_MEMBER_LOCAL);
	ae_party_add(&party, &m);
	ae_party_bot(&m, "a", 5); ae_party_add(&party, &m);
	ae_party_bot(&m, "b", 5); ae_party_add(&party, &m);
	ae_party_bot(&m, "c", 5); ae_party_add(&party, &m);
	/* a playlist of 2: the last joined sit out first, never the leader */
	count = ae_party_sit_out(&party, AE_PARTY_SOURCE_OFFLINE, 2, out, AE_PARTY_MAX);
	EQ(count, 2);
	EQ(out[0], 3); EQ(out[1], 2);
	EQ(ae_party_sit_out(&party, AE_PARTY_SOURCE_OFFLINE, 4, out, AE_PARTY_MAX), 0);
	EQ(ae_party_sit_out(&party, AE_PARTY_SOURCE_OFFLINE, 1, out, AE_PARTY_MAX), 3);
	/* online the bots do not count: nobody sits out */
	EQ(ae_party_sit_out(&party, AE_PARTY_SOURCE_ONLINE, 1, out, AE_PARTY_MAX), 0);
}

static void test_team(void)
{
	/* seats: 0 you (team 0), 1 and 2 party bots on team 1, 3 .. 7 others */
	int team[8] = { 0, 1, 1, 0, 0, 0, 1, 1 };
	int party[8] = { 1, 1, 1, 0, 0, 0, 0, 0 };
	int swaps = ae_party_keep_on_team(team, party, 8, 0);
	int at, on_team = 0;

	EQ(swaps, 2);
	CHECK(team[0] == 0 && team[1] == 0 && team[2] == 0);
	for (at = 0; at < 8; at++)
		on_team += team[at] == 0;
	/* (the teams stay the size they were) */
	EQ(on_team, 4);
	/* nothing to swap with: left as it is */
	{
		int all_team[3] = { 0, 1, 1 }, all_party[3] = { 1, 1, 1 };

		EQ(ae_party_keep_on_team(all_team, all_party, 3, 0), 0);
	}
}

static void test_drift(void)
{
	EQ(ae_party_tier_drift(7, 12, 4), 0);
	EQ(ae_party_tier_drift(7, 12, 5), 1);
	EQ(ae_party_tier_drift(7, 12, 12), 2);
	EQ(ae_party_tier_drift(7, 8, 50), 1);
	EQ(ae_party_tier_drift(12, 7, 5), -1);
	EQ(ae_party_tier_drift(7, 7, 50), 0);
}

static void test_initial(void)
{
	char out[8];

	ae_party_initial("player 1", out, sizeof(out)); CHECK(!strcmp(out, "P"));
	ae_party_initial("TheRhino", out, sizeof(out)); CHECK(!strcmp(out, "T"));
	ae_party_initial("xMirax", out, sizeof(out)); CHECK(!strcmp(out, "X"));
	ae_party_initial("7even", out, sizeof(out)); CHECK(!strcmp(out, "7"));
	/* a whole UTF-8 character: \xC3\xA9 is e acute, upper-cased */
	ae_party_initial("\xC3\xA9lan", out, sizeof(out)); CHECK(!strcmp(out, "\xC3\x89"));
	ae_party_initial("\xD0\xB0\xD0\xBD\xD1\x8F", out, sizeof(out)); CHECK(!strcmp(out, "\xD0\x90"));
	ae_party_initial("\xCE\xB1\xCE\xB2", out, sizeof(out)); CHECK(!strcmp(out, "\xCE\x91"));
	/* three and four byte characters are taken whole and kept */
	ae_party_initial("\xE6\x97\xA5" "x", out, sizeof(out)); CHECK(!strcmp(out, "\xE6\x97\xA5"));
	ae_party_initial("\xF0\x9F\x98\x80" "x", out, sizeof(out)); CHECK(!strcmp(out, "\xF0\x9F\x98\x80"));
	/* an empty name, a stray byte, a cut character */
	ae_party_initial("", out, sizeof(out)); CHECK(!strcmp(out, "?"));
	ae_party_initial("\x80", out, sizeof(out)); CHECK(!strcmp(out, "?"));
	ae_party_initial("\xC3", out, sizeof(out)); CHECK(!strcmp(out, "?"));
	ae_party_initial(NULL, out, sizeof(out)); CHECK(!strcmp(out, "?"));
	/* a buffer too small for a character: empty */
	ae_party_initial("abc", out, 3); CHECK(!out[0]);
}

int main(void)
{
	test_members();
	test_source();
	test_rule();
	test_sit_out();
	test_team();
	test_drift();
	test_initial();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
