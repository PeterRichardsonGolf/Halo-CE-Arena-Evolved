/*
AE_GLUE_STEAM_FRIENDS.C

See ae_glue_steam_friends.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_glue_steam_friends.h"
#include "../src/ae_party_client.h"
#include "../src/port_config.h"

void platform_log(const char *format, ...);

static struct
{
	unsigned long long own;
	int count;
	struct ae_steam_friend friends[AE_STEAM_FRIENDS_MAX];
} list;

void ae_steam_friends_provide(unsigned long long own_id, const struct ae_steam_friend *friends, int count)
{
	if (count > AE_STEAM_FRIENDS_MAX)
		count = AE_STEAM_FRIENDS_MAX;
	list.own = own_id;
	list.count = count < 0 ? 0 : count;
	if (list.count)
		memcpy(list.friends, friends, sizeof(list.friends[0]) * (size_t)list.count);
}

int ae_steam_friends_send(char *text, int size)
{
	char hash[AE_STEAM_HASH], csv[600];
	int from = 0, taken, calls = 0;

	if (!config_boolean("network.steam_friends"))
	{
		snprintf(text, (size_t)size, "network.steam_friends is off: nothing is sent");
		return 0;
	}
	if (!list.own)
	{
		snprintf(text, (size_t)size, "no Steam account (nothing signed in, and no made-up list)");
		return 0;
	}
	ae_steam_hash(list.own, hash);
	if (!ae_party_client_call("steam_register", "hash", hash, NULL))
	{
		snprintf(text, (size_t)size, "the party service is not started");
		return 0;
	}
	while ((taken = ae_steam_hashes_csv(list.friends, list.count, from, csv, sizeof(csv))) > 0)
	{
		if (!ae_party_client_call("steam_lookup", "hashes", csv, "reset", from ? "0" : "1", NULL))
			break;
		from += taken;
		calls++;
	}
	platform_log("ae steam friends: registered, asked about %d friends in %d calls", from, calls);
	snprintf(text, (size_t)size, "registered; asked about %d friends in %d calls", from, calls);
	return 1;
}

int ae_steam_friends_withdraw(char *text, int size)
{
	int ok = ae_party_client_call("steam_register", "hash", "", NULL);

	snprintf(text, (size_t)size, ok ? "Steam friends withdrawn" : "the party service is not started");
	platform_log("ae steam friends: withdrawn");
	return ok;
}

int ae_steam_friends_rows(struct ae_steam_row *rows, int maximum, int *others)
{
	struct ae_pc_view view;

	if (!ae_party_client_view(&view))
	{
		if (others)
			*others = list.count;
		return 0;
	}
	return ae_steam_rows(list.friends, list.count, view.steam, view.steam_count, rows, maximum, others);
}

void ae_steam_friends_summary(char *text, int size)
{
	struct ae_steam_row rows[AE_PC_STEAM];
	int others = 0, made = ae_steam_friends_rows(rows, AE_PC_STEAM, &others), index, at;

	at = snprintf(text, (size_t)size, "steam friends: %d playing AE, %d others", made, others);
	for (index = 0; index < made && at < size; index++)
		at += snprintf(text + at, (size_t)(size - at), " [%s = %s %s, %s]", rows[index].steam_name, rows[index].ae_name,
			rows[index].code, ae_steam_state_word(rows[index].state, 0));
}
