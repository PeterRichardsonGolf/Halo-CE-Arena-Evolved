/* Running a dedicated server from inside the game: the pure rules (see the header) */
#include <stdio.h>
#include <string.h>

#include "ae_server_ingame_rules.h"
#include "mcc_sha256.h"

/* ---------- roles and actions */

static const char *const commands[AE_INGAME_ACTIONS] =
{
	"sv_team", "mute", "kick", "ban", "sv_rotation_begin", "sv_end_game", "say", "sv_restart",
};

const char *ae_ingame_command(int action)
{
	return action >= 0 && action < AE_INGAME_ACTIONS ? commands[action] : "";
}

unsigned int ae_ingame_allowed(int role, ae_ingame_role_of role_of, void *context)
{
	unsigned int mask = 0;
	int action;

	if (role <= AE_INGAME_ROLE_NONE || !role_of)
		return 0;
	for (action = 0; action < AE_INGAME_ACTIONS; action++)
	{
		int needed = role_of(context, commands[action]);

		/* (a command the server does not have: -1) */
		if (needed < 0 || needed > role)
			continue;
		if (action == AE_INGAME_BAN && role < AE_INGAME_ROLE_ADMIN)
			continue;
		mask |= 1u << action;
	}
	return mask;
}

int ae_ingame_allows(unsigned int mask, int action)
{
	return action >= 0 && action < AE_INGAME_ACTIONS && (mask & (1u << action)) != 0;
}

const char *ae_ingame_role_word(int role)
{
	switch (role)
	{
	case AE_INGAME_ROLE_MODERATOR: return "MODERATOR";
	case AE_INGAME_ROLE_ADMIN: return "ADMIN";
	case AE_INGAME_ROLE_OWNER: return "OWNER";
	}
	return "";
}

/* ---------- the ban lengths */

static const int ban_minutes[AE_INGAME_BAN_LENGTHS] = { 60, 24 * 60, 7 * 24 * 60, 0 };
static const char *const ban_words[AE_INGAME_BAN_LENGTHS] = { "1 HOUR", "1 DAY", "1 WEEK", "FOREVER" };

int ae_ingame_ban_minutes(int length)
{
	return length >= 0 && length < AE_INGAME_BAN_LENGTHS ? ban_minutes[length] : ban_minutes[AE_INGAME_BAN_FIRST];
}

const char *ae_ingame_ban_word(int length)
{
	return length >= 0 && length < AE_INGAME_BAN_LENGTHS ? ban_words[length] : ban_words[AE_INGAME_BAN_FIRST];
}

static void upper(const char *text, char *out, int size)
{
	int index;

	for (index = 0; text && text[index] && index < size - 1; index++)
		out[index] = text[index] >= 'a' && text[index] <= 'z' ? (char)(text[index] - 'a' + 'A') : text[index];
	if (size > 0)
		out[index < size ? index : size - 1] = 0;
}

void ae_ingame_ban_yes(const char *name, int length, char *text, int size)
{
	char who[24];

	if (size <= 0)
		return;
	upper(name ? name : "", who, (int)sizeof(who));
	if (ae_ingame_ban_minutes(length) == 0)
		snprintf(text, (size_t)size, "BAN %s FOREVER", who);
	else
		snprintf(text, (size_t)size, "BAN %s FOR %s", who, ae_ingame_ban_word(length));
}

void ae_ingame_ban_command_length(int minutes, char *text, int size)
{
	if (size <= 0)
		return;
	if (minutes <= 0)
		snprintf(text, (size_t)size, "forever");
	else
		snprintf(text, (size_t)size, "%dm", minutes);
}

/* ---------- the player card's words */

const char *ae_ingame_move_label(int team_now)
{
	return team_now ? "MOVE TO RED TEAM" : "MOVE TO BLUE TEAM";
}

void ae_ingame_card_role_line(int role, char *text, int size)
{
	if (size <= 0)
		return;
	if (role <= AE_INGAME_ROLE_NONE || role > AE_INGAME_ROLE_OWNER)
	{
		text[0] = 0;
		return;
	}
	snprintf(text, (size_t)size, "You are %s %s on this server. The web page lists every kick and ban (its audit log).",
		role == AE_INGAME_ROLE_MODERATOR ? "a" : role == AE_INGAME_ROLE_ADMIN ? "an" : "the", ae_ingame_role_word(role));
}

/* ---------- admins.txt's key rows */

static int hex_value(char character)
{
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	return -1;
}

int ae_ingame_key_hex(const char *text, char *kept, int size)
{
	int index;

	if (!text || size <= AE_INGAME_KEY_HEX || strlen(text) != AE_INGAME_KEY_HEX)
		return 0;
	for (index = 0; index < AE_INGAME_KEY_HEX; index++)
	{
		int value = hex_value(text[index]);

		if (value < 0)
			return 0;
		kept[index] = "0123456789abcdef"[value];
	}
	kept[AE_INGAME_KEY_HEX] = 0;
	return 1;
}

int ae_ingame_key_bytes(const char *text, unsigned char *key)
{
	char kept[AE_INGAME_KEY_HEX + 1];
	int index;

	if (!ae_ingame_key_hex(text, kept, (int)sizeof(kept)))
		return 0;
	for (index = 0; index < AE_INGAME_KEY_HEX / 2; index++)
		key[index] = (unsigned char)(hex_value(kept[index * 2]) << 4 | hex_value(kept[index * 2 + 1]));
	return 1;
}

void ae_ingame_key_text(const unsigned char *key, char *text)
{
	int index;

	for (index = 0; index < AE_INGAME_KEY_HEX / 2; index++)
	{
		text[index * 2] = "0123456789abcdef"[key[index] >> 4];
		text[index * 2 + 1] = "0123456789abcdef"[key[index] & 15];
	}
	text[AE_INGAME_KEY_HEX] = 0;
}

/* ---------- text */

int ae_ingame_clean_text(const char *text, char *kept, int size)
{
	int length = 0, index;

	if (size <= 0)
		return 0;
	kept[0] = 0;
	if (!text)
		return 0;
	while (*text == ' ')
		text++;
	for (index = 0; text[index] && length < size - 1; index++)
	{
		unsigned char character = (unsigned char)text[index];

		if (character < 0x20 || character > 0x7E || character == '"' || character == '\\')
			continue;
		kept[length++] = (char)character;
	}
	while (length > 0 && kept[length - 1] == ' ')
		length--;
	kept[length] = 0;
	return length > 0;
}

/* ---------- the wire */

static void put_short(unsigned char *at, unsigned int value)
{
	at[0] = (unsigned char)(value & 0xFF);
	at[1] = (unsigned char)((value >> 8) & 0xFF);
}

static unsigned short get_short(const unsigned char *at)
{
	return (unsigned short)(at[0] | (at[1] << 8));
}

/* a text field: printable ASCII, ended inside it, zeros after */
static int text_ok(const char *text, int size)
{
	int index, ended = 0;

	for (index = 0; index < size; index++)
	{
		unsigned char character = (unsigned char)text[index];

		if (ended)
		{
			if (character)
				return 0;
			continue;
		}
		if (!character)
		{
			ended = 1;
			continue;
		}
		if (character < 0x20 || character > 0x7E)
			return 0;
	}
	return ended;
}

static void put_text(unsigned char *at, const char *text, int size)
{
	int index;

	memset(at, 0, (size_t)size);
	for (index = 0; text[index] && index < size - 1; index++)
	{
		unsigned char character = (unsigned char)text[index];

		at[index] = character >= 0x20 && character <= 0x7E ? character : '?';
	}
}

static int state_ok(const struct ae_ingame_wire *state)
{
	return state->role >= AE_INGAME_ROLE_MODERATOR && state->role <= AE_INGAME_ROLE_OWNER &&
		!(state->flags & ~AE_INGAME_WIRE_FLAGS) && !(state->allowed & ~((1u << AE_INGAME_ACTIONS) - 1)) &&
		state->count <= AE_INGAME_WIRE_ENTRIES && state->count <= state->total &&
		(state->now == AE_INGAME_WIRE_NO_ENTRY || state->now < state->total) &&
		(state->next == AE_INGAME_WIRE_NO_ENTRY || state->next < state->total);
}

int ae_ingame_wire_encode(const struct ae_ingame_wire *state, unsigned char *bytes, int size)
{
	int needed, index;
	unsigned char *at;

	if (!state || !state_ok(state))
		return 0;
	needed = AE_INGAME_WIRE_HEAD + state->count * AE_INGAME_WIRE_ENTRY;
	if (size < needed)
		return 0;
	memset(bytes, 0, (size_t)needed);
	bytes[0] = AE_INGAME_WIRE_VERSION;
	bytes[1] = state->role;
	bytes[2] = state->flags;
	put_short(bytes + 3, state->allowed);
	bytes[5] = state->players;
	bytes[6] = state->bots;
	bytes[7] = state->max_players;
	bytes[8] = state->count;
	bytes[9] = state->total;
	bytes[10] = state->now;
	bytes[11] = state->next;
	put_short(bytes + 12, state->up_minutes);
	put_short(bytes + 14, state->restart_seconds);
	put_text(bytes + 16, state->name, AE_INGAME_WIRE_NAME);
	at = bytes + AE_INGAME_WIRE_HEAD;
	for (index = 0; index < state->count; index++, at += AE_INGAME_WIRE_ENTRY)
	{
		put_text(at, state->entries[index].map, AE_INGAME_WIRE_MAP);
		put_text(at + AE_INGAME_WIRE_MAP, state->entries[index].gametype, AE_INGAME_WIRE_GAMETYPE);
		at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE] = state->entries[index].flags & AE_INGAME_ENTRY_REJECTED;
		at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE + 1] = state->entries[index].index;
	}
	return needed;
}

int ae_ingame_wire_decode(const unsigned char *bytes, int size, struct ae_ingame_wire *state)
{
	struct ae_ingame_wire read;
	const unsigned char *at;
	int index;

	if (!bytes || !state || size < AE_INGAME_WIRE_HEAD || bytes[0] != AE_INGAME_WIRE_VERSION)
		return 0;
	memset(&read, 0, sizeof(read));
	read.role = bytes[1];
	read.flags = bytes[2];
	read.allowed = get_short(bytes + 3);
	read.players = bytes[5];
	read.bots = bytes[6];
	read.max_players = bytes[7];
	read.count = bytes[8];
	read.total = bytes[9];
	read.now = bytes[10];
	read.next = bytes[11];
	read.up_minutes = get_short(bytes + 12);
	read.restart_seconds = get_short(bytes + 14);
	if (read.count > AE_INGAME_WIRE_ENTRIES || size != AE_INGAME_WIRE_HEAD + read.count * AE_INGAME_WIRE_ENTRY ||
		!text_ok((const char *)bytes + 16, AE_INGAME_WIRE_NAME))
	{
		return 0;
	}
	memcpy(read.name, bytes + 16, AE_INGAME_WIRE_NAME);
	at = bytes + AE_INGAME_WIRE_HEAD;
	for (index = 0; index < read.count; index++, at += AE_INGAME_WIRE_ENTRY)
	{
		if (!text_ok((const char *)at, AE_INGAME_WIRE_MAP) ||
			!text_ok((const char *)at + AE_INGAME_WIRE_MAP, AE_INGAME_WIRE_GAMETYPE) ||
			(at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE] & ~AE_INGAME_ENTRY_REJECTED) ||
			at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE + 1] >= read.total)
		{
			return 0;
		}
		memcpy(read.entries[index].map, at, AE_INGAME_WIRE_MAP);
		memcpy(read.entries[index].gametype, at + AE_INGAME_WIRE_MAP, AE_INGAME_WIRE_GAMETYPE);
		read.entries[index].flags = at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE];
		read.entries[index].index = at[AE_INGAME_WIRE_MAP + AE_INGAME_WIRE_GAMETYPE + 1];
	}
	if (!state_ok(&read))
		return 0;
	*state = read;
	return 1;
}

/* ---------- UP NEXT */

static int playable(const struct ae_ingame_wire *state, int entry)
{
	return entry >= 0 && entry < state->count && !(state->entries[entry].flags & AE_INGAME_ENTRY_REJECTED) &&
		state->entries[entry].index != state->now;
}

int ae_ingame_next_step(const struct ae_ingame_wire *state, int shown, int direction)
{
	int step = direction < 0 ? -1 : 1, at, tries;

	if (!state || !state->count)
		return shown;
	at = shown >= 0 && shown < state->count ? shown : 0;
	for (tries = 0; tries < state->count; tries++)
	{
		at = (at + step + state->count) % state->count;
		if (playable(state, at))
			return at;
	}
	/* (none other: the one played now, if it is in the list and can be) */
	for (at = 0; at < state->count; at++)
	{
		if (state->entries[at].index == state->now && !(state->entries[at].flags & AE_INGAME_ENTRY_REJECTED))
			return at;
	}
	return shown;
}

void ae_ingame_map_word(const char *map, char *text, int size)
{
	char copy[AE_INGAME_WIRE_MAP];
	char *at;
	int index;

	if (size <= 0)
		return;
	snprintf(copy, sizeof(copy), "%s", map ? map : "");
	/* (a pack's or Workshop's part: "beavercreek@pro", "pat_beavercreek@ws2899342285"; a folder: "custom_maps\x") */
	at = strchr(copy, '@');
	if (at)
		*at = 0;
	at = strrchr(copy, '\\');
	if (!at)
		at = strrchr(copy, '/');
	at = at ? at + 1 : copy;
	for (index = 0; at[index]; index++)
	{
		if (at[index] == '_')
			at[index] = ' ';
	}
	upper(at, text, size);
}

void ae_ingame_gametype_word(const char *gametype, char *text, int size)
{
	int index;

	if (size <= 0)
		return;
	upper(gametype ? gametype : "", text, size);
	for (index = 0; text[index]; index++)
	{
		if (text[index] == '_')
			text[index] = ' ';
	}
}

void ae_ingame_up_text(unsigned int minutes, char *text, int size)
{
	if (size <= 0)
		return;
	if (!minutes)
		snprintf(text, (size_t)size, "under a minute");
	else if (minutes < 60)
		snprintf(text, (size_t)size, "%u min", minutes);
	else if (minutes % 60)
		snprintf(text, (size_t)size, "%u h %u min", minutes / 60, minutes % 60);
	else
		snprintf(text, (size_t)size, "%u h", minutes / 60);
}

/* ---------- the sign-in offer (message 114) */

void ae_ingame_offer_hash(const unsigned char *salt, const unsigned char *key, unsigned char *hash)
{
	struct mcc_sha256 context;
	unsigned char digest[32];

	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, salt, AE_INGAME_OFFER_SALT);
	mcc_sha256_add(&context, key, 32);
	mcc_sha256_end(&context, digest);
	memcpy(hash, digest, AE_INGAME_OFFER_HASH);
}

int ae_ingame_offer_add(struct ae_ingame_offer *offer, const unsigned char *key)
{
	if (offer->count >= AE_INGAME_OFFER_HASHES)
		return 0;
	ae_ingame_offer_hash(offer->salt, key, offer->hashes[offer->count]);
	offer->count++;
	return 1;
}

int ae_ingame_offer_encode(const struct ae_ingame_offer *offer, unsigned char *bytes, int size)
{
	int length = AE_INGAME_OFFER_HEAD + offer->count * AE_INGAME_OFFER_HASH;

	if (offer->count > AE_INGAME_OFFER_HASHES || (offer->flags & ~AE_INGAME_OFFER_FLAGS) || size < length)
		return 0;
	bytes[0] = AE_INGAME_OFFER_VERSION;
	bytes[1] = offer->flags;
	bytes[2] = offer->count;
	bytes[3] = 0;
	memcpy(bytes + 4, offer->salt, AE_INGAME_OFFER_SALT);
	memcpy(bytes + AE_INGAME_OFFER_HEAD, offer->hashes, (size_t)offer->count * AE_INGAME_OFFER_HASH);
	return length;
}

int ae_ingame_offer_decode(const unsigned char *bytes, int size, struct ae_ingame_offer *offer)
{
	int count;

	memset(offer, 0, sizeof(*offer));
	if (size < AE_INGAME_OFFER_HEAD || bytes[0] != AE_INGAME_OFFER_VERSION || (bytes[1] & ~AE_INGAME_OFFER_FLAGS) ||
		bytes[3])
	{
		return 0;
	}
	count = bytes[2];
	if (count > AE_INGAME_OFFER_HASHES || size < AE_INGAME_OFFER_HEAD + count * AE_INGAME_OFFER_HASH)
		return 0;
	offer->flags = bytes[1];
	offer->count = (unsigned char)count;
	memcpy(offer->salt, bytes + 4, AE_INGAME_OFFER_SALT);
	memcpy(offer->hashes, bytes + AE_INGAME_OFFER_HEAD, (size_t)count * AE_INGAME_OFFER_HASH);
	return 1;
}

int ae_ingame_offer_matches(const struct ae_ingame_offer *offer, const unsigned char *own_key)
{
	unsigned char hash[AE_INGAME_OFFER_HASH];
	int index;

	if (offer->flags & AE_INGAME_OFFER_BY_ADDRESS)
		return 1;
	if (!own_key)
		return 0;
	ae_ingame_offer_hash(offer->salt, own_key, hash);
	for (index = 0; index < offer->count; index++)
	{
		if (!memcmp(offer->hashes[index], hash, sizeof(hash)))
			return 1;
	}
	return 0;
}

int ae_ingame_card_pick(int count, char const *const *names, char const *name)
{
	int index;

	if (count <= 0 || !names)
		return -1;
	if (!name || !name[0])
		return 0;
	for (index = 0; index < count; index++)
	{
		if (names[index] && !strcmp(names[index], name))
			return index;
	}
	return -1;
}
