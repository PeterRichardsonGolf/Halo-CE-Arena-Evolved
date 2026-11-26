#include <stdio.h>
#include <string.h>
#include "ae_split_join_rules.h"
#include "ae_seed_ext_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_split_join_facts ready(void)
{
	struct ae_split_join_facts facts;

	memset(&facts, 0, sizeof(facts));
	facts.multiplayer = 1;
	facts.option_on = 1;
	facts.pad = 1;
	facts.local_players = 1;
	facts.slots_free = 3;
	return facts;
}

int main(void)
{
	struct ae_split_join_facts facts = ready();
	struct ae_variant_ext ext;
	int reason;

	/* a game under way with the switch on, a free pad and room */
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_OK);
	/* the switch off refuses, with the host's words */
	facts.option_on = 0;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_OFF);
	CHECK(!strcmp(ae_split_join_reason_text(AE_SPLIT_JOIN_OFF), "THE HOST HAS TURNED OFF JOINING IN PROGRESS"));
	facts = ready();
	facts.multiplayer = 0;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_NOT_MULTIPLAYER);
	facts = ready(); facts.replay = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_REPLAY);
	facts = ready(); facts.postgame = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_POSTGAME);
	facts = ready(); facts.tournament = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_TOURNAMENT);
	facts = ready(); facts.vote = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_VOTE);
	facts = ready(); facts.pad = 0;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_NO_PAD);
	facts = ready(); facts.controller_plays = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_IN_GAME);
	facts = ready(); facts.local_players = 4;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_MAX_LOCAL);
	facts = ready(); facts.slots_free = 0;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_FULL);
	/* the switch is told before a tournament's, and every refusal has words */
	facts = ready(); facts.option_on = 0; facts.tournament = 1;
	CHECK(ae_split_join_check(&facts) == AE_SPLIT_JOIN_OFF);
	for (reason = 1; reason < AE_SPLIT_JOIN_REASONS; reason++)
		CHECK(ae_split_join_reason_text(reason)[0] != 0);
	CHECK(ae_split_join_reason_text(AE_SPLIT_JOIN_OK)[0] == 0);

	/* the host: a machine's first player is never refused here, a later one follows the switch */
	CHECK(ae_split_join_host_allows(0, 0, 0, -1));
	CHECK(!ae_split_join_host_allows(0, 0, 1, -1));
	CHECK(ae_split_join_host_allows(1, 0, 1, -1));
	CHECK(!ae_split_join_host_allows(1, 1, 2, -1));
	/* (the pads a machine joins with ask just after its first player: the same join, whatever the switch) */
	CHECK(ae_split_join_host_allows(0, 0, 1, 3000));
	CHECK(ae_split_join_host_allows(0, 1, 1, 3000));
	CHECK(!ae_split_join_host_allows(0, 0, 1, AE_SPLIT_JOIN_TOGETHER_MS));

	/* leaving: a player of a split screen, not the last */
	CHECK(ae_split_leave_allowed(1, 2));
	CHECK(!ae_split_leave_allowed(1, 1));
	CHECK(!ae_split_leave_allowed(0, 2));

	/* the controller taken: the hint when free, else the first free pad */
	CHECK(ae_split_join_controller(0x1, 0xF, -1) == 1);
	CHECK(ae_split_join_controller(0x1, 0xF, 2) == 2);
	CHECK(ae_split_join_controller(0x3, 0xF, 1) == 2);
	CHECK(ae_split_join_controller(0xF, 0xF, -1) == -1);
	CHECK(ae_split_join_controller(0x1, 0x1, -1) == -1);
	CHECK(ae_split_join_views_after(1, 1) == 2 && ae_split_join_views_after(2, 0) == 1 &&
		ae_split_join_views_after(4, 1) == 4 && ae_split_join_views_after(1, 0) == 1);

	/* each gametype family's default: the casual and big team sets on, comp, the drills and the rest off */
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL, &ext);
	CHECK(ext.split_join == AE_EXT_SPLIT_JOIN_ON);
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(AE_SEED_EXT_CASUAL | AE_SEED_EXT_CTF, &ext);
	CHECK(ext.split_join == AE_EXT_SPLIT_JOIN_ON);
	ae_variant_ext_default(&ext);
	ae_seed_ext_apply(0, &ext);
	CHECK(ext.split_join == AE_EXT_SPLIT_JOIN_OFF && ae_variant_ext_is_default(&ext));
	/* a stored value is kept by the sanitizer, one out of range is the default */
	ae_variant_ext_default(&ext);
	ext.split_join = AE_EXT_SPLIT_JOIN_ON;
	CHECK(ae_variant_ext_sanitize(&ext) && ext.split_join == AE_EXT_SPLIT_JOIN_ON);
	ext.split_join = 7;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.split_join == AE_EXT_SPLIT_JOIN_OFF);

	if (failures)
		return 1;
	printf("ae_split_join_rules_test: ok\n");
	return 0;
}
