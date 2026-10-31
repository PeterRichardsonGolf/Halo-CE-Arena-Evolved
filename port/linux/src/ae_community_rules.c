/*
AE_COMMUNITY_RULES.C

Arena Evolved: the community services' opt-in, its rules (ae_community_rules.h).
*/

#include "ae_community_rules.h"

#include <ctype.h>

static int same_word(char const *value, char const *word)
{
	while (*value && *word)
	{
		if (tolower((unsigned char)*value) != *word)
			return 0;
		value++;
		word++;
	}
	return !*value && !*word;
}

int ae_community_state_of(char const *value)
{
	if (!value)
		return AE_COMMUNITY_ASK;
	if (same_word(value, "yes"))
		return AE_COMMUNITY_YES;
	if (same_word(value, "no"))
		return AE_COMMUNITY_NO;
	return AE_COMMUNITY_ASK;
}

int ae_community_on(int state)
{
	return state == AE_COMMUNITY_YES;
}

int ae_community_should_ask(struct ae_community_context const *context)
{
	return context && context->state == AE_COMMUNITY_ASK && context->pc_menus && context->renderer &&
		!context->headless;
}

void ae_community_answer(int use, int list_row_on, struct ae_community_writes *writes)
{
	writes->services = use ? "yes" : "no";
	writes->list_hosted_games = use && list_row_on;
	writes->report_joined_games = use != 0;
	writes->report_events = use != 0;
}

int ae_community_input_open(unsigned long opened_at, unsigned long now)
{
	return now - opened_at >= AE_COMMUNITY_INPUT_GUARD_MS;
}
