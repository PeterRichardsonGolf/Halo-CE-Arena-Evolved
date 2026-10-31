/* The community services' opt-in (ae_community_rules.c): the setting's states, who is asked (a new install, an
upgrade from 0.3.4, never a headless run or the Xbox's menus), what each answer writes, and the input guard */
#include <stdio.h>
#include <string.h>
#include "ae_community_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_community_context context(int state)
{
	struct ae_community_context c;

	c.state = state;
	c.pc_menus = 1;
	c.renderer = 1;
	c.headless = 0;
	return c;
}

static void states(void)
{
	CHECK(ae_community_state_of("yes") == AE_COMMUNITY_YES);
	CHECK(ae_community_state_of("YES") == AE_COMMUNITY_YES);
	CHECK(ae_community_state_of("no") == AE_COMMUNITY_NO);
	CHECK(ae_community_state_of("No") == AE_COMMUNITY_NO);
	CHECK(ae_community_state_of("ask") == AE_COMMUNITY_ASK);
	/* (a 0.3.4 config.toml has no line: the default, or nothing read at all) */
	CHECK(ae_community_state_of("") == AE_COMMUNITY_ASK);
	CHECK(ae_community_state_of(NULL) == AE_COMMUNITY_ASK);
	/* (a typo is not a yes) */
	CHECK(ae_community_state_of("yess") == AE_COMMUNITY_ASK);
	CHECK(ae_community_state_of("y") == AE_COMMUNITY_ASK);
	CHECK(ae_community_state_of("true") == AE_COMMUNITY_ASK);
	CHECK(ae_community_state_of("nope") == AE_COMMUNITY_ASK);

	/* only a yes uses them */
	CHECK(ae_community_on(AE_COMMUNITY_YES));
	CHECK(!ae_community_on(AE_COMMUNITY_ASK));
	CHECK(!ae_community_on(AE_COMMUNITY_NO));
}

static void who_is_asked(void)
{
	struct ae_community_context c;

	/* a first boot, and a 0.3.4 upgrade (no line in config.toml: "ask"), on the PC menus */
	c = context(ae_community_state_of(NULL));
	CHECK(ae_community_should_ask(&c));
	c = context(ae_community_state_of("ask"));
	CHECK(ae_community_should_ask(&c));
	/* answered: never again, either way */
	c = context(ae_community_state_of("yes"));
	CHECK(!ae_community_should_ask(&c));
	c = context(ae_community_state_of("no"));
	CHECK(!ae_community_should_ask(&c));
	/* the Xbox's menus: not asked (they stay off) */
	c = context(AE_COMMUNITY_ASK);
	c.pc_menus = 0;
	CHECK(!ae_community_should_ask(&c));
	/* nothing to draw with, or nobody there */
	c = context(AE_COMMUNITY_ASK);
	c.renderer = 0;
	CHECK(!ae_community_should_ask(&c));
	c = context(AE_COMMUNITY_ASK);
	c.headless = 1;
	CHECK(!ae_community_should_ask(&c));
	CHECK(!ae_community_should_ask(NULL));
}

static void answers(void)
{
	struct ae_community_writes w;

	/* USE with the row at rest (OFF): the services and both stats, no listing */
	memset(&w, 0, sizeof(w));
	ae_community_answer(1, 0, &w);
	CHECK(!strcmp(w.services, "yes"));
	CHECK(!w.list_hosted_games);
	CHECK(w.report_joined_games);
	CHECK(w.report_events);
	/* USE with the row turned ON: listing too */
	ae_community_answer(1, 1, &w);
	CHECK(!strcmp(w.services, "yes"));
	CHECK(w.list_hosted_games);
	CHECK(w.report_joined_games);
	CHECK(w.report_events);
	/* NOT NOW: everything off */
	ae_community_answer(0, 0, &w);
	CHECK(!strcmp(w.services, "no"));
	CHECK(!w.list_hosted_games);
	CHECK(!w.report_joined_games);
	CHECK(!w.report_events);
	/* NOT NOW with the row turned ON: still off (listing needs the list's server) */
	ae_community_answer(0, 1, &w);
	CHECK(!strcmp(w.services, "no"));
	CHECK(!w.list_hosted_games);
	CHECK(!w.report_joined_games);
	CHECK(!w.report_events);
	/* the answer is then never asked again */
	{
		struct ae_community_context c = context(ae_community_state_of(w.services));

		CHECK(!ae_community_should_ask(&c));
		ae_community_answer(1, 0, &w);
		c = context(ae_community_state_of(w.services));
		CHECK(!ae_community_should_ask(&c));
		CHECK(ae_community_on(c.state));
	}
}

static void guard(void)
{
	CHECK(!ae_community_input_open(1000, 1000));
	CHECK(!ae_community_input_open(1000, 1499));
	CHECK(ae_community_input_open(1000, 1500));
	CHECK(ae_community_input_open(1000, 60000));
	/* (the millisecond clock wrapping under it) */
	CHECK(!ae_community_input_open((unsigned long)-200, 100));
	CHECK(ae_community_input_open((unsigned long)-200, 300));
}

int main(void)
{
	states();
	who_is_asked();
	answers();
	guard();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_community_rules: all passed\n");
	return 0;
}
