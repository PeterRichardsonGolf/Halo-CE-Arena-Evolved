/* The party record of this PC (see ae_glue_party.h). */
#include <stdio.h>
#include <string.h>

#include "ae_glue_party.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "../src/ae_private_files.h"

void platform_log(char const *format, ...);

#define FILE_NAME "party_store.txt"

static struct
{
	struct ae_party party;          /* the real party: the local players first, built each time it is asked */
	struct ae_party shown;
	struct ae_party_store store, staged_store;
	char text[AE_STORE_TEXT];
	int read;
	struct ae_party_member bots[AE_PARTY_MAX];  /* the party's bots, in the order they joined */
	int bot_count;
	int staged, staged_source;
	struct ae_party staged_party;
} party;

static void store_read(void)
{
	long length;

	if (party.read)
		return;
	party.read = 1;
	ae_party_store_clear(&party.store);
	length = ae_private_file_read(FILE_NAME, party.text, sizeof(party.text));
	if (length > 0)
	{
		party.text[length < (long)sizeof(party.text) ? length : (long)sizeof(party.text) - 1] = 0;
		ae_party_store_read(&party.store, party.text);
	}
	platform_log("ae party: %s: %d people and bots remembered", FILE_NAME, party.store.count);
}

static void store_write(void)
{
	int length = ae_party_store_write(&party.store, party.text, (int)sizeof(party.text));

	if (length < 0 || !ae_private_file_write(FILE_NAME, party.text, length))
		platform_log("ae party: %s could not be written", FILE_NAME);
}

struct ae_party_store *ae_glue_party_store(void)
{
	store_read();
	return party.staged ? &party.staged_store : &party.store;
}

struct ae_party const *ae_glue_party(int *source)
{
	int local, players = ae_social_party_size(), at;
	char name[48];

	if (party.staged)
	{
		if (source)
			*source = party.staged_source;
		return &party.staged_party;
	}
	if (source)
		*source = AE_PARTY_SOURCE_OFFLINE;
	ae_party_clear(&party.shown);
	for (local = 0; local < players && local < AE_SOCIAL_LOCAL; local++)
	{
		struct ae_party_member member;

		memset(&member, 0, sizeof(member));
		if (!ae_social_profile_name((short)local, name, sizeof(name)))
			snprintf(name, sizeof(name), "PLAYER %d", local + 1);
		snprintf(member.name, sizeof(member.name), "%s", name);
		snprintf(member.id, sizeof(member.id), "local:%d", local);
		member.kind = AE_MEMBER_LOCAL;
		member.seats = 1;
		member.ping_ms = -1;
		ae_party_add(&party.shown, &member);
	}
	for (at = 0; at < party.bot_count; at++)
		ae_party_add(&party.shown, &party.bots[at]);
	return &party.shown;
}

int ae_glue_party_up_bot(char const *name, int tier)
{
	struct ae_party_record *record;
	struct ae_party_member member;

	store_read();
	if (!name || !name[0] || ae_social_party_size() + party.bot_count >= AE_PARTY_MAX)
		return 0;
	ae_party_bot(&member, name, tier);
	{
		int at;

		for (at = 0; at < party.bot_count; at++)
			if (!strcmp(party.bots[at].id, member.id))
				return 0;
	}
	record = ae_party_store_touch(&party.store, member.id, name, AE_MEMBER_BOT, tier);
	if (record)
		ae_party_store_member(record, &member);
	member.kind = AE_MEMBER_BOT;
	party.bots[party.bot_count++] = member;
	store_write();
	platform_log("ae party: %s (tier %d) joined the party (%d bots)", name, tier, party.bot_count);
	return 1;
}

int ae_glue_party_remove(char const *id)
{
	int at;

	for (at = 0; at < party.bot_count; at++)
		if (!strcmp(party.bots[at].id, id))
		{
			memmove(&party.bots[at], &party.bots[at + 1], sizeof(party.bots[0]) * (size_t)(party.bot_count - at - 1));
			party.bot_count--;
			platform_log("ae party: %s left the party", id);
			return 1;
		}
	return 0;
}

int ae_glue_party_bots(char names[][AE_PARTY_NAME], int *tiers, int max)
{
	int at;

	for (at = 0; at < party.bot_count && at < max; at++)
	{
		snprintf(names[at], AE_PARTY_NAME, "%s", party.bots[at].name);
		tiers[at] = party.bots[at].tier;
	}
	return at;
}

void ae_glue_party_credit(char const *name, int won)
{
	char id[AE_PARTY_ID];
	struct ae_party_record *record;
	int at;

	store_read();
	snprintf(id, sizeof(id), "bot:%s", name);
	ae_party_store_game(&party.store, id, 1, won);
	record = ae_party_store_find(&party.store, id);
	for (at = 0; record && at < party.bot_count; at++)
		if (!strcmp(party.bots[at].id, id))
		{
			party.bots[at].together_games = record->together_games;
			party.bots[at].together_wins = record->together_wins;
			party.bots[at].saved = record->saved;
		}
	store_write();
}

void ae_glue_party_note(char const *name, int tier, int together, int won)
{
	char id[AE_PARTY_ID];

	store_read();
	snprintf(id, sizeof(id), "bot:%s", name);
	if (!ae_party_store_touch(&party.store, id, name, AE_MEMBER_BOT, tier))
		return;
	/* (a teammate that is not in the party is only remembered; an opponent's game counts against) */
	if (!together)
		ae_party_store_game(&party.store, id, 0, won);
	store_write();
}

int ae_glue_party_staged(void)
{
	return party.staged;
}

/* a made-up member */
static void fake(struct ae_party *list, char const *id, char const *name, int kind, int state, int tier, int ping, int expires, int saved,
	int games, int wins)
{
	struct ae_party_member member;

	memset(&member, 0, sizeof(member));
	snprintf(member.id, sizeof(member.id), "%s", id);
	snprintf(member.name, sizeof(member.name), "%s", name);
	member.kind = kind;
	member.state = state;
	member.tier = tier;
	member.ping_ms = ping;
	member.expires_s = expires;
	member.saved = saved;
	member.seats = 1;
	member.together_games = games;
	member.together_wins = wins;
	member.against_games = games ? games / 2 : 0;
	ae_party_add(list, &member);
}

static void fake_recent(int online)
{
	struct ae_party_record *record;

	ae_party_store_clear(&party.staged_store);
	if (online)
	{
		record = ae_party_store_touch(&party.staged_store, "D4WN-0000-0001", "Dawnbreaker", AE_MEMBER_FRIEND, 0);
		record->together_games = 2;
		record = ae_party_store_touch(&party.staged_store, "M0SS-0000-0007", "Mossy7", AE_MEMBER_FRIEND, 0);
		record->against_games = 1;
	}
	else
	{
		record = ae_party_store_touch(&party.staged_store, "bot:RowdyRhino", "RowdyRhino", AE_MEMBER_BOT, 7);
		record->against_games = 3;
		record = ae_party_store_touch(&party.staged_store, "bot:RustyCedar", "RustyCedar", AE_MEMBER_BOT, 6);
		record->against_games = 2;
	}
}

int ae_glue_party_dev(char const *state, char *text, int size)
{
	struct ae_party *p = &party.staged_party;

	if (!strcmp(state, "clear"))
	{
		party.staged = 0;
		snprintf(text, (size_t)size, "social party: the real party again");
		return 1;
	}
	ae_party_clear(p);
	party.staged_source = AE_PARTY_SOURCE_OFFLINE;
	fake(p, "local:0", "PLAYER 1", AE_MEMBER_LOCAL, AE_PARTY_IN_LOBBY, 0, -1, 0, 0, 0, 0);
	if (!strcmp(state, "offline") || !strcmp(state, "toobig") || !strcmp(state, "full"))
	{
		fake(p, "bot:TheRhino", "TheRhino", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 7, -1, 0, 1, 6, 4);
		fake(p, "bot:xMirax", "xMirax", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 8, -1, 0, 0, 0, 0);
		if (!strcmp(state, "full") || !strcmp(state, "toobig"))
			fake(p, "bot:Nebulaxd", "Nebulaxd", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 6, -1, 0, 0, 1, 1);
		if (!strcmp(state, "toobig"))
		{
			fake(p, "bot:Boulder99", "Boulder99", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 5, -1, 0, 0, 2, 1);
			fake(p, "bot:DuskBlade", "DuskBlade", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 9, -1, 0, 0, 0, 0);
		}
		fake_recent(0);
	}
	else if (!strcmp(state, "online"))
	{
		party.staged_source = AE_PARTY_SOURCE_ONLINE;
		snprintf(p->member[0].id, sizeof(p->member[0].id), "K7QD-2MXA-9PWE");
		fake(p, "K4ST-0000-0044", "Kestrel44", AE_MEMBER_FRIEND, AE_PARTY_READY, 0, 38, 0, 1, 0, 0);
		fake(p, "H0LL-0000-0F0X", "Hollow_Fox", AE_MEMBER_FRIEND, AE_PARTY_INVITED, 0, -1, 292, 0, 0, 0);
		fake(p, "bot:TheRhino", "TheRhino", AE_MEMBER_BOT, AE_PARTY_IN_LOBBY, 7, -1, 0, 1, 6, 4);
		fake_recent(1);
	}
	else if (!strcmp(state, "empty"))
		fake_recent(0);
	else
	{
		snprintf(text, (size_t)size, "social party: offline|online|toobig|full|empty|clear");
		return 0;
	}
	party.staged = 1;
	snprintf(text, (size_t)size, "social party %s: staged (%d members)", state, p->count);
	return 1;
}
