/*
AE_STEAM_FRIENDS_RULES.C

See ae_steam_friends_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_steam_friends_rules.h"
#include "mcc_sha256.h"

void ae_steam_hash(unsigned long long id, char out[AE_STEAM_HASH])
{
	static const char hex[] = "0123456789abcdef";
	struct mcc_sha256 context;
	unsigned char digest[32];
	char text[48];
	int index;

	snprintf(text, sizeof(text), "ae-steam-1%llu", id);
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, text, (int)strlen(text));
	mcc_sha256_end(&context, digest);
	for (index = 0; index < 6; index++)
	{
		out[2 * index] = hex[digest[index] >> 4];
		out[2 * index + 1] = hex[digest[index] & 15];
	}
	out[12] = 0;
}

int ae_steam_hashes_csv(const struct ae_steam_friend *friends, int count, int from, char *csv, int size)
{
	int index, written = 0, at = 0;

	if (size > 0)
		csv[0] = 0;
	for (index = from; index < count && written < AE_STEAM_CHUNK; index++)
	{
		char hash[AE_STEAM_HASH];

		if (at + 13 + (written ? 1 : 0) >= size)
			break;
		ae_steam_hash(friends[index].id, hash);
		if (written)
			csv[at++] = ',';
		memcpy(csv + at, hash, 12);
		at += 12;
		csv[at] = 0;
		written++;
	}
	return written;
}

int ae_steam_rows(const struct ae_steam_friend *friends, int count, const struct ae_pc_steam *found, int found_count,
	struct ae_steam_row *rows, int maximum, int *others)
{
	int index, each, made = 0;

	for (index = 0; index < count && index < AE_STEAM_FRIENDS_MAX; index++)
	{
		char hash[AE_STEAM_HASH];

		ae_steam_hash(friends[index].id, hash);
		for (each = 0; each < found_count; each++)
			if (!strcmp(found[each].hash, hash))
				break;
		if (each == found_count)
			continue;
		if (made < maximum)
		{
			snprintf(rows[made].steam_name, sizeof(rows[made].steam_name), "%s", friends[index].name);
			snprintf(rows[made].ae_name, sizeof(rows[made].ae_name), "%s", found[each].name);
			snprintf(rows[made].code, sizeof(rows[made].code), "%s", found[each].code);
			snprintf(rows[made].state, sizeof(rows[made].state), "%s", found[each].state);
			rows[made].verified = found[each].verified;
		}
		made++;
	}
	if (made > maximum)
		made = maximum;
	if (others)
		*others = (count < AE_STEAM_FRIENDS_MAX ? count : AE_STEAM_FRIENDS_MAX) - made;
	return made;
}

const char *ae_steam_state_word(const char *state, int in_party)
{
	if (in_party)
		return "IN YOUR PARTY";
	if (!strcmp(state, "searching"))
		return "IN MATCHMAKING";
	if (!strcmp(state, "party"))
		return "IN A PARTY";
	if (!strcmp(state, "game"))
		return "IN A GAME";
	return "IN THE MENUS";
}
