/*
AE_COMMUNITY.C

Arena Evolved: the community services' opt-in, the settings side (ae_community.h).
*/

#include "ae_community.h"
#include "ae_community_rules.h"
#include "platform.h"
#include "port_config.h"

#ifdef HALO_GAME_BROWSER

/* the state last logged (the threads that ask may race on it: at worst the line is written twice) */
static volatile int logged_state = -1;

int ae_community_state(void)
{
	return ae_community_state_of(config_string("network.community_services"));
}

int ae_community_services_on(void)
{
	int state = ae_community_state();

	if (state != logged_state)
	{
		logged_state = state;
		if (ae_community_on(state))
			platform_log("community services: on (network.community_services = \"yes\"): %s",
				config_string("network.browser_url"));
		else
			platform_log("community services: off (network.community_services = \"%s\"): nothing goes to %s",
				state == AE_COMMUNITY_NO ? "no" : "ask", config_string("network.browser_url"));
	}
	return ae_community_on(state);
}

int ae_community_write_answer(int use, int list_row_on)
{
	struct ae_community_writes writes;
	int written = 1;

	ae_community_answer(use, list_row_on, &writes);
	written &= config_write_boolean("network.list_hosted_games", writes.list_hosted_games) != 0;
	written &= config_write_boolean("network.report_joined_games", writes.report_joined_games) != 0;
	written &= config_write_boolean("network.report_events", writes.report_events) != 0;
	/* (last: the services go on only with the rows they gate already written) */
	written &= config_write("network.community_services", writes.services) != 0;
	platform_log("community services: %s (list hosted games %s, game stats %s, hosted stats %s)%s",
		use ? "USE COMMUNITY SERVICES" : "NOT NOW", writes.list_hosted_games ? "on" : "off",
		writes.report_joined_games ? "on" : "off", writes.report_events ? "on" : "off",
		written ? "" : "; config.toml could not be written");
	return written;
}

#else

int ae_community_state(void)
{
	return AE_COMMUNITY_ASK;
}

int ae_community_services_on(void)
{
	return 0;
}

int ae_community_write_answer(int use, int list_row_on)
{
	(void)use;
	(void)list_row_on;
	return 0;
}

#endif
