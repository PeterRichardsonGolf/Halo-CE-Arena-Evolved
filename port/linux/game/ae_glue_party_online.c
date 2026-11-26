/*
AE_GLUE_PARTY_ONLINE.C

See ae_glue_party_online.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_glue_party_online.h"
#include "ae_dev_cmd_rules.h"
#include "ae_party_follow.h"
#include "ae_glue_steam_friends.h"
#include "../src/ae_party_client.h"

/* a decimal number (unsigned, 64-bit) and where it ends; the game's files build as C89 on macOS, where <stdlib.h> declares
no strtoull */
static unsigned long long read_decimal(const char *at, const char **end)
{
	unsigned long long value = 0;

	while (*at >= '0' && *at <= '9')
		value = value * 10 + (unsigned long long)(*at++ - '0');
	if (end)
		*end = at;
	return value;
}

static int is(const char *word, const char *name)
{
	for (; *word && *name; word++, name++)
		if ((*word >= 'A' && *word <= 'Z' ? *word + 32 : *word) != *name)
			return 0;
	return !*word && !*name;
}

static int status_text(char *text, int size)
{
	static const char *const states[] = { "off", "connecting", "up", "retrying" };
	struct ae_pc_view view;
	char code[AE_FRIEND_CODE_SIZE], last[160];
	int at, index;

	char follow[100];
	int have_view;

	memset(&view, 0, sizeof(view));
	ae_party_client_code(code, sizeof(code));
	ae_party_client_last(last, sizeof(last));
	ae_party_follow_status(follow, sizeof(follow));
	at = snprintf(text, (size_t)size, "party service %s, code %s", ae_party_client_enabled() ? states[ae_party_client_state()] : "not set (network.party_service)", code);
	have_view = ae_party_client_view(&view);
	if (!have_view)
		memset(&view, 0, sizeof(view));
	if (have_view && view.has_party && at < size)
	{
		at += snprintf(text + at, (size_t)(size - at), "; party %d/%d seats, leader %.4s, %s", view.seats, AE_PNET_MAX, view.leader,
			ae_pnet_privacy_label(view.privacy));
		for (index = 0; index < view.member_count && at < size; index++)
			at += snprintf(text + at, (size_t)(size - at), " [%s %s]", view.member[index].name, view.member[index].state);
	}
	else if (at < size)
		at += snprintf(text + at, (size_t)(size - at), have_view ? "; no party" : "; no view");
	if (view.invite_count && at < size)
		at += snprintf(text + at, (size_t)(size - at), "; %d invite%s", view.invite_count, view.invite_count == 1 ? "" : "s");
	if (view.search.present && at < size)
		at += snprintf(text + at, (size_t)(size - at), "; search %s %s%s%s", view.search.state, view.search.playlist,
			view.search.server_address[0] ? " at " : "", view.search.server_address);
	if (at < size)
		at += snprintf(text + at, (size_t)(size - at), "; follow: %s", follow);
	if (last[0] && at < size)
		snprintf(text + at, (size_t)(size - at), "; last: %s", last);
	return 1;
}

/* a call queued: the answer comes with `party online status` (last:) */
static int queued(int ok, const char *op, char *text, int size)
{
	snprintf(text, (size_t)size, ok ? "%s queued: `party online status` shows its answer" : "%s not queued (the service is not started: `party online start`)", op);
	return ok;
}

int ae_party_online_dev(struct ae_dev_cmd const *cmd, char *text, int size)
{
	const char *verb, *a1 = cmd->words > 2 ? cmd->word[2] : "";

	if (cmd->words >= 1 && is(cmd->word[0], "code"))
	{
		char code[AE_FRIEND_CODE_SIZE];

		if (!ae_party_client_code(code, sizeof(code)))
		{
			snprintf(text, (size_t)size, "no party key could be made");
			return 0;
		}
		snprintf(text, (size_t)size, "friend code %s", code);
		return 1;
	}
	if (cmd->words < 1 || !is(cmd->word[0], "online"))
	{
		snprintf(text, (size_t)size, "party code | party online <status|start|stop|name|invite|answer|leave|kick|promote|privacy|link|newlink|seats|follow|quickplay|cancel>");
		return 0;
	}
	verb = cmd->words > 1 ? cmd->word[1] : "status";
	/* (any party control the player uses ends an automatic follow's pause) */
	if (!is(verb, "status"))
		ae_party_follow_resume();
	if (is(verb, "status"))
		return status_text(text, size);
	if (is(verb, "start"))
	{
		int ok = ae_party_client_start();

		snprintf(text, (size_t)size, ok ? "party service starting" : "party service off (network.party_service is empty, or no player key)");
		return ok;
	}
	if (is(verb, "stop"))
	{
		ae_party_client_stop();
		snprintf(text, (size_t)size, "party service stopping");
		return 1;
	}
	if (is(verb, "steam"))
	{
		/* party online steam fake <own id> [<friend id>[=<name>],...] | send | list | off */
		const char *what = a1;

		if (is(what, "fake") && cmd->words > 3)
		{
			struct ae_steam_friend friends[AE_STEAM_FRIENDS_MAX];
			const char *at = cmd->words > 4 ? cmd->word[4] : "";
			int count = 0;

			while (*at && count < AE_STEAM_FRIENDS_MAX)
			{
				const char *end;
				unsigned long long id = read_decimal(at, &end);

				if (end == at)
					break;
				friends[count].id = id;
				snprintf(friends[count].name, sizeof(friends[count].name), "Friend %d", count + 1);
				count++;
				at = *end == ',' ? end + 1 : end;
			}
			ae_steam_friends_provide(read_decimal(cmd->word[3], NULL), friends, count);
			snprintf(text, (size_t)size, "made-up Steam list: account %s, %d friends", cmd->word[3], count);
			return 1;
		}
		if (is(what, "send"))
			return ae_steam_friends_send(text, size);
		if (is(what, "off"))
			return ae_steam_friends_withdraw(text, size);
		if (is(what, "list"))
		{
			ae_steam_friends_summary(text, size);
			return 1;
		}
		snprintf(text, (size_t)size, "party online steam fake <own id> [<id>,<id>...] | send | list | off");
		return 0;
	}
	if (is(verb, "autojoin"))
	{
		ae_party_client_dev_autojoin(!(cmd->words > 2 && is(a1, "off")));
		snprintf(text, (size_t)size, "autojoin %s: the first invite is answered yes at once", cmd->words > 2 && is(a1, "off") ? "off" : "on");
		return 1;
	}
	if (is(verb, "name"))
	{
		ae_party_client_identity(a1, 0);
		snprintf(text, (size_t)size, "name %s", a1);
		return 1;
	}
	if (is(verb, "seats") && cmd->words > 2)
	{
		int seats = atoi(a1);

		ae_party_client_identity(NULL, seats);
		return queued(ae_party_client_call("party_seats", "seats", a1, NULL), "party_seats", text, size);
	}
	if (is(verb, "invite") && cmd->words > 2)
		return queued(ae_party_client_call("party_invite", "to", a1, NULL), "party_invite", text, size);
	if (is(verb, "answer") && cmd->words > 2)
	{
		struct ae_pc_view view;
		int index = atoi(a1) - 1;
		int join = cmd->words < 4 || !is(cmd->word[3], "no");

		if (!ae_party_client_view(&view) || index < 0 || index >= view.invite_count)
		{
			snprintf(text, (size_t)size, "no invite %d (`party online status` counts them)", index + 1);
			return 0;
		}
		return queued(ae_party_client_call("party_answer", "party", view.invite[index].party, "join", join ? "1" : "0", NULL),
			"party_answer", text, size);
	}
	if (is(verb, "leave"))
		return queued(ae_party_client_call("party_leave", NULL), "party_leave", text, size);
	if (is(verb, "kick") && cmd->words > 2)
		return queued(ae_party_client_call("party_kick", "code", a1, NULL), "party_kick", text, size);
	if (is(verb, "promote") && cmd->words > 2)
		return queued(ae_party_client_call("party_promote", "code", a1, NULL), "party_promote", text, size);
	if (is(verb, "privacy") && cmd->words > 2)
		return queued(ae_party_client_call("party_privacy", "level", a1, NULL), "party_privacy", text, size);
	if (is(verb, "link") && cmd->words > 2)
		return queued(ae_party_client_call("party_link", "link", a1, NULL), "party_link", text, size);
	if (is(verb, "newlink"))
		return queued(ae_party_client_call("party_new_link", NULL), "party_new_link", text, size);
	if (is(verb, "follow") && cmd->words > 3)
		return queued(ae_party_client_call("party_follow", "kind", a1, is(a1, "hosted") ? "invite" : "address", cmd->word[3], NULL),
			"party_follow", text, size);
	if (is(verb, "quickplay") && cmd->words > 2)
	{
		/* party online quickplay <playlist> [<address>=<ms>,...] [again] */
		const char *pings = cmd->words > 3 && !is(cmd->word[3], "again") ? cmd->word[3] : "";
		int again = cmd->words > 3 && is(cmd->word[cmd->words - 1], "again");

		return queued(again ? ae_party_client_call("quick_play", "playlist", a1, "pings", pings, "again", "1", NULL) :
			ae_party_client_call("quick_play", "playlist", a1, "pings", pings, NULL), "quick_play", text, size);
	}
	if (is(verb, "cancel"))
		return queued(ae_party_client_call("quick_cancel", NULL), "quick_cancel", text, size);
	snprintf(text, (size_t)size, "party online <status|start|stop|steam ...|autojoin [off]|name <text>|invite <code>|answer <n> [no]|leave|kick <code>|promote <code>|privacy <invite|friends|closed>|link <text>|newlink|seats <1-4>|follow server|hosted|lan <address>|quickplay <playlist> [<address>=<ms>,...] [again]|cancel>");
	return 0;
}
