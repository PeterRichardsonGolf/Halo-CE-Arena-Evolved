/*
AE_PARTY_CLIENT_RULES.C

The party service's wire, game side (ae_party_client_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_party_client_rules.h"
#include "../src/server_json.h"

void ae_pc_hello_text(const char *origin, const char *nonce, const char *pub_hex, char *out, int size)
{
	snprintf(out, (size_t)size, "ae-party-hello-2\n%s\n%s\n%s", origin, nonce, pub_hex);
}

int ae_pc_origin(const char *base, char *out, int size)
{
	const char *authority = strstr(base, "://");
	int length, index, slash;

	if (!authority || authority == base)
		return 0;
	length = (int)strcspn(base, "?#");
	while (length > 0 && base[length - 1] == '/')
		length--;
	if (length >= size)
		return 0;
	/* (scheme and host are not case sensitive: the path is) */
	slash = (int)(authority - base) + 3;
	while (slash < length && base[slash] != '/')
		slash++;
	for (index = 0; index < length; index++)
	{
		char c = base[index];

		out[index] = index < slash && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
	}
	out[length] = 0;
	return length > (int)(authority - base) + 3;
}

int ae_pc_url(char *url, int size, const char *base, const char *op)
{
	int length = (int)strlen(base);

	while (length && base[length - 1] == '/')
		length--;
	return snprintf(url, (size_t)size, "%.*s/v1/%s", length, base, op) < size;
}

/* the length of the well-formed UTF-8 sequence at c, 0 when it is not one */
static int utf8_length(const unsigned char *c)
{
	int extra, index;

	if (*c < 0x80)
		return 1;
	if (*c >= 0xc2 && *c <= 0xdf)
		extra = 1;
	else if (*c >= 0xe0 && *c <= 0xef)
		extra = 2;
	else if (*c >= 0xf0 && *c <= 0xf4)
		extra = 3;
	else
		return 0;
	for (index = 1; index <= extra; index++)
		if ((c[index] & 0xc0) != 0x80)
			return 0;
	return extra + 1;
}

/* the text as a JSON string's inside, written at *at in body (room kept for the closing quote, brace and NUL); 1 when it fit */
static int json_text(char *body, int *at, int size, const char *text)
{
	static const char hex[] = "0123456789abcdef";
	const unsigned char *c = (const unsigned char *)text;

	while (*c)
	{
		int length = utf8_length(c);

		if (*at + 8 >= size)
			return 0;
		if (!length)
		{
			body[(*at)++] = '?';
			c++;
		}
		else if (length > 1)
		{
			while (length--)
				body[(*at)++] = (char)*c++;
		}
		else if (*c == '"' || *c == '\\')
		{
			body[(*at)++] = '\\';
			body[(*at)++] = (char)*c++;
		}
		else if (*c < 0x20 || *c == 0x7f)
		{
			body[(*at)++] = '\\';
			body[(*at)++] = 'u';
			body[(*at)++] = '0';
			body[(*at)++] = '0';
			body[(*at)++] = hex[*c >> 4];
			body[(*at)++] = hex[*c & 15];
			c++;
		}
		else
			body[(*at)++] = (char)*c++;
	}
	return 1;
}

int ae_pc_body_add(char *body, int size, const char *key, const char *value)
{
	int length = (int)strlen(body), at = length;

	if (size < 16 || length + 16 >= size || (length && body[length - 1] != '}'))
		return 0;
	if (!length)
		body[at++] = '{';
	else
		body[at - 1] = ',';
	body[at++] = '"';
	if (!json_text(body, &at, size, key))
		goto fail;
	if (at + 8 >= size)
		goto fail;
	body[at++] = '"';
	body[at++] = ':';
	body[at++] = '"';
	if (!json_text(body, &at, size, value ? value : ""))
		goto fail;
	body[at++] = '"';
	body[at++] = '}';
	body[at] = 0;
	return 1;
fail:
	if (length)
		body[length - 1] = '}';
	body[length] = 0;
	return 0;
}

typedef struct server_json_document const *doc_t;

static int text_of(doc_t d, int object, const char *key, char *out, int size)
{
	int token = object < 0 ? -1 : server_json_member(d, object, key);

	if (token < 0 || !server_json_get_string(d, token, out, size))
	{
		if (size)
			out[0] = 0;
		return 0;
	}
	return 1;
}

static long number_of(doc_t d, int object, const char *key, long fallback)
{
	int token = object < 0 ? -1 : server_json_member(d, object, key);
	double value;

	if (token < 0 || !server_json_get_number(d, token, &value))
		return fallback;
	/* (a number the cast to long cannot hold is not undefined here: the service's numbers are small) */
	return (long)(value > 1e9 ? 1e9 : value < -1e9 ? -1e9 : value);
}

/* a seat count the service sent, held to what a party can have (a member holds 1-4 seats, a party at most AE_PNET_MAX) */
static int clamp_seats(int seats, int low, int high)
{
	return seats < low ? low : seats > high ? high : seats;
}

static int flag_of(doc_t d, int object, const char *key)
{
	int token = object < 0 ? -1 : server_json_member(d, object, key), value = 0;

	return token >= 0 && server_json_get_boolean(d, token, &value) ? value : 0;
}

static int object_of(doc_t d, int object, const char *key)
{
	int token = object < 0 ? -1 : server_json_member(d, object, key);

	return token >= 0 && d->tokens[token].type == _server_json_object ? token : -1;
}

static void read_search(doc_t d, int s, struct ae_pc_search *out)
{
	int server;

	memset(out, 0, sizeof(*out));
	out->ping_ms = -1;
	if (s < 0)
		return;
	out->present = 1;
	text_of(d, s, "state", out->state, sizeof(out->state));
	text_of(d, s, "playlist", out->playlist, sizeof(out->playlist));
	text_of(d, s, "ticket", out->ticket, sizeof(out->ticket));
	text_of(d, s, "reason", out->reason, sizeof(out->reason));
	out->n = number_of(d, s, "n", 0);
	out->seats = (int)number_of(d, s, "seats", 0);
	out->ask = flag_of(d, s, "ask");
	out->again = flag_of(d, s, "again");
	out->room_for = (int)number_of(d, s, "room_for", 0);
	server = object_of(d, s, "server");
	if (server >= 0)
	{
		text_of(d, server, "address", out->server_address, sizeof(out->server_address));
		text_of(d, server, "name", out->server_name, sizeof(out->server_name));
		out->server_players = (int)number_of(d, server, "players", 0);
		out->server_max = (int)number_of(d, server, "max", 0);
		out->ping_ms = (int)number_of(d, server, "ping_ms", -1);
	}
}

int ae_pc_view_parse(const char *json, int length, struct ae_pc_view *out)
{
	struct server_json_document document;
	int root = 0, token, index;

	memset(out, 0, sizeof(*out));
	out->search.ping_ms = -1;
	if (!json || length <= 0 || !server_json_parse(&document, json, length, 12))
		return 0;
	if (document.count < 1 || document.tokens[0].type != _server_json_object)
	{
		server_json_document_free(&document);
		return 0;
	}
	/* (an error counts whatever its length: one too long for the field is still a refusal) */
	out->ok = server_json_member(&document, root, "error") < 0;
	if (!out->ok && !text_of(&document, root, "error", out->error, sizeof(out->error)))
		snprintf(out->error, sizeof(out->error), "%s", "error");
	text_of(&document, root, "detail", out->detail, sizeof(out->detail));
	out->room = (int)number_of(&document, root, "room", 0);
	out->same = flag_of(&document, root, "same");
	out->rev = number_of(&document, root, "rev", 0);
	text_of(&document, root, "code", out->code, sizeof(out->code));
	text_of(&document, root, "sid", out->sid, sizeof(out->sid));
	text_of(&document, root, "nonce", out->nonce, sizeof(out->nonce));
	token = object_of(&document, root, "config");
	out->poll_ms = (int)number_of(&document, token, "poll_ms", 0);
	token = object_of(&document, root, "notice");
	if (token >= 0)
		text_of(&document, token, "text", out->notice, sizeof(out->notice));

	token = object_of(&document, root, "party");
	if (token >= 0)
	{
		int members = server_json_member(&document, token, "members");
		char word[AE_PC_WORD];

		out->has_party = 1;
		text_of(&document, token, "id", out->party_id, sizeof(out->party_id));
		text_of(&document, token, "leader", out->leader, sizeof(out->leader));
		text_of(&document, token, "link", out->link, sizeof(out->link));
		text_of(&document, token, "privacy", word, sizeof(word));
		out->privacy = ae_pnet_privacy_parse(word);
		if (out->privacy < 0)
			out->privacy = AE_PNET_INVITE_ONLY;
		out->seats = clamp_seats((int)number_of(&document, token, "seats", 0), 0, AE_PNET_MAX);
		for (index = 0; members >= 0 && index < AE_PNET_MAX; index++)
		{
			int member = server_json_element(&document, members, index);

			if (member < 0)
				break;
			text_of(&document, member, "code", out->member[index].code, sizeof(out->member[index].code));
			text_of(&document, member, "name", out->member[index].name, sizeof(out->member[index].name));
			text_of(&document, member, "state", out->member[index].state, sizeof(out->member[index].state));
			out->member[index].seats = clamp_seats((int)number_of(&document, member, "seats", 1), 1, 4);
			out->member_count = index + 1;
		}
	}
	token = server_json_member(&document, root, "invites");
	for (index = 0; token >= 0 && index < AE_PC_INVITES; index++)
	{
		int invite = server_json_element(&document, token, index), from;

		if (invite < 0)
			break;
		text_of(&document, invite, "party", out->invite[index].party, sizeof(out->invite[index].party));
		from = object_of(&document, invite, "from");
		text_of(&document, from, "code", out->invite[index].from_code, sizeof(out->invite[index].from_code));
		text_of(&document, from, "name", out->invite[index].from_name, sizeof(out->invite[index].from_name));
		out->invite[index].seats = clamp_seats((int)number_of(&document, invite, "seats", 0), 0, AE_PNET_MAX);
		out->invite[index].expires_s = (int)number_of(&document, invite, "expires_s", 0);
		out->invite_count = index + 1;
	}
	token = server_json_member(&document, root, "steam");
	for (index = 0; token >= 0 && index < AE_PC_STEAM; index++)
	{
		int friend = server_json_element(&document, token, index);

		if (friend < 0)
			break;
		text_of(&document, friend, "hash", out->steam[index].hash, sizeof(out->steam[index].hash));
		text_of(&document, friend, "code", out->steam[index].code, sizeof(out->steam[index].code));
		text_of(&document, friend, "name", out->steam[index].name, sizeof(out->steam[index].name));
		text_of(&document, friend, "state", out->steam[index].state, sizeof(out->steam[index].state));
		out->steam[index].verified = flag_of(&document, friend, "verified");
		out->steam_count = index + 1;
	}
	read_search(&document, object_of(&document, root, "search"), &out->search);
	token = object_of(&document, root, "follow");
	if (token >= 0)
	{
		out->follow.present = 1;
		text_of(&document, token, "kind", out->follow.kind, sizeof(out->follow.kind));
		text_of(&document, token, "address", out->follow.address, sizeof(out->follow.address));
		text_of(&document, token, "invite", out->follow.invite, sizeof(out->follow.invite));
		out->follow.n = number_of(&document, token, "n", 0);
	}
	server_json_document_free(&document);
	return 1;
}

int ae_pc_poll_ms(int service_ms, int active)
{
	int ms = service_ms < AE_PC_POLL_MIN ? AE_PC_POLL_MIN : service_ms > AE_PC_POLL_MAX ? AE_PC_POLL_MAX : service_ms;

	return active || ms >= AE_PC_POLL_IDLE ? ms : AE_PC_POLL_IDLE;
}

void ae_pc_party(const struct ae_pc_view *view, struct ae_pnet *party)
{
	int index;

	ae_pnet_clear(party);
	if (!view->has_party)
		return;
	for (index = 0; index < view->member_count && index < AE_PNET_MAX; index++)
	{
		struct ae_pnet_member *member = &party->member[index];

		snprintf(member->code, sizeof(member->code), "%s", view->member[index].code);
		snprintf(member->name, sizeof(member->name), "%s", view->member[index].name);
		member->seats = view->member[index].seats;
		member->online = strcmp(view->member[index].state, "offline") != 0;
		if (!strcmp(view->member[index].code, view->leader))
			party->leader = index;
	}
	party->count = view->member_count;
	party->privacy = view->privacy;
}

void ae_pc_view_summary(const struct ae_pc_view *view, char *text, int size)
{
	if (!view->ok)
		snprintf(text, (size_t)size, "refused %s%s%s", view->error, view->detail[0] ? ": " : "", view->detail);
	else if (view->same)
		snprintf(text, (size_t)size, "unchanged rev %ld", view->rev);
	else
		snprintf(text, (size_t)size, "rev %ld, %s, %d invite%s%s%s", view->rev,
			view->has_party ? "in a party" : "no party", view->invite_count, view->invite_count == 1 ? "" : "s",
			view->search.present ? ", search " : "", view->search.present ? view->search.state : "");
}
