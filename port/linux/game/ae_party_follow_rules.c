/*
AE_PARTY_FOLLOW_RULES.C

See ae_party_follow_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_party_follow_rules.h"

void ae_follow_reset(struct ae_follow_state *state)
{
	memset(state, 0, sizeof(*state));
}

void ae_follow_decide(const struct ae_pc_view *view, const char *me, int busy, struct ae_follow_state *state, struct ae_follow_action *out)
{
	memset(out, 0, sizeof(*out));
	if (!view->ok || view->same || !view->has_party || busy)
		return;
	if (view->search.present && view->search.n != state->seen_search)
	{
		if (!strcmp(view->search.state, "found") && view->search.server_address[0])
		{
			state->seen_search = view->search.n;
			out->kind = AE_FOLLOW_SERVER;
			snprintf(out->address, sizeof(out->address), "%s", view->search.server_address);
			return;
		}
		if (!strcmp(view->search.state, "bots"))
		{
			state->seen_search = view->search.n;
			if (!strcmp(view->leader, me))
			{
				out->kind = AE_FOLLOW_BOTS_OFFER;
				out->ask = view->search.ask;
				snprintf(out->reason, sizeof(out->reason), "%s", view->search.reason);
			}
			return;
		}
		if (!strcmp(view->search.state, "none"))
		{
			state->seen_search = view->search.n;
			out->kind = AE_FOLLOW_STOPPED;
			snprintf(out->reason, sizeof(out->reason), "%s", view->search.reason);
			return;
		}
	}
	if (view->follow.present && view->follow.n != state->seen_follow)
	{
		state->seen_follow = view->follow.n;
		/* (the leader's own game: it hosts, nobody follows it there) */
		if (!strcmp(view->leader, me))
			return;
		if (!strcmp(view->follow.kind, "hosted") && view->follow.invite[0])
		{
			out->kind = AE_FOLLOW_INVITE;
			snprintf(out->invite, sizeof(out->invite), "%s", view->follow.invite);
		}
		else if (!strcmp(view->follow.kind, "server") && view->follow.address[0])
		{
			out->kind = AE_FOLLOW_SERVER;
			snprintf(out->address, sizeof(out->address), "%s", view->follow.address);
		}
		else
		{
			out->kind = AE_FOLLOW_STOPPED;
			snprintf(out->reason, sizeof(out->reason), "%s", "not_followable");
		}
	}
}

int ae_follow_may_start(const struct ae_follow_state *state, unsigned now_ms)
{
	if (state->paused)
		return 0;
	return !state->started || now_ms - state->last_start_ms >= AE_FOLLOW_GAP_MS;
}

void ae_follow_started(struct ae_follow_state *state, unsigned now_ms)
{
	state->started = 1;
	state->last_start_ms = now_ms;
}

void ae_follow_failed(struct ae_follow_state *state, unsigned now_ms)
{
	int index;

	if (state->failed == AE_FOLLOW_FAILURES)
	{
		for (index = 1; index < AE_FOLLOW_FAILURES; index++)
			state->failed_ms[index - 1] = state->failed_ms[index];
		state->failed--;
	}
	state->failed_ms[state->failed++] = now_ms;
	if (state->failed == AE_FOLLOW_FAILURES && now_ms - state->failed_ms[0] <= AE_FOLLOW_FAILURE_WINDOW_MS)
		state->paused = 1;
}

void ae_follow_resume(struct ae_follow_state *state)
{
	state->paused = 0;
	state->failed = 0;
}

/* the dotted quad at text (four numbers of 1-3 digits, 0-255) and the port after its colon; 1 when "a.b.c.d:port" */
static int quad(const char *text, int *octet, long *port)
{
	const char *c = text;
	int index;

	for (index = 0; index < 4; index++)
	{
		int digits = 0, value = 0;

		while (*c >= '0' && *c <= '9' && digits < 4)
		{
			value = value * 10 + (*c++ - '0');
			digits++;
		}
		if (digits < 1 || digits > 3 || value > 255)
			return 0;
		octet[index] = value;
		if (index < 3 && *c++ != '.')
			return 0;
	}
	if (*c++ != ':' || *c < '0' || *c > '9')
		return 0;
	*port = 0;
	while (*c >= '0' && *c <= '9' && *port <= 65535)
		*port = *port * 10 + (*c++ - '0');
	return !*c && *port >= 1 && *port <= 65535;
}

int ae_follow_target_ok(const char *address, int service_is_loopback)
{
	int o[4];
	long port;

	if (!address || strlen(address) >= AE_PC_ADDRESS || !quad(address, o, &port))
		return 0;
	if (service_is_loopback)
		return 1;
	if (o[0] == 0 || o[0] == 10 || o[0] == 127 || o[0] >= 224)
		return 0;
	if ((o[0] == 100 && o[1] >= 64 && o[1] <= 127) || (o[0] == 169 && o[1] == 254) || (o[0] == 172 && o[1] >= 16 && o[1] <= 31) ||
		(o[0] == 192 && o[1] == 168))
		return 0;
	return 1;
}

int ae_follow_invite_ok(const char *invite)
{
	int index;

	if (!invite || strlen(invite) != 64)
		return 0;
	for (index = 0; index < 64; index++)
		if (!((invite[index] >= '0' && invite[index] <= '9') || (invite[index] >= 'a' && invite[index] <= 'f')))
			return 0;
	return 1;
}

int ae_follow_service_is_loopback(const char *origin)
{
	static const char *const local[] = { "http://127.0.0.1", "http://localhost", NULL };
	int index;

	for (index = 0; origin && local[index]; index++)
	{
		size_t length = strlen(local[index]);

		if (!strncmp(origin, local[index], length) && (!origin[length] || origin[length] == ':' || origin[length] == '/'))
			return 1;
	}
	return 0;
}

int ae_follow_member_left(const struct ae_follow_members *before, const struct ae_pc_view *view)
{
	int index, other;

	if (!view->ok || view->same || !view->has_party)
		return 0;
	for (index = 0; index < before->count; index++)
	{
		for (other = 0; other < view->member_count; other++)
			if (!strcmp(before->code[index], view->member[other].code))
				break;
		if (other == view->member_count)
			return 1;
	}
	return 0;
}

void ae_follow_members_note(struct ae_follow_members *before, const struct ae_pc_view *view)
{
	int index;

	before->count = 0;
	if (!view->ok || !view->has_party)
		return;
	for (index = 0; index < view->member_count && index < AE_PNET_MAX; index++)
		snprintf(before->code[before->count++], sizeof(before->code[0]), "%s", view->member[index].code);
}

int ae_follow_should_announce(const struct ae_pc_view *view, const char *me, const char *invite, const char *last)
{
	return view->ok && view->has_party && view->member_count >= 2 && !strcmp(view->leader, me) && invite && invite[0] &&
		strlen(invite) < AE_PC_ADDRESS && strcmp(invite, last ? last : "") != 0;
}

int ae_follow_playlist_known(const char *id)
{
	static const char *const known[] = { "casual", "btb", "comp", "classic", "community", NULL };
	int index;

	for (index = 0; id && known[index]; index++)
		if (!strcmp(id, known[index]))
			return 1;
	return 0;
}

int ae_follow_continue(const struct ae_pc_view *view, const char *me, const char *playlist, int *ask)
{
	*ask = 0;
	if (!view->ok || !view->has_party || view->member_count < 2 || !ae_follow_playlist_known(playlist))
		return 0;
	*ask = strcmp(view->leader, me) == 0;
	return 1;
}
